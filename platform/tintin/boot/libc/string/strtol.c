/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   long int strtol(const char *nptr, char **endptr, int base);

#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdbool.h>

intmax_t strtoX_core(const char * restrict nptr, char ** restrict endptr, int base, bool do_errors,
                     intmax_t max, intmax_t min) {
  intmax_t value = 0;
  int sign = 1;
  while (isspace((int)*nptr)) {
    nptr++;
  }
  switch (*nptr) {
    case '+': sign =  1; nptr++; break;
    case '-': sign = -1; nptr++; break;
  }
  if (nptr[0] == '0' && nptr[1] == 'x' && (base == 0 || base == 16)) {
    base = 16;
    nptr += 2;
  } else if (nptr[0] == '0' && (base == 0 || base == 8)) {
    base = 8;
    nptr++;
  } else if (base == 0) {
    base = 10;
  }
  // We break on '\0' anyways
  for (;;) {
    char ch = *nptr;
    if (ch >= '0' && ch <= '9') {
      ch = ch - '0';
    } else if (ch >= 'A' && ch <= 'Z') {
      ch = ch - 'A' + 10;
    } else if (ch >= 'a' && ch <= 'z') {
      ch = ch - 'a' + 10;
    } else { // This will catch '\0'
      break;
    }
    if (ch >= base) {
      break;
    }
    value *= base;
    value += ch;
    if (do_errors) {
      if ((sign > 0) && (value > max)) {
        value = max;
      } else if ((sign < 0) && (-value < min)) {
        value = -min;
      }
    }
    nptr++;
  }
  if (endptr) {
    *endptr = (char*)nptr;
  }
  return value * sign;
}

long int strtol(const char * restrict nptr, char ** restrict endptr, int base) {
  return strtoX_core(nptr, endptr, base, true, INT_MAX, INT_MIN);
}
