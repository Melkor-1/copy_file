#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE

#define _POSIX_C_SOURCE 200819
#define _XOPEN_SOURCE   700

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "unix-copy-file.h"

/* See: https://unix.stackexchange.com/q/338667/553881. */
#ifdef _AIX
    #error "Replace /dev/stdin with another file (not regular/not symlink)."
#else 
    #define NOT_ISREG_OR_ISLNK  "/dev/stdin"
#endif
/* _AIX */

[[noreturn, gnu::format(printf, 1, 2)]] static void fatal_error(const char fmt[static 1], ...)
{
    va_list args;
    va_start(args);
    vfprintf(stderr, fmt, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

[[noreturn]] static void cassert(const char cond[static 1], 
                                 const char file[static 1],
                                 size_t line)
{
    fflush(stdout);
    fprintf(stderr, "Assertion failed: '%s' at %s, line %zu.\n", cond, file, line);
    exit(EXIT_FAILURE);
}

#define BLOCK(...)      do { __VA_ARGS__ } while (false)

#define fatal(cond, fmt, ...)                                     \
    BLOCK(                                                        \
        if ((cond)) {                                             \
            fatal_error("%s::%d::%s(): " fmt, __FILE__, __LINE__, \
                __func__, __VA_ARGS__);                           \
        }                                                         \
    )
    
#define test(cond)                              \
    BLOCK(                                      \
        if (!(cond)) {                          \
            cassert(#cond, __FILE__, __LINE__); \
        }                                       \
    )

static bool has_same_perms_path(const char path1[static 1], const char path2[static 1])
{
    struct stat st1;
    struct stat st2;

    fatal(stat(path1, &st1) == -1 || stat(path2, &st2) == -1,
        "error: fstat failed: %s.\n", strerror(errno));

    return st1.st_mode == st2.st_mode;
}

static bool has_same_perms_fd(int fd1, int fd2)
{
    struct stat st1;
    struct stat st2;

    fatal(fstat(fd1, &st1) == -1 || fstat(fd2, &st2) == -1,
        "error: fstat failed: %s.\n", strerror(errno));

    return st1.st_mode == st2.st_mode;
}

static bool has_same_contents(const char file1[restrict static 1], 
                              const char file2[restrict static 1])
{
    switch (fork()) {
        case -1: 
            fatal(true, "error: failed to fork child: %s.\n", strerror(errno));

        case 0:  
            execlp("cmp", "cmp", file1, file2, (char *)0); 
            _Exit(EXIT_FAILURE);

        default: 
            int status; 
            
            fatal(wait(&status) == -1, "error: could not wait for child: %s.\n",
                strerror(errno));
            
            return WIFEXITED(status) != 0 && WEXITSTATUS(status) == 0;
    }

    unreachable();
}

static int create_temp_file(char temp[static 1])
{
    int fd;

    /* printf("%s\n", temp); */
    /* printf("%d\n", fd = mkstemp(temp)); */
    fatal((fd = mkstemp(temp)) == -1, 
        "error: failed to generate temporary file: %s.\n", strerror(errno));

    return fd;
}

static void test_unix_fcopy_file(void)
{
    static char temp1[] = "To-be-or-not-to-be.XXXXXX";
    static char temp2[] = "That-is-the-question.XXXXXX";
    const int valid_fd1 = create_temp_file(temp1);
    const int valid_fd2 = create_temp_file(temp2);
    const int invalid_fd = -1;
    
    /* Invalid src_fd. */
    test(!unix_fcopy_file(invalid_fd, valid_fd1, UNIX_OVERWRITE_EXISTING));

    /* src_fd is not a regular file or symbolic link. */
    test(!unix_fcopy_file(STDOUT_FILENO, valid_fd1, UNIX_NONE));

    /* Invalid dest_fd. */
    test(!unix_fcopy_file(valid_fd1, invalid_fd, UNIX_OVERWRITE_EXISTING));

    /* dest_fd is not a regular file or symbolic link. */
    test(!unix_fcopy_file(valid_fd1, STDOUT_FILENO, UNIX_NONE));

    /* Both UNIX_SKIP_EXISTING and UNIX_OVERWRITE_EXISTING specified. */
    test(!unix_fcopy_file(valid_fd1, valid_fd2, UNIX_OVERWRITE_EXISTING | UNIX_SKIP_EXISTING));

    /* Both UNIX_SYNCHRONIZE and UNIX_DATA_SYNCHRONIZE specified. */
    test(!unix_fcopy_file(valid_fd1, valid_fd2, UNIX_SYNCHRONIZE | UNIX_SYNCHRONIZE_DATA));

    /* UNIX_SKIP_EXISTING specified. */
    test(!unix_fcopy_file(valid_fd1, valid_fd2, UNIX_SKIP_EXISTING));

    /* src_fd and dest_fd are equivalent, same file descriptors. */
    test(!unix_fcopy_file(valid_fd1, valid_fd1, UNIX_NONE));
    test(!unix_fcopy_file(valid_fd2, valid_fd2, UNIX_NONE));

    /* src_fd and dest_fd are equivalent, different file descriptors. */
    int dup_valid_fd1;

    fatal((dup_valid_fd1 = dup(valid_fd1)) == -1, 
        "error: failed to get duplicate file descriptor with dup(): %s.\n", strerror(errno));
    
    test(!unix_fcopy_file(valid_fd1, dup_valid_fd1, UNIX_OVERWRITE_EXISTING));

    /* Now test for success. */
    test(unix_fcopy_file(valid_fd1, valid_fd2, UNIX_OVERWRITE_EXISTING | UNIX_SYNCHRONIZE));
    test(has_same_perms_fd(valid_fd1, valid_fd2));
    test(has_same_contents(temp1, temp2));

    fatal(unlink(temp1) == -1, "error: failed to unlink: \"%s\": %s.\n", temp1,
        strerror(errno)); 
    fatal(unlink(temp2) == -1, "error: failed to unlink: \"%s\": %s.\n", temp2,
        strerror(errno));

    close(valid_fd1);
    close(valid_fd2);
}

static void test_unix_copy_file(void)
{
    /* Perfect for this situation, they're deprecated for other reasons. */
    const char *const valid_path1 = tmpnam((static char [L_tmpnam]){}); 
    const char *const valid_path2 = tmpnam((static char [L_tmpnam]){}); 

    fatal(!valid_path1 || !valid_path2, 
        "error: could not generate temporary file names: %s.\n", strerror(errno));

    /* At least this does not exist in this directory. */
    static const char *const invalid_path = "caskncsaccskncasdbckcaksncbadska320cas.caskncas";   

    /* Invalid src_path. */
    test(!unix_copy_file(invalid_path, valid_path1, UNIX_OVERWRITE_EXISTING));

    /* src_path is not a regular file or symbolic link. */
    test(!unix_copy_file(NOT_ISREG_OR_ISLNK, valid_path1, UNIX_NONE));

    /* dest_path is not a regular file or symbolic link. */
    test(!unix_copy_file(valid_path1, NOT_ISREG_OR_ISLNK, UNIX_NONE));

    /* Both UNIX_SKIP_EXISTING and UNIX_OVERWRITE_EXISTING specified. */
    test(!unix_copy_file(valid_path2, invalid_path, UNIX_OVERWRITE_EXISTING | UNIX_SKIP_EXISTING));

    /* Both UNIX_SYNCHRONIZE and UNIX_DATA_SYNCHRONIZE specified. */
    test(!unix_copy_file(valid_path2, invalid_path, UNIX_SYNCHRONIZE | UNIX_SYNCHRONIZE_DATA));

    /* UNIX_SKIP_EXISTING specified. */
    test(!unix_copy_file(valid_path2, invalid_path, UNIX_SKIP_EXISTING));

    /* src_path and dest_path are equivalent. */
    test(!unix_copy_file(valid_path1, valid_path1, UNIX_NONE));
    test(!unix_copy_file(valid_path2, valid_path2, UNIX_NONE));

    /* Now test for success. */
    test(unix_copy_file(valid_path1, valid_path2, UNIX_OVERWRITE_EXISTING | UNIX_SYNCHRONIZE));
    test(has_same_perms_path(valid_path1, valid_path2));
    test(has_same_contents(valid_path1, valid_path2));

    fatal(unlink(valid_path1) == -1, "error: failed to unlink: \"%s\": %s.\n",
          valid_path1, strerror(errno));

    fatal(unlink(valid_path2) == -1, "error: failed to unlink: \"%s\": %s.\n",
          valid_path2, strerror(errno));
}

int main(void)
{
    test_unix_fcopy_file();
    test_unix_copy_file();

    return EXIT_SUCCESS;
}
