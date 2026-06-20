/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Host-boundary adapter for freestanding test builds.
 *
 * This is the ONE translation unit compiled WITHOUT -nostdinc, so it can
 * reach host-libc headers.  All other TUs in a freestanding build use only
 * pblibc headers and the compiler's built-in includes.
 *
 * Symbols provided:
 *
 *   freestanding_write — write(2)-backed output, used by clar_io_freestanding.c
 *   calloc / realloc / strdup — heap helpers missing from pblibc's stdlib.h
 *   abort — not in pblibc's stdlib.h
 *   strcasecmp — POSIX, absent from pblibc; used by clar_categorize.c
 *   qsort — used only on the -l listing path; insertion-sort stub
 *
 * Note: malloc and free are declared in pblibc/stdlib.h and resolve to the
 * host libc at link time without any forwarding needed here.
 *
 * Note: printf, fprintf, vprintf, vfprintf, fflush, FILE, and stderr are
 * provided by clar_io_freestanding.c (compiled as part of clar_main.c under
 * -nostdinc).  They are NOT defined here.
 */

/* Host headers — only allowed in this file. */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Platform allocation-size query — needed for a free-compatible realloc. */
#if defined(__APPLE__)
#  include <malloc/malloc.h>
#  define _host_malloc_size(p) malloc_size(p)
#elif defined(__linux__)
#  include <malloc.h>
#  define _host_malloc_size(p) malloc_usable_size(p)
#else
#  error "Unsupported platform: cannot determine allocation size for realloc"
#endif

/* ---- output helper ------------------------------------------------------- */

/*
 * Write len bytes from buf to file descriptor fd.
 * fd 1 = stdout, fd 2 = stderr.
 * Called from clar_io_freestanding.c, which is compiled under -nostdinc.
 */
void freestanding_write(int fd, const char *buf, int len) {
  while (len > 0) {
    ssize_t n = write(fd, buf, (size_t)len);
    if (n <= 0) {
      break;
    }
    buf += n;
    len -= (int)n;
  }
}

/* ---- heap ---------------------------------------------------------------- */

/*
 * calloc — uses bare malloc (compatible with bare free).
 *
 * clar allocates error structs with calloc and frees them with free, so the
 * allocation must be compatible with the host free.  malloc from pblibc's
 * stdlib.h resolves to the host libc at link time, so this is correct.
 */
void *calloc(size_t nmemb, size_t size) {
  size_t total = nmemb * size;
  void *p = malloc(total);
  if (p) {
    memset(p, 0, total);
  }
  return p;
}

/*
 * realloc — malloc/memcpy/free implementation using the platform allocator's
 * own size query so that all pointers are fully compatible with bare free().
 *
 * The old size-header trick (_REALLOC_ALIGN prefix) broke as soon as product
 * code called realloc() and then later called free() on the returned pointer,
 * because free() saw an interior pointer rather than the raw malloc block.
 * Using malloc_size (macOS) / malloc_usable_size (Linux) avoids the header
 * entirely: every returned pointer is a plain malloc'd block that free() can
 * release without any special handling.
 */
void *realloc(void *ptr, size_t size) {
  if (ptr == NULL) {
    return malloc(size);
  }
  if (size == 0) {
    free(ptr);
    return NULL;
  }
  void *new_ptr = malloc(size);
  if (!new_ptr) return NULL;
  size_t old_size = _host_malloc_size(ptr);
  memcpy(new_ptr, ptr, old_size < size ? old_size : size);
  free(ptr);
  return new_ptr;
}

char *strdup(const char *s) {
  size_t n = strlen(s) + 1;
  char *p = malloc(n);
  if (p) {
    memcpy(p, s, n);
  }
  return p;
}

void abort(void) {
  _exit(1);
}

/* ---- POSIX stubs --------------------------------------------------------- */

int strcasecmp(const char *s1, const char *s2) {
  unsigned char c1, c2;
  do {
    c1 = (unsigned char)*s1++;
    c2 = (unsigned char)*s2++;
    if (c1 >= 'A' && c1 <= 'Z') c1 += ('a' - 'A');
    if (c2 >= 'A' && c2 <= 'Z') c2 += ('a' - 'A');
  } while (c1 && c1 == c2);
  return (int)c1 - (int)c2;
}

/*
 * qsort — insertion-sort stub.  Only called from clar_category_print_enabled()
 * which runs on the -l listing path; never invoked during a normal test run.
 */
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *)) {
  char *b = (char *)base;
  char tmp[256];
  for (size_t i = 1; i < nmemb; i++) {
    size_t j = i;
    while (j > 0 && compar(b + (j - 1) * size, b + j * size) > 0) {
      if (size <= sizeof(tmp)) {
        memcpy(tmp,                    b + (j - 1) * size, size);
        memcpy(b + (j - 1) * size,    b + j * size,       size);
        memcpy(b + j * size,           tmp,                size);
      }
      j--;
    }
  }
}
