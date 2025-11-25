/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/* This file provides clear error messages when developer 
 * attempts to use a function call supported by newlib-c but
 * not provided by the pebble libraries.
 *
 * ex. use of fopen("test.txt", "r") will display...
 * src/main.c:123:3 error: static assertion failed: "fopen() is not supported"
*/

// Rather than asserting on printf, 
// lets map it to app_log for simple cross-compiles
#define printf(format, ...) \
  app_log(APP_LOG_LEVEL_DEBUG, __FILE_NAME__, __LINE__, format, ##__VA_ARGS__)

// List of unsupported file io calls
#define fopen(...)    _Static_assert(0, "fopen() is not supported")
#define fclose(...)   _Static_assert(0, "fclose() is not supported")
#define fread(...)    _Static_assert(0, "fread() is not supported")
#define fwrite(...)   _Static_assert(0, "fwrite() is not supported")
#define fseek(...)    _Static_assert(0, "fseek() is not supported")
#define ftell(...)    _Static_assert(0, "ftell() is not supported")
#define fsetpos(...)  _Static_assert(0, "fsetpos() is not supported")
#define fscanf(...)   _Static_assert(0, "fscanf() is not supported")
#define fgetc(...)    _Static_assert(0, "fgetc() is not supported")
#define fgets(...)    _Static_assert(0, "fgets() is not supported")
#define fputc(...)    _Static_assert(0, "fputc() is not supported")
#define fputs(...)    _Static_assert(0, "fputs() is not supported")

// List of unsupported print calls
#define fprintf(...)    _Static_assert(0, "fprintf() is not supported")
#define sprintf(...)    _Static_assert(0, "sprintf() is not supported")
#define vfprintf(...)   _Static_assert(0, "vfprintf() is not supported")
#define vsprintf(...)   _Static_assert(0, "vsprintf() is not supported")
#define vsnprintf(...)  _Static_assert(0, "vsnprintf() is not supported")

// List of unsupported io calls
#define open(...)   _Static_assert(0, "open() is not supported")
#define close(...)  _Static_assert(0, "close() is not supported")
#define creat(...)  _Static_assert(0, "creat() is not supported")
#define read(...)   _Static_assert(0, "read() is not supported")
#define write(...)  _Static_assert(0, "write() is not supported")
#define stat(...)   _Static_assert(0, "stat() is not supported")

// List of unsupported memory calls
#define alloca(...)   _Static_assert(0, "alloca() is not supported")
#define mmap(...)     _Static_assert(0, "mmap() is not supported")
#define brk(...)      _Static_assert(0, "brk() is not supported")
#define sbrk(...)     _Static_assert(0, "sbrk() is not supported")

