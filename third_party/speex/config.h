/* config.h for Speex in PebbleOS */
#ifndef CONFIG_H
#define CONFIG_H

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if your system has a GNU libc compatible `malloc' function,
   and to 0 otherwise. */
#define HAVE_MALLOC 1

/* Define to 1 if you have the `memcpy' function. */
#define HAVE_MEMCPY 1

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define EXPORT macro for symbol visibility */
#define EXPORT

/* Define to 1 to enable fixed-point arithmetic */
#define FIXED_POINT 1

/* Define to 1 to disable floating-point API */
#define DISABLE_FLOAT_API 1

/* Define to 1 to disable VBR */
#define DISABLE_VBR 1

/* Define to 1 to disable wideband */
/* #define DISABLE_WIDEBAND 1 */  /* Commented out to enable wideband support */

/* Define to 1 if you have the `alloca' function. */
#define HAVE_ALLOCA 1

/* Define to 1 if you have the <alloca.h> header file. */
#define HAVE_ALLOCA_H 1

/* The size of `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* The size of `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* Version number of package */
#define VERSION "1.2.1"

/* Define to 1 if the X Window System is missing or not being used. */
#define X_DISPLAY_MISSING 1

/* Disable functions not available in embedded environment */
#undef HAVE_STDIO_H
#undef HAVE_STDLIB_H
#undef HAVE_SQRT
#undef HAVE_FLOOR
#undef HAVE_COS
#undef HAVE_SIN
#undef HAVE_EXP
#undef HAVE_LOG
#undef HAVE_POW
#undef HAVE_RINT
#undef HAVE_FABS

/* Use custom memory allocation for embedded system */
#define OVERRIDE_SPEEX_ALLOC
#define OVERRIDE_SPEEX_ALLOC_SCRATCH
#define OVERRIDE_SPEEX_REALLOC
#define OVERRIDE_SPEEX_FREE
#define OVERRIDE_SPEEX_FREE_SCRATCH

/* Override debug/warning functions */
#define OVERRIDE_SPEEX_FATAL
#define OVERRIDE_SPEEX_WARNING
#define OVERRIDE_SPEEX_WARNING_INT
#define OVERRIDE_SPEEX_NOTIFY

#endif /* CONFIG_H */
