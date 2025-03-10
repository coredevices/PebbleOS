/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
