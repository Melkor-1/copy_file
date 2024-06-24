#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include "unix-copy-file.h"

/* See: https://unix.stackexchange.com/q/338667/553881. */
#ifdef _AIX
    #error "Replace /dev/stdin with another file (not regular/not symlink)."
#endif  /* _AIX */

/* Current versions of gcc and clang support -std=c2x which sets 
 * __STDC_VERSION__ to this placeholder value. GCC 14.1 does not set
 * __STDC_VERSION__ to 202311L with the std=c23 flag, but Clang 18.1 does. */
#define C23_PLACEHOLDER 202000L
    
#if defined(__STDC_VERSION__) && __STDC_VERSION >= C23_PLACEHOLDER
    #define NORETURN    [[noreturn]]
#elif defined(_MSC_VER)
    #define NORETURN    __declspec(noreturn)
#elif defined(__GNUC__) || defined(__clang__) || defined(__INTEL_LLVM_COMPILER)
    #define NORETURN    __attribute__((noreturn))
#else
    #define NORETURN    _Noreturn
#endif

NORETURN static void cassert(const char cond[static 1], 
                             const char file[static 1],
                             size_t line)
{
    fflush(stdout);
    fprintf(stderr, "Assertion failed: '%s' at %s, line %zu.\n", cond, file, line);
    exit(EXIT_FAILURE);
}

#define test(cond) do { \
    if (!(cond)) { cassert(#cond, __FILE__, __LINE__); } } while (false)

static const char *const valid_path1 = "unix-copy-file.c";
static const char *const valid_path2 = "unix-copy-file.h";
static const char *const invalid_path = "/cdsaknkasc/cskncasdbck/a320cas.caskncas";   

static int valid_fd1;
static int valid_fd2;
static const int invalid_fd = -1;

static void test_unix_fcopy_file(void)
{
    /* Invalid src_fd. */
    test(!unix_fcopy_file(invalid_fd, valid_fd1, UNIX_OVERWRITE_EXISTING));

    /* src_fd is not a regular file or symbolic link. */
    test(!unix_fcopy_file(STDOUT_FILENO, valid_fd1, 0));

    /* Invalid dest_fd. */
    test(!unix_fcopy_file(valid_fd1, invalid_fd, UNIX_OVERWRITE_EXISTING));

    /* dest_fd is not a regular file or symbolic link. */
    test(!unix_fcopy_file(valid_fd1, STDOUT_FILENO, 0));

    /* Both UNIX_SKIP_EXISTING and UNIX_OVERWRITE_EXISTING specified. */
    test(!unix_fcopy_file(valid_fd1, valid_fd2, UNIX_OVERWRITE_EXISTING | UNIX_SKIP_EXISTING));

    /* Both UNIX_SYNCHRONIZE and UNIX_DATA_SYNCHRONIZE specified. */
    test(!unix_fcopy_file(valid_fd1, valid_fd2, UNIX_SYNCHRONIZE | UNIX_SYNCHRONIZE_DATA));

    /* UNIX_SKIP_EXISTING specified. */
    test(!unix_fcopy_file(valid_fd1, valid_fd2, UNIX_SKIP_EXISTING));

    /* src_fd and dest_fd are equivalent. */
    test(!unix_fcopy_file(valid_fd1, valid_fd1, 0));
    test(!unix_fcopy_file(valid_fd2, valid_fd2, 0));
}

static void test_unix_copy_file(void)
{
    /* Invalid src_path. */
    test(!unix_copy_file(invalid_path, valid_path1, UNIX_OVERWRITE_EXISTING));

    /* src_path is not a regular file or symbolic link. */
    test(!unix_copy_file("/dev/stdin", valid_path1, 0));

    /* Invalid dest_path. */
    test(!unix_copy_file(valid_path1, invalid_path, UNIX_OVERWRITE_EXISTING));

    /* dest_path is not a regular file or symbolic link. */
    test(!unix_copy_file(valid_path1, "/dev/stdin", 0));

    /* Both UNIX_SKIP_EXISTING and UNIX_OVERWRITE_EXISTING specified. */
    test(!unix_copy_file(valid_path2, invalid_path, UNIX_OVERWRITE_EXISTING | UNIX_SKIP_EXISTING));

    /* Both UNIX_SYNCHRONIZE and UNIX_DATA_SYNCHRONIZE specified. */
    test(!unix_copy_file(valid_path2, invalid_path, UNIX_SYNCHRONIZE | UNIX_SYNCHRONIZE_DATA));

    /* UNIX_SKIP_EXISTING specified. */
    test(!unix_copy_file(valid_path2, invalid_path, UNIX_SKIP_EXISTING));

    /* src_path and dest_path are equivalent. */
    test(!unix_copy_file(valid_path1, valid_path1, 0));
    test(!unix_copy_file(valid_path2, valid_path2, 0));
}

int main(void)
{
    valid_fd1 = open(valid_path1, O_RDONLY);

    if (valid_fd1 == -1) {
        fprintf(stderr, "error: failed to open \"%s\": %s.\n", valid_path1, strerror(errno));
        return EXIT_FAILURE;
    }
    
    valid_fd2 = open(valid_path2, O_RDONLY);

    if (valid_fd2 == -1) {
        fprintf(stderr, "error: failed to open \"%s\": %s.\n", valid_path2, strerror(errno));
        return EXIT_FAILURE;
    }

    test_unix_fcopy_file();
    test_unix_copy_file();

    close(valid_fd1);
    close(valid_fd2);

    return EXIT_SUCCESS;
}
