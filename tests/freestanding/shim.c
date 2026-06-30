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
 * realloc — size-header implementation.
 *
 * realloc needs to know the old allocation's size to copy the right number
 * of bytes.  We achieve this by storing a size_t header immediately before
 * the user pointer:
 *
 *   [ size_t bytes_pad_to_align | user data ... ]
 *   ^                              ^
 *   raw malloc'd block             returned to caller
 *
 * This is safe because the ONLY caller of realloc in the clar harness is
 * clar_category_add_to_list, which:
 *   - first call: passes NULL (we treat as malloc)
 *   - subsequent calls: passes the pointer returned by our previous realloc
 *
 * The names array is a global static and is never freed, so there is no
 * mismatch between our size-header pointer and bare free.
 */
#define _REALLOC_ALIGN \
  (sizeof(size_t) > sizeof(void *) ? sizeof(size_t) : sizeof(void *))

static void *_realloc_alloc(size_t bytes) {
  char *raw = malloc(_REALLOC_ALIGN + bytes);
  if (!raw) return NULL;
  *(size_t *)(void *)raw = bytes;
  return raw + _REALLOC_ALIGN;
}

static void _realloc_free(void *ptr) {
  free((char *)ptr - _REALLOC_ALIGN);
}

static size_t _realloc_size(const void *ptr) {
  const char *raw = (const char *)ptr - _REALLOC_ALIGN;
  return *(const size_t *)(const void *)raw;
}

void *realloc(void *ptr, size_t size) {
  if (ptr == NULL) {
    return _realloc_alloc(size);
  }
  if (size == 0) {
    _realloc_free(ptr);
    return NULL;
  }
  void *new_ptr = _realloc_alloc(size);
  if (!new_ptr) return NULL;
  size_t old_size = _realloc_size(ptr);
  memcpy(new_ptr, ptr, old_size < size ? old_size : size);
  _realloc_free(ptr);
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
