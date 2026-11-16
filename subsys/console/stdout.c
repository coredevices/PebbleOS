/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdio.h>

#include <console/console_private.h>
#include <unistd.h>

#ifdef CONFIG_LIBC_PICOLIBC
static int prv_put(char c, FILE *file) {
  pbl_console__write_text(&c, 1);

  return (unsigned char)c;
}

static FILE s_stdout = FDEV_SETUP_STREAM(prv_put, nullptr, nullptr, _FDEV_SETUP_WRITE);

FILE *const stdout = &s_stdout;
FILE *const stderr = &s_stdout;
#endif

#if defined(CONFIG_LIBC_NEWLIB) || defined(CONFIG_LIBC_NEWLIB_NANO)
int _write(int fd, const void *data, size_t len) {
  if (fd != STDOUT_FILENO && fd != STDERR_FILENO) {
    errno = EBADF;
    return -1;
  }

  pbl_console__write_text(data, len);

  return len;
}
#endif

#ifdef CONFIG_LIBC_HOST
// The native console is the process's standard output. Going through the console instead could
// switch threads under the host's stdio lock.
__attribute__((constructor)) static void prv_stdout_init(void) {
  setvbuf(stdout, nullptr, _IONBF, 0);
}
#endif
