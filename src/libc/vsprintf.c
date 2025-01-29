///////////////////////////////////////
// Implements:
//  int vsnprintf(char *str, size_t size, const char *format, va_list args)
//  int vsprintf(char *buf, const char *fmt, va_list args)
//  int vsniprintf(char *str, size_t size, const char *format, va_list args)
//  int sniprintf(char *dst, size_t size, const char *fmt, ...)
//  int snprintf(char *dst, size_t size, const char *fmt, ...)
//  int sprintf(char *dst, const char *fmt, ...)
///////////////////////////////////////
// Exports to apps:
//   vsnprintf

/*
 * Copyright (c) 1995 Patrick Powell.
 *
 * This code is based on code written by Patrick Powell <papowell@astart.com>.
 * It may be used for any purpose as long as this notice remains intact on all
 * source code distributions.

 * Copyright (c) 2008 Holger Weiss.
 *
 * This version of the code is maintained by Holger Weiss <holger@jhweiss.de>.
 * My changes to the code may freely be used, modified and/or redistributed for
 * any purpose.  It would be nice if additions and fixes to this file (including
 * trivial code cleanups) would be sent back in order to let me include them in
 * the version available at <http://www.jhweiss.de/software/snprintf.html>.
 * However, this is not a requirement for using or redistributing (possibly
 * modified) versions of this file, nor is leaving this notice intact mandatory.
 */

/*
 * History
 *
 * 2015-07-07 Alex Marshall <amarshall@pebble.com>
 *
 *  More cleanup, removed vestigial quote flag support.
 *
 * 2015-07-06 Alex Marshall <amarshall@pebble.com>
 *
 *  Cleaned up code style things, modified various things to be more compliant
 *  with C99 standard, and changed some small things for compatibility with
 *  newlib's sprintf implementation.
 *
 * 2009-03-05 Hector Martin "marcan" <marcan@marcansoft.com>
 *
 *  Hacked up and removed a lot of stuff including floating-point support,
 *  a bunch of ifs and defines, locales, and tests
 *
 * 2008-01-20 Holger Weiss <holger@jhweiss.de> for C99-snprintf 1.1:
 *
 *  Fixed the detection of infinite floating point values on IRIX (and
 *  possibly other systems) and applied another few minor cleanups.
 *
 * 2008-01-06 Holger Weiss <holger@jhweiss.de> for C99-snprintf 1.0:
 *
 *  Added a lot of new features, fixed many bugs, and incorporated various
 *  improvements done by Andrew Tridgell <tridge@samba.org>, Russ Allbery
 *  <rra@stanford.edu>, Hrvoje Niksic <hniksic@xemacs.org>, Damien Miller
 *  <djm@mindrot.org>, and others for the Samba, INN, Wget, and OpenSSH
 *  projects.  The additions include: support the "e", "E", "g", "G", and
 *  "F" conversion specifiers (and use conversion style "f" or "F" for the
 *  still unsupported "a" and "A" specifiers); support the "hh", "ll", "j",
 *  "t", and "z" length modifiers; support the "#" flag and the (non-C99)
 *  "'" flag; use localeconv(3) (if available) to get both the current
 *  locale's decimal point character and the separator between groups of
 *  digits; fix the handling of various corner cases of field width and
 *  precision specifications; fix various floating point conversion bugs;
 *  handle infinite and NaN floating point values; don't attempt to write to
 *  the output buffer (which may be NULL) if a size of zero was specified;
 *  check for integer overflow of the field width, precision, and return
 *  values and during the floating point conversion; use the OUTCHAR() macro
 *  instead of a function for better performance; provide asprintf(3) and
 *  vasprintf(3) functions; add new test cases.  The replacement functions
 *  have been renamed to use an "rpl_" prefix, the function calls in the
 *  main project (and in this file) must be redefined accordingly for each
 *  replacement function which is needed (by using Autoconf or other means).
 *  Various other minor improvements have been applied and the coding style
 *  was cleaned up for consistency.
 *
 * 2007-07-23 Holger Weiss <holger@jhweiss.de> for Mutt 1.5.13:
 *
 *  C99 compliant snprintf(3) and vsnprintf(3) functions return the number
 *  of characters that would have been written to a sufficiently sized
 *  buffer (excluding the '\0').  The original code simply returned the
 *  length of the resulting output string, so that's been fixed.
 *
 * 1998-03-05 Michael Elkins <me@mutt.org> for Mutt 0.90.8:
 *
 *  The original code assumed that both snprintf(3) and vsnprintf(3) were
 *  missing.  Some systems only have snprintf(3) but not vsnprintf(3), so
 *  the code is now broken down under HAVE_SNPRINTF and HAVE_VSNPRINTF.
 *
 * 1998-01-27 Thomas Roessler <roessler@does-not-exist.org> for Mutt 0.89i:
 *
 *  The PGP code was using unsigned hexadecimal formats.  Unfortunately,
 *  unsigned formats simply didn't work.
 *
 * 1997-10-22 Brandon Long <blong@fiction.net> for Mutt 0.87.1:
 *
 *  Ok, added some minimal floating point support, which means this probably
 *  requires libm on most operating systems.  Don't yet support the exponent
 *  (e,E) and sigfig (g,G).  Also, fmtint() was pretty badly broken, it just
 *  wasn't being exercised in ways which showed it, so that's been fixed.
 *  Also, formatted the code to Mutt conventions, and removed dead code left
 *  over from the original.  Also, there is now a builtin-test, run with:
 *  gcc -DTEST_SNPRINTF -o snprintf snprintf.c -lm && ./snprintf
 *
 * 1996-09-15 Brandon Long <blong@fiction.net> for Mutt 0.43:
 *
 *  This was ugly.  It is still ugly.  I opted out of floating point
 *  numbers, but the formatter understands just about everything from the
 *  normal C string format, at least as far as I can tell from the Solaris
 *  2.5 printf(3S) man page.
 */

#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <ctype.h>
#include <stdbool.h>

#include "pblibc_private.h"

// Not using 64-bit division for Dialog Bluetooth. This saves *gobs* of memory.
#ifdef ARCH_NO_NATIVE_LONG_DIVIDE
  #define CONVERT_VALUE_TYPE uint32_t
#else
  #define CONVERT_VALUE_TYPE UINTMAX_T
#endif

#define VA_START(ap, last) va_start(ap, last)
#define VA_SHIFT(ap, value, type) /* No-op for ANSI C. */

// These are the correct sizes for everything on our target platform
// Doesn't matter much for building on the target, but it makes unit tests easier.
#define ULLONG uint64_t
#define UINTMAX_T uint64_t
#define LLONG int64_t
#define INTMAX_T int64_t
#define UINTPTR_T uint32_t
#define PTRDIFF_T int32_t
#define UPTRDIFF_T uint32_t
#define SSIZE_T int32_t
#define SIZE_T uint32_t

// amarshall: Changed this to be max for uint64, as we can't ever do uint128 anyways!
/*
 * Buffer size to hold the octal string representation of UINT64_MAX without
 * nul-termination ("1777777777777777777777").
 */
#ifdef MAX_CONVERT_LENGTH
#undef MAX_CONVERT_LENGTH
#endif
#define MAX_CONVERT_LENGTH      22

/* Format read states. */
#define PRINT_S_DEFAULT         0
#define PRINT_S_FLAGS           1
#define PRINT_S_WIDTH           2
#define PRINT_S_DOT             3
#define PRINT_S_PRECISION       4
#define PRINT_S_MOD             5
#define PRINT_S_CONV            6

/* Format flags. */
#define PRINT_F_MINUS           (1 << 0)
#define PRINT_F_PLUS            (1 << 1)
#define PRINT_F_SPACE           (1 << 2)
#define PRINT_F_NUM             (1 << 3)
#define PRINT_F_ZERO            (1 << 4)
#define PRINT_F_UP              (1 << 6)
#define PRINT_F_UNSIGNED        (1 << 7)
#define PRINT_F_TYPE_G          (1 << 8)
#define PRINT_F_TYPE_E          (1 << 9)

/* Conversion flags. */
#define PRINT_C_CHAR            1
#define PRINT_C_SHORT           2
#define PRINT_C_LONG            3
#define PRINT_C_LLONG           4
#define PRINT_C_SIZE            6
#define PRINT_C_PTRDIFF         7
#define PRINT_C_INTMAX          8

#ifndef MAX
# define MAX(x, y) ((x >= y) ? x : y)
#endif

#ifndef CHARTOINT
# define CHARTOINT(ch) (ch - '0')
#endif

#ifndef ISDIGIT
# define ISDIGIT(ch) isdigit(ch)
#endif

#define OUTCHAR(str, len, size, ch) do { \
  if ((len) + 1 < size) { \
    str[len] = ch; \
  } \
  (len)++; \
} while (0)

static int prv_convert(CONVERT_VALUE_TYPE value, char *buf, size_t size, int base, int caps) {
  const char *digits = caps ? "0123456789ABCDEF" : "0123456789abcdef";
  size_t pos = 0;

  /* We return an unterminated buffer with the digits in reverse order. */
  do {
    buf[pos++] = digits[value % base];
    value /= base;
  } while (value != 0 && pos < size);

  return (int)pos;
}

static void fmtstr(char *str, size_t *len, size_t size, const char *value, int width, int precision,
                   int flags) {
  int padlen, strln;  /* Amount to pad. */
  int noprecision = (precision == -1);

  if (value == NULL) { /* We're forgiving. */
    value = "(null)";
  }

  /* If a precision was specified, don't read the string past it. */
  for (strln = 0; value[strln] != '\0' && (noprecision || strln < precision); strln++) {
    continue;
  }

  if ((padlen = width - strln) < 0) {
    padlen = 0;
  }
  if (flags & PRINT_F_MINUS) { /* Left justify. */
    padlen = -padlen;
  }

  while (padlen > 0) {  /* Leading spaces. */
    OUTCHAR(str, *len, size, ' ');
    padlen--;
  }
  while (*value != '\0' && (noprecision || precision-- > 0)) {
    OUTCHAR(str, *len, size, *value);
    value++;
  }
  while (padlen < 0) {  /* Trailing spaces. */
    OUTCHAR(str, *len, size, ' ');
    padlen++;
  }
}

// spadj:0x44 + reg:0x24 bytes of stack
static void fmtint(char *str, size_t *len, size_t size, INTMAX_T value, int base, int width,
                   int precision, int flags) {
  UINTMAX_T uvalue;
  char iconvert[MAX_CONVERT_LENGTH];
  char sign = '\0';
  char hexprefix = '\0';
  int spadlen = 0;  /* Amount to space pad. */
  int zpadlen = 0;  /* Amount to zero pad. */
  int pos;
  bool noprecision = (precision == -1);

  if (flags & PRINT_F_UNSIGNED) {
    uvalue = value;
  } else {
    uvalue = (value >= 0) ? value : -value;
    if (value < 0) {
      sign = '-';
    } else if (flags & PRINT_F_PLUS) { /* Do a sign. */
      sign = '+';
    } else if (flags & PRINT_F_SPACE) {
      sign = ' ';
    }
  }

  pos = prv_convert(uvalue, iconvert, sizeof(iconvert), base, flags & PRINT_F_UP);

  // amarshall: modified to support zero precision with octal alternate form
  if (flags & PRINT_F_NUM) {
    /*
     * C99 says: "The result is converted to an `alternative form'.
     * For `o' conversion, it increases the precision, if and only
     * if necessary, to force the first digit of the result to be a
     * zero (if the value and precision are both 0, a single 0 is
     * printed).  For `x' (or `X') conversion, a nonzero result has
     * `0x' (or `0X') prefixed to it." (7.19.6.1, 6)
     */
    switch (base) {
      case 8:
        if (precision == 0 && uvalue == 0) {
          precision = 1;
        } else if (uvalue != 0 && precision <= pos) {
          precision = pos + 1;
        }
        break;
      case 16:
        if (uvalue != 0) {
          hexprefix = (flags & PRINT_F_UP) ? 'X' : 'x';
        }
        break;
    }
  }

  zpadlen = precision - pos;
  if (zpadlen < 0) {
    zpadlen = 0;
  }

  spadlen = width                         /* Minimum field width. */
      - MAX(precision, pos)               /* Number of integer digits. */
      - ((sign != 0) ? 1 : 0)             /* Will we print a sign? */
      - ((hexprefix != 0) ? 2 : 0);       /* Will we print a prefix? */
  if (spadlen < 0) {
    spadlen = 0;
  }

  /*
   * C99 says: "If the `0' and `-' flags both appear, the `0' flag is
   * ignored.  For `d', `i', `o', `u', `x', and `X' conversions, if a
   * precision is specified, the `0' flag is ignored." (7.19.6.1, 6)
   */
  if (flags & PRINT_F_MINUS) { /* Left justify. */
    spadlen = -spadlen;
  } else if (flags & PRINT_F_ZERO && noprecision) {
    zpadlen += spadlen;
    spadlen = 0;
  }
  while (spadlen > 0) { /* Leading spaces. */
    OUTCHAR(str, *len, size, ' ');
    spadlen--;
  }
  if (sign != '\0') { /* Sign. */
    OUTCHAR(str, *len, size, sign);
  }
  if (hexprefix != '\0') { /* A "0x" or "0X" prefix. */
    OUTCHAR(str, *len, size, '0');
    OUTCHAR(str, *len, size, hexprefix);
  }
  while (zpadlen > 0) { /* Leading zeros. */
    OUTCHAR(str, *len, size, '0');
    zpadlen--;
  }
  // amarshall: Support zero precision
  if (uvalue == 0 && precision == 0) {
    pos = 0;
  }
  while (pos > 0) { /* The actual digits. */
    pos--;
    OUTCHAR(str, *len, size, iconvert[pos]);
  }
  while (spadlen < 0) { /* Trailing spaces. */
    OUTCHAR(str, *len, size, ' ');
    spadlen++;
  }
}

// spadj:0x28 + reg:0x20 bytes of stack
int vsnprintf(char *str, size_t size, const char *format, va_list args) {
  INTMAX_T value;
  unsigned char cvalue;
  const char *strvalue;
  size_t len = 0;
  int base = 0;
  int cflags = 0;
  int flags = 0;
  int width = 0;
  int precision = -1;
  int state = PRINT_S_DEFAULT;
  char ch = *format++;

  /*
   * C99 says: "If `n' is zero, nothing is written, and `s' may be a null
   * pointer." (7.19.6.5, 2)  We're forgiving and allow a NULL pointer
   * even if a size larger than zero was specified.  At least NetBSD's
   * snprintf(3) does the same, as well as other versions of this file.
   * (Though some of these versions will write to a non-NULL buffer even
   * if a size of zero was specified, which violates the standard.)
   */
  if (str == NULL && size != 0) {
    size = 0;
  }
  if (size > 0) {
    str[0] = '\0';
  }

  while (ch != '\0') {
    switch (state) {
      case PRINT_S_DEFAULT:
        if (ch == '%') {
          state = PRINT_S_FLAGS;
          // amarshall: moved these up here to make maintaining the state machine code easier.
          base = cflags = flags = width = 0;
          precision = -1;
        } else {
          OUTCHAR(str, len, size, ch);
        }
        ch = *format++;
        break;
      case PRINT_S_FLAGS:
        switch (ch) {
          case '-':
            flags |= PRINT_F_MINUS;
            break;
          case '+':
            flags |= PRINT_F_PLUS;
            break;
          case ' ':
            flags |= PRINT_F_SPACE;
            break;
          case '#':
            flags |= PRINT_F_NUM;
            break;
          case '0':
            flags |= PRINT_F_ZERO;
            break;
          default:
            state = PRINT_S_WIDTH;
            break;
        }
        if (state != PRINT_S_WIDTH) {
          ch = *format++;
        }
        break;
      case PRINT_S_WIDTH:
        if (ISDIGIT(ch)) {
          ch = CHARTOINT(ch);
          if (width > (INT_MAX - ch) / 10) {
            // amarshall: simplified error handling
            return -1;
          }
          width = 10 * width + ch;
          ch = *format++;
        } else if (ch == '*') {
          /*
           * C99 says: "A negative field width argument is
           * taken as a `-' flag followed by a positive
           * field width." (7.19.6.1, 5)
           */
          if ((width = va_arg(args, int)) < 0) {
            flags |= PRINT_F_MINUS;
            width = -width;
          }
          ch = *format++;
          state = PRINT_S_DOT;
        } else {
          state = PRINT_S_DOT;
        }
        break;
      case PRINT_S_DOT:
        if (ch == '.') {
          state = PRINT_S_PRECISION;
          ch = *format++;
        } else {
          state = PRINT_S_MOD;
        }
        break;
      case PRINT_S_PRECISION:
        if (precision == -1) {
          precision = 0;
        }
        if (ISDIGIT(ch)) {
          ch = CHARTOINT(ch);
          if (precision > (INT_MAX - ch) / 10) {
            // amarshall: simplified error handling
            return -1;
          }
          precision = 10 * precision + ch;
          ch = *format++;
        } else if (ch == '*') {
          /*
           * C99 says: "A negative precision argument is
           * taken as if the precision were omitted."
           * (7.19.6.1, 5)
           */
          if ((precision = va_arg(args, int)) < 0) {
            precision = 0; // amarshall: It actually means as if you did '%.d', not '%d'
          }
          ch = *format++;
          state = PRINT_S_MOD;
        } else if (ch == '-') { // amarshall: added support for negative immediates in precision
          /*
           * C99 says: "A negative precision argument is
           * taken as if the precision were omitted."
           * (7.19.6.1, 5)
           */
          precision = 0;
          while (isdigit(ch = *format++)) {}
          state = PRINT_S_MOD;
        } else {
          state = PRINT_S_MOD;
        }
        break;
      case PRINT_S_MOD:
        switch (ch) {
          case 'h':
            ch = *format++;
            if (ch == 'h') { // hh
              ch = *format++;
              cflags = PRINT_C_CHAR;
            } else {
              cflags = PRINT_C_SHORT;
            }
            break;
          case 'l':
            ch = *format++;
            if (ch == 'l') { // ll
              ch = *format++;
              cflags = PRINT_C_LLONG;
            } else {
              cflags = PRINT_C_LONG;
            }
            break;
          case 'j':
            cflags = PRINT_C_INTMAX;
            ch = *format++;
            break;
          case 't':
            cflags = PRINT_C_PTRDIFF;
            ch = *format++;
            break;
          case 'z':
            cflags = PRINT_C_SIZE;
            ch = *format++;
            break;
        }
        state = PRINT_S_CONV;
        break;
      case PRINT_S_CONV:
        switch (ch) {
          case 'd':
            /* FALLTHROUGH */
          case 'i':
            /* amarshall: Changed to machine-independent types
             * This will not affect native builds, useful for unit tests.
             */
            switch (cflags) {
              case PRINT_C_CHAR:
                value = (int8_t)va_arg(args, int);
                break;
              case PRINT_C_SHORT:
                value = (int16_t)va_arg(args, int);
                break;
              case PRINT_C_LONG:
                value = (int32_t)va_arg(args, int32_t);
                break;
              case PRINT_C_LLONG:
                value = (LLONG)va_arg(args, LLONG);
                break;
              case PRINT_C_SIZE:
                value = (SSIZE_T)va_arg(args, SSIZE_T);
                break;
              case PRINT_C_INTMAX:
                value = (INTMAX_T)va_arg(args, INTMAX_T);
                break;
              case PRINT_C_PTRDIFF:
                value = (PTRDIFF_T)va_arg(args, PTRDIFF_T);
                break;
              default:
                value = (int32_t)va_arg(args, int);
                break;
            }
            fmtint(str, &len, size, value, 10, width,
                   precision, flags);
            break;
          case 'X':
            flags |= PRINT_F_UP;
            /* FALLTHROUGH */
          case 'x':
            base = 16;
            /* FALLTHROUGH */
          case 'o':
            if (base == 0)
              base = 8;
            /* FALLTHROUGH */
          case 'u':
            if (base == 0)
              base = 10;
            flags |= PRINT_F_UNSIGNED;
            /* amarshall: Changed to machine-independent types
             * This will not affect native builds, useful for unit tests.
             */
            switch (cflags) {
              case PRINT_C_CHAR:
                value = (uint8_t)va_arg(args, unsigned int);
                break;
              case PRINT_C_SHORT:
                value = (uint16_t)va_arg(args, unsigned int);
                break;
              case PRINT_C_LONG:
                value = (uint32_t)va_arg(args, uint32_t);
                break;
              case PRINT_C_LLONG:
                value = (ULLONG)va_arg(args, ULLONG);
                break;
              case PRINT_C_SIZE:
                value = (SIZE_T)va_arg(args, SIZE_T);
                break;
              case PRINT_C_INTMAX:
                value = (UINTMAX_T)va_arg(args, UINTMAX_T);
                break;
              case PRINT_C_PTRDIFF:
                value = (UPTRDIFF_T)va_arg(args, UPTRDIFF_T);
                break;
              default:
                value = (uint32_t)va_arg(args, unsigned int);
                break;
            }
            fmtint(str, &len, size, value, base, width,
                   precision, flags);
            break;
          case 'c':
            while (width > 1) { // Leading spaces
              OUTCHAR(str, len, size, ' ');
              width--;
            }
            cvalue = va_arg(args, int);
            OUTCHAR(str, len, size, cvalue);
            break;
          case 's':
            strvalue = va_arg(args, char *);
            fmtstr(str, &len, size, strvalue, width,
                   precision, flags);
            break;
          case 'p':
            /*
             * C99 says: "The value of the pointer is
             * converted to a sequence of printing
             * characters, in an implementation-defined
             * manner." (C99: 7.19.6.1, 8)
             */
            strvalue = va_arg(args, void *);
            // amarshall: Changed this to act like newlib (where it's basically %#x)
            flags |= PRINT_F_NUM;
            flags |= PRINT_F_UNSIGNED;
            fmtint(str, &len, size,
                   (UINTPTR_T)strvalue, 16, width,
                   precision, flags);
            break;
          case 'n':
            /* amarshall: Changed to machine-independent types
             * This will not affect native builds, useful for unit tests.
             */
            // amarshall: Removed pointer variables to try to improve stack footprint
            switch (cflags) {
              case PRINT_C_CHAR:
                *(int8_t*)va_arg(args, int8_t*) = len;
                break;
              case PRINT_C_SHORT:
                *(int16_t*)va_arg(args, int16_t*) = len;
                break;
              case PRINT_C_LONG:
                *(int32_t*)va_arg(args, int32_t*) = len;
                break;
              case PRINT_C_LLONG:
                *(LLONG*)va_arg(args, LLONG*) = len;
                break;
              case PRINT_C_SIZE:
                /*
                 * C99 says that with the "z" length
                 * modifier, "a following `n' conversion
                 * specifier applies to a pointer to a
                 * signed integer type corresponding to
                 * size_t argument." (7.19.6.1, 7)
                 */
                *(SSIZE_T*)va_arg(args, SSIZE_T*) = len;
                break;
              case PRINT_C_INTMAX:
                *(INTMAX_T*)va_arg(args, INTMAX_T*) = len;
                break;
              case PRINT_C_PTRDIFF:
                *(PTRDIFF_T*)va_arg(args, PTRDIFF_T*) = len;
                break;
              default:
                *(int32_t*)va_arg(args, int32_t*) = len;
                break;
            }
            break;
          case '%': // Print a "%" character verbatim.
          default: // amarshall: newlib's behavior is to just print the character.
            OUTCHAR(str, len, size, ch);
            break;
        }
        ch = *format++;
        state = PRINT_S_DEFAULT;
        break;
    }
  }
  if (len < size) {
    str[len] = '\0';
  } else if (size > 0) {
    str[size - 1] = '\0';
  }

  if (len >= INT_MAX) {
    return -1;
  }
  return (int)len;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Additional sprintf variants

int vsprintf(char *buf, const char *fmt, va_list args) {
  return vsnprintf(buf, INT_MAX, fmt, args);
}

int snprintf(char *dst, size_t size, const char *fmt, ...) {
  int ret;
  va_list arg;
  va_start(arg, fmt);
  ret = vsnprintf(dst, size, fmt, arg);
  va_end(arg);
  return ret;
}

int sprintf(char *dst, const char *fmt, ...) {
  int ret;
  va_list arg;
  va_start(arg, fmt);
  ret = vsprintf(dst, fmt, arg);
  va_end(arg);
  return ret;
}
