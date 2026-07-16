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

#if __clang__
// Default builtins break the clang build for some reason,
// and we don't have a real implementation of these functions
#define abs(x) __builtin_abs(x)
#define labs(x) __builtin_labs(x)
#endif

int atoi(const char *nptr) __attribute__((__pure__));
long int atol(const char *nptr) __attribute__((__pure__));
long int strtol(const char * restrict nptr, char ** restrict endptr, int base);

// Implemented in src/fw/util/rand/rand.c
int rand(void);
int rand_r(unsigned *seed);
void srand(unsigned seed);

void exit(int status) __attribute__((noreturn));

// Not implemented, but included in the header to build the default platform.c of libs.
void free(void *ptr);
void *malloc(size_t bytes);

long jrand48(unsigned short int s[3]);
