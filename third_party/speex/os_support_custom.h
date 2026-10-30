/* os_support_custom.h for PebbleOS */
#ifndef OS_SUPPORT_CUSTOM_H
#define OS_SUPPORT_CUSTOM_H

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/malloc.h>

/* Override memory allocation functions to use the kernel heap */
static inline void *speex_alloc(int size) {
  return pbl_zalloc_check((size_t)size);
}
static inline void *speex_alloc_scratch(int size) {
  return pbl_zalloc_check((size_t)size);
}
static inline void *speex_realloc(void *ptr, int size) {
  return pbl_realloc(ptr, (size_t)size);
}
static inline void speex_free(void *ptr) {
  pbl_free(ptr);
}
static inline void speex_free_scratch(void *ptr) {
  pbl_free(ptr);
}

/* Override debug/warning functions to be no-ops for embedded system */
static inline void _speex_fatal(const char *str, const char *file, int line) {
  (void)str;
  (void)file;
  (void)line;
}
static inline void speex_warning(const char *str) {
  (void)str;
}
static inline void speex_warning_int(const char *str, int val) {
  (void)str;
  (void)val;
}
static inline void speex_notify(const char *str) {
  (void)str;
}

#endif /* OS_SUPPORT_CUSTOM_H */
