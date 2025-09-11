/* os_support_custom.h for PebbleOS */
#ifndef OS_SUPPORT_CUSTOM_H
#define OS_SUPPORT_CUSTOM_H

#include <stdint.h>
#include <stddef.h>

/* Use task-level allocation so Speex temporary/scratch buffers are
	allocated on the current task/app heap instead of the kernel heap. */
extern void *task_zalloc_check(size_t size);
extern void *task_realloc(void *ptr, size_t size);
extern void task_free(void *ptr);

/* Override memory allocation functions to use the task/app heap */
static inline void *speex_alloc(int size) { return task_zalloc_check((size_t)size); }
static inline void *speex_alloc_scratch(int size) { return task_zalloc_check((size_t)size); }
static inline void *speex_realloc(void *ptr, int size) { return task_realloc(ptr, (size_t)size); }
static inline void speex_free(void *ptr) { task_free(ptr); }

/* Override debug/warning functions to be no-ops for embedded system */
static inline void _speex_fatal(const char *str, const char *file, int line) { (void)str; (void)file; (void)line; }
static inline void speex_warning(const char *str) { (void)str; }
static inline void speex_warning_int(const char *str, int val) { (void)str; (void)val; }
static inline void speex_notify(const char *str) { (void)str; }

/* Disable math functions that aren't available */
#define sqrt(x) 0
#define floor(x) ((int)(x))
#define cos(x) 0
#define sin(x) 0
#define exp(x) 1
#define log(x) 0
#define pow(x, y) 1
#define rint(x) ((int)(x))
#define fabs(x) ((x) < 0 ? -(x) : (x))

#endif /* OS_SUPPORT_CUSTOM_H */