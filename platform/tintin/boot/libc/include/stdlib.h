/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif
#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif

// It's an int.
#ifndef RAND_MAX
# define RAND_MAX (0x7FFFFFFFL)
#endif

#define __need_size_t
#define __need_NULL
#include <stddef.h>

int abs(int j) __attribute__((__const__));
long labs(long j) __attribute__((__const__));

int atoi(const char *nptr) __attribute__((__pure__));
long int atol(const char *nptr) __attribute__((__pure__));
long int strtol(const char * restrict nptr, char ** restrict endptr, int base);
