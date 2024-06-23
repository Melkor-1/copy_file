# copy_file - A portable function to copy files for UNIX and UNIX-like systems

Following the POSIX API, there are 2 versions of the function: `unix_copy_file()`, and `unix_fcopy_file()`. One works with file descriptors, and is preceded with `f` (like `fchmod()`, `fchown()`, `fstat()` et cetera), and the other with file paths (like `chmod()`, `chown()`, `stat()`).

The functions are modeled after Boost library's `filesystem::copy_file` function. `copy_options::update_existing` is not supported, mainly because as the its documentation states:

> [Note: When `copy_options::update_existing` is specified, checking the write times of `from` and `to` may not be atomic with the copy operation. Another process may create or modify the file identified by `to` after the file modification times have been checked but before copying starts. In this case the target file will be overwritten.]

## Building

Note that you would require a C23 compiler to build and run the tests, and specify the compiler with `CC`. To build and run the tests: 

```shell
make CC=gcc-13   # Where gcc-13 is any compiler with C23 support 
```


