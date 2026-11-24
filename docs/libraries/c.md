# C library

PebbleOS links against a standard C library chosen at build time. `lib/c`
connects it to the kernel: its locks are kernel mutexes, its heap is the
kernel's, as it may not grow one of its own, and `assert()`, like the
assertions of the {doc}`utilities <util>`, logs and stops the system. With
`CONFIG_CONSOLE_STDOUT`, `printf()` writes to the console.

| | |
| --- | --- |
| Directory | `lib/c` |
| Kconfig | C library choice, always built |

## C libraries

| C library | Kconfig |
| --- | --- |
| Picolibc, the default: smallest, from the toolchain or `third_party/picolibc` | `CONFIG_LIBC_PICOLIBC` |
| Newlib, complete but largest | `CONFIG_LIBC_NEWLIB` |
| Newlib nano, with a C99 `printf` linked ahead of its own | `CONFIG_LIBC_NEWLIB_NANO` |
| The host's, on native boards | `CONFIG_LIBC_HOST` |
