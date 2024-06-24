#ifdef __linux__
    #define _GNU_SOURCE    /* For Linux's fallocate(). */
    #define HAVE_FALLOCATE 1
#endif  /* __linux__ */

#undef POSIX_C_SOURCE
#undef _XOPEN_SOURCE

#define _POSIX_C_SOURCE 2008'19L
#define _XOPEN_SOURCE   700

#include "copyfile_unix.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* On POSIX systems on which fdatasync() is available, _POSIX_SYNCHRONIZED_IO
 * is defined in <unistd.h> to a value greater than 0. */
#if defined(_POSIX_SYNCHRONIZED_IO) && _POSIX_SYNCHRONIZED_IO > 0
    #define HAVE_FDATASYNC 1
#endif /* defined(_POSIX_SYNCHRONIZED_IO) && _POSIX_SYNCHRONIZED_IO > 0 */

#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_LLVM_COMPILER)
    #define unlikely(EXPR)  __builtin_expect(!!(EXPR), 0)
    #define likely(EXPR)    __builtin_expect(!!(EXPR), 1)
#endif /* defined(__GNUC__) || defined(__clang__) || defined(__INTEL_LLVM_COMPILER) */

[[gnu::always_inline]] static inline bool is_fd_valid(int fd)
{
    /* F_GETFD is cheaper in principle since it only dereferences the
     * (process-local) file descriptor in kernel space, not the underlying
     * open file description (process-shared) which it refers to. 
     *
     * In particular, the specification suggests that it cannot be interrupted
     * by signals, nor is it affected by any sort of lock held anywhere.
     *
     * See: https://stackoverflow.com/a/12340725/20017547 and cboard.cprogramming.com/networking-device-communication/93683-checking-if-file-descriptor-valid.html#post678998 */
    return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

[[gnu::always_inline]] static inline bool is_mode_regular_file(mode_t m)
{
    return S_ISREG(m);
}

[[gnu::always_inline]] static inline bool is_mode_symlink(mode_t m)
{
    return S_ISLNK(m);
}

[[gnu::nonnull, gnu::always_inline]] static bool is_equivalent_stat(
        struct stat st1[restrict static 1],
        struct stat st2[restrict static 1])
{
    /* According to the POSIX stat specification, "The st_ino and st_dev fields
     * taken together uniquely identify the file wihin the system." */
    return st1->st_dev == st2->st_dev && st1->st_ino == st2->st_ino;
}

/**
 * Flushes buffered data and attributes written to the file to permanent storage. */
static int fsync_eintr(int fd)
{
    int ret;

    do {
#if defined(__APPLE__) && defined(__MACH__) && defined(F_FULLSYNC)
        /* Mac OS X does not flush data to physical storage with fsync(). 
         * See: https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fsync.2.html */
        ret = fcntl(fd, F_FULLSYNC);
#else
        ret = fsync(fd);
#endif /* defined(__APPLE__) && defined(__MACH__) && defined(F_FULLSYNC) */
    } while (unlikely(ret == -1) && ret == EINTR);

    return ret;
}

/**
 * Closes a file descriptor and returns the result, similar to close(). Unlike
 * close(), guarantees that the file descriptor is closed even if EINTR error
 * happens.
 *
 * Note: Some systems do not close the file descriptor in case the thread is
 * interrupted by a signal and close() returns EINTR. Other (most) systems do
 * close the file descriptor even when close() returns EINTR, and attempting to
 * close it again could close a different file descriptor that was opened by a
 * different thread. This function hides this difference in behavior. */
static int close_eintr(int fd)
{
/* HP-UX does not close the file descriptor when interrupted by a signal. 
 * See: https://www.unix.com/man-page/hpux/2/close */
#if defined(__hpux)
    int ret;

    do {
        ret = close(fd);
    } while (unlikely(ret == -1) && errno == EINTR);

    return ret;
#else
    return close(fd);
#endif  /* defined(__hpux) */
}

static ssize_t read_eintr(int fd, void *buf, size_t size)
{
    ssize_t ret = 0;

    do {
        ret = read(fd, buf, size);
    } while (unlikely(ret == -1) && errno == EINTR);

    return ret;
}

static ssize_t write_eintr(int fd, const void *buf, size_t size)
{
    ssize_t ret = 0;

    do {
        ret = write(fd, buf, size);
    } while (unlikely(ret == -1) && errno == EINTR);

    return ret;
}

static ssize_t write_all(int fd, const void *buf, size_t size)
{
    size_t wcount = 0;

    while (wcount < size) {
        ssize_t ret = write_eintr(fd, (char *) buf + wcount, size - wcount);

        if (unlikely(ret == -1)) {
            return -1;
        }
        
        wcount += (size_t) ret;
    }

    return (ssize_t) wcount;
}

/**
 * Flushes buffered data written to the file to permanent storage. */
static int fdatasync_eintr(int fd)
{
#if defined(HAVE_FDATASYNC) && !(defined(__APPLE__) && defined(__DARWIN__) && defined(F_FULLSYNC))
    int ret;

    do {
        ret = fdatasync(fd);
    } while (ret == -1 && errno == EINTR);

    return ret;
#else
    return fsync_eintr(fd);
#endif  /* defined(HAVE_FDATASYNC) && !(defined(__APPLE__) && defined(__DARWIN__) && defined(F_FULLSYNC) */
}

[[gnu::always_inline]] static inline bool set_file_perms(int fd, mode_t m)
{
    return fchmod(fd, m) != -1;
}

bool unix_fcopy_file(int src_fd, int dest_fd, unsigned char options)
{
    /* The behavior of C++'s filesystem::copy_file is undefined if there is more
     * than one option in any of options option group present in the valid
     * option groups, and perhaps Boost too. We define it and return false. */
    if (((options & UNIX_SKIP_EXISTING) && (options & UNIX_OVERWRITE_EXISTING))
        || ((options & UNIX_SYNCHRONIZE) && (options & UNIX_SYNCHRONIZE_DATA))) {
        return false;
    }

    if (!is_fd_valid(src_fd) || !is_fd_valid(dest_fd)) {
        return false;
    }

    if (options & UNIX_SKIP_EXISTING) {
        /* Do nothing. */
        return false;
    }

    struct stat src_st;

    /* If source file does not exist or is not a regular file or symlink, fail. */
    if (unlikely(fstat(src_fd, &src_st) == -1)
        || !(is_mode_regular_file(src_st.st_mode) || is_mode_symlink(src_st.st_mode))) {
        return false;
    }

    struct stat dest_st;

    if (unlikely(fstat(dest_fd, &dest_st) == -1)
        || !(is_mode_regular_file(dest_st.st_mode) || is_mode_symlink(dest_st.st_mode))
        || unlikely(is_equivalent_stat(&src_st, &dest_st))
        || unlikely(!set_file_perms(dest_fd, src_st.st_mode))) {
        return false;
    }

    posix_fadvise(src_fd, 0, 0, POSIX_FADV_SEQUENTIAL);

    /* Save the original seek position of both src_fd and dest_fd. We shall seek
     * to restore them before returning in either failure or success case. 
     *
     * Do not check for errors as none from EBADF, EINVAL, ENXIO, EOVERFLOW,
     * or ESPIPE is possible. */
    const off_t src_orig_pos = lseek(src_fd, 0, SEEK_CUR);
    const off_t dest_orig_pos = lseek(dest_fd, 0, SEEK_CUR);

    /* Buffer size is selected to minimize the overhead from system calls.
     * The value is picked based on coreutils cp(1) benchmarking data described
     * here:
     * https://github.com/coreutils/coreutils/blob/d1b0257077c0b0f0ee25087efd46270345d1dd1f/src/ioblksize.h#L23-L72 */
    char buf[256u * 1024u];
    bool ret = false;

    for (;;) {
        ssize_t rcount = read_eintr(src_fd, buf, sizeof buf);
        
        if (rcount == 0) {
            ret = true;
            goto restore; 
        }

        if (rcount == -1 
            || write_all(dest_fd, buf, (size_t) rcount) == -1) {
            goto restore;
        }
    }

  restore:
    lseek(src_fd, src_orig_pos, SEEK_SET);
    lseek(dest_fd, dest_orig_pos, SEEK_SET);

    if (!ret) {
        return false;
    }

    if ((options & (UNIX_SYNCHRONIZE_DATA | UNIX_SYNCHRONIZE))) {
        return options & UNIX_SYNCHRONIZE_DATA 
            ? fdatasync_eintr(dest_fd) != -1
            : fsync_eintr(dest_fd) != -1;
    }
    
    return true;
}

/** 
 * Hints the filesystem to opportunistically preallocate storage for a file. */
static bool preallocate_storage(int fd, off_t len)
{
#ifdef HAVE_FALLOCATE 
    /* We intentionally use fallocate rather than posix_fallocate() to avoid
     * invoking glibc emulation that writes zeros to the end of the file. We
     * want this call to act like a hint to a filesystem and an early check for
     * the free storage space. We do not want to write zeros only to later
     * overwrite them with the actual data. */
    int ret; 

    do {
        ret = fallocate(fd, FALLOC_FL_KEEP_SIZE, 0, len);

        /* Ignore the error if the operation is not supported by the kernel
         * or filesystem. */
        if (errno == EOPNOTSUPP || errno == ENOSYS) {
            return true;
        }
    } while (unlikely(ret == -1) && errno == EINTR);

    return ret != -1;
#else
    return true;
#endif  /* HAVE_FALLOCATE */
}

bool unix_copy_file(const char src_path[restrict static 1], 
                    const char dest_path[restrict static 1],
                    unsigned char options)
{
    if (((options & UNIX_SKIP_EXISTING) && (options & UNIX_OVERWRITE_EXISTING))
        || ((options & UNIX_SYNCHRONIZE) && (options & UNIX_SYNCHRONIZE_DATA))) {
        return false;
    }

    int src_fd;

    /* open() follows symlinks by default. */
    if (src_fd = open(src_path, O_RDONLY), src_fd == -1) {
        return false;
    }

    int opts = O_WRONLY;
    int dest_fd;

    if (dest_fd = open(dest_path, opts), dest_fd == -1) {
        if (errno != ENOENT) {
            /* File does not already exist. */
            close_eintr(src_fd);
            return false;
        }

        /* Create it. */
        opts |= O_CREAT | O_TRUNC;

        if ((options & UNIX_OVERWRITE_EXISTING) == 0
            || (options & UNIX_SKIP_EXISTING)) {
            opts |= O_EXCL;
        }

        if (dest_fd = open(dest_path, opts, 0640), dest_fd == -1) {
            if (errno == EEXIST && (options & UNIX_SKIP_EXISTING)) {
                close_eintr(src_fd);
                return false;            
            }

            close_eintr(src_fd);
            return false;
        }
    }

    struct stat st;

    /* unix_fcopy_file() calls fstat() too. Can we somehow reduce one syscall? */
    if (fstat(dest_fd, &st) == -1
        || (!preallocate_storage(dest_fd, st.st_size) && (errno == EIO || errno == ENOSPC))) {
        /* FIXME: Oops! But we already truncated the file if UNIX_OVERWRITE_EXISTING
         * was true. Can we remove/unlink it if it was not? Perhaps do it for
         * the other error that shall now follow? */
        close_eintr(src_fd);
        close_eintr(dest_fd);
        return false;
    }
    
    /* TODO: Do not overwrite the destination file in place, since then a failed
     * copy will leave a damaged file. Or perhaps do not bother with this as we
     * are already requested to overwrite the file? */
    const bool ret = unix_fcopy_file(src_fd, dest_fd, options);
    
    /* Ignore errors on read-only file. */
    close_eintr(src_fd);
    
    if (close_eintr(dest_fd) == -1) {
        /* EINPROGRESS is an allowed error code in future POSIX revisions,
         * according to https://www.austingroupbugs.net/view.php?id=529#c1200. */
        if (errno != EINTR && errno != EINPROGRESS) {
            return false;
        }
    }
    
    return ret;
}
