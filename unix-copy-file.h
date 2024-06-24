#ifndef UNIX_COPY_FILE_H
#define UNIX_COPY_FILE_H 1

#include <sys/types.h>

#define UNIX_NONE                   0b0000'0000
#define UNIX_OVERWRITE_EXISTING     0b0000'0001
#define UNIX_SKIP_EXISTING          0b0000'0010
#define UNIX_SYNCHRONIZE_DATA       0b0000'0100
#define UNIX_SYNCHRONIZE            0b0000'1000

/**
 * io_fcopy_file() copies a single file from src_fd to dest_fd, using the copy
 * options indicated by options. 
 *
 * src_fd:  A file descriptor opened for reading.
 * dest_fd: A file descriptor opened for writing.
 * options: Copy options.
 *
 * Precondition: 
 *     options must contains at most one option from each of the following groups:
 *       - UNIX_SKIP_EXISTING or UNIX_OVERWRITE_EXISTING
 *       - UNIX_SYNCHRONIZE or UNIX_SYNCHRONIZE_DATA
 *
 *
 * Effects: 
 *     Fail if:
 *       - src_fd is invalid, or does not correspond to a regular file or 
 *         symbolic link.
 *       - dest_fd is invalid, or does not correspond to a regular file or
 *         symbolic link.
 *       - src_fd and dest_fd correspond to the same file.
 *       - Both UNIX_OVERWRITE_EXISTING and UNIX_SKIP_EXISTING are set.
 *       - Both UNIX_SYNCHRONIZE and UNIX_SYNCHRONIZE_DATA are set.
 *
 *     Otherwise, return successfully with no effect if dest_fd is valid and 
 *     (options & UNIX_SKIP_EXISTING) != UNIX_NONE.
 *
 *     Otherwise:
 *       - The contents and attributes of the file corresponding to src_fd are
 *         copied to the file corresponding to dest_fd; then
 *       - If (options & UNIX_SYNCHRONIZE) != UNIX_NONE, the written data and 
 *         attributes are synchronized with the permanent storage; otherwise
 *       - If (options & UNIX_SYNCHRONIZE_DATA) != UNIX_NONE, the written data
 *         is synchronized with the permanent storage.
 *
 * Returns:
 *     true if the file was copied without error, otherwise false.
 *
 * Note:
 *     - The UNIX_SYNCHRONIZE_DATA and UNIX_SYNCHRONIZE options may have a
 *       significant performance impact. The UNIX_SYNCHRONIZE_DATA option may 
 *       be less expensive than UNIX_SYNCHRONIZE. However, without these options,
 *       upon returning from unix_fcopy_file() it is not guaranteed that the
 *       copied file is completely written and preserved in case of a system
 *       failure. Any delayed write operations may fail after the function
 *       returns, at the point of physically writing the data to the underlying
 *       media, and this error shall not be reported to the caller.
 *
 *     - This copying is not done within the kernel, and would require 
 *       transferring data to and from user space. But it is portable across 
 *       all UNIX-like systems. 
 *
 *     - The seek position of both the source file descriptor and the
 *       destination file descriptors are restored before returning.
 *
 *     - Symbolic links are followed. */
[[nodiscard, gnu::nonnull]] bool unix_copy_file(const char src_path[restrict static 1], 
                                                const char dest_path[restrict static 1],
                                                unsigned char options);

/**
 * io_copy_file() functions exactly the same as io_fcopy_file(), except that it
 * works with file paths instead of file descriptors.
 *
 * src_path:  Path to the source file.
 * dest_path: Path to the destination file.
 * options:   Copy options. 
 *
 * Note: 
 *     If dest_path does not exist, and (options & UNIX_SKIP_EXISTING) ==
 *     UNIX_NONE, the file is created. */
[[nodiscard]] bool unix_fcopy_file(int src_fd, int dest_fd, unsigned char options);

#endif /* UNIX_COPY_FILE_H */
