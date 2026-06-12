/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Freestanding I/O layer included into clar_main.c.
 *
 * Provides printf / fprintf / vprintf / vfprintf / fflush implementations
 * that work under -nostdinc by routing output through freestanding_write()
 * (defined in tests/freestanding/shim.c, which IS compiled against host
 * headers).
 *
 * Also declares strcasecmp so clar_categorize.c compiles without
 * <strings.h>, which is a host-only header.
 *
 * This module is included into the clar_main.c translation unit by clar.py
 * when --report-to=freestanding is requested.  It must appear before
 * clar_categorize.c in the module list so that strcasecmp is declared.
 */

/* freestanding_write() lives in shim.c; declare it here for the call below. */
void freestanding_write(int fd, const char *buf, int len);

/* pblibc's stdio.h already declared printf/fprintf/vprintf/vfprintf with
 * the correct signatures (included at the top of clar.c).  We provide the
 * definitions here inside the same TU so no duplicate-definition conflict
 * arises.
 *
 * pblibc's vsprintf.c provides vsnprintf/snprintf; they are linked as a
 * separate object, not inlined here. */

/* Forward-declare vsnprintf exactly as pblibc/stdio.h does so this module
 * can be read in isolation during review. */
#ifdef __GNUC__
__attribute__((__format__(__printf__, 3, 0)))
#endif
int vsnprintf(char * restrict str, size_t size,
              const char * restrict fmt, __gnuc_va_list ap);

/* Declare strcasecmp — not in pblibc, provided by shim.c. */
int strcasecmp(const char *s1, const char *s2);

/* The FILE type is declared in pblibc's stdio.h as an empty struct.  We
 * define the stderr object here; clar.c's clar_print_onabort writes to
 * it via vfprintf(stderr, …). */
FILE _clar_stderr_storage;
FILE *stderr = &_clar_stderr_storage;

/* Internal helper: vsnprintf into a stack buffer then write to fd. */
static int _clar_vwrite(int fd, const char *fmt, va_list ap) {
  char buf[2048];
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  if (n > 0) {
    int len = n < (int)(sizeof(buf) - 1) ? n : (int)(sizeof(buf) - 1);
    freestanding_write(fd, buf, len);
  }
  return n;
}

int vprintf(const char * restrict fmt, __gnuc_va_list ap) {
  return _clar_vwrite(1, fmt, ap);
}

int printf(const char * restrict fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = _clar_vwrite(1, fmt, ap);
  va_end(ap);
  return n;
}

int vfprintf(FILE * restrict stream, const char * restrict fmt,
             __gnuc_va_list ap) {
  /* Route stderr (fd 2), everything else to stdout (fd 1). */
  int fd = (stream == stderr) ? 2 : 1;
  return _clar_vwrite(fd, fmt, ap);
}

int fprintf(FILE * restrict stream, const char * restrict fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vfprintf(stream, fmt, ap);
  va_end(ap);
  return n;
}

int fflush(FILE * restrict stream) {
  /* write(2) is unbuffered; this is a genuine no-op. */
  (void)stream;
  return 0;
}
