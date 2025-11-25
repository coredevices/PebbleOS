/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Rename (and re-define) the libc functions
#if UNITTEST
#include <stddef.h>
#include <stdarg.h>

# define PBLIBC(fn) pblibc_##fn

// String functions
# undef memcmp
# define memcmp PBLIBC(memcmp)
# undef memcpy
# define memcpy PBLIBC(memcpy)
# undef memmove
# define memmove PBLIBC(memmove)
# undef memset
# define memset PBLIBC(memset)
# undef memchr
# define memchr PBLIBC(memchr)
# undef atoi
# define atoi PBLIBC(atoi)
# undef atol
# define atol PBLIBC(atol)
# undef strtol
# define strtol PBLIBC(strtol)
# undef strcat
# define strcat PBLIBC(strcat)
# undef strncat
# define strncat PBLIBC(strncat)
# undef strlen
# define strlen PBLIBC(strlen)
# undef strnlen
# define strnlen PBLIBC(strnlen)
# undef strcpy
# define strcpy PBLIBC(strcpy)
# undef strncpy
# define strncpy PBLIBC(strncpy)
# undef strcmp
# define strcmp PBLIBC(strcmp)
# undef strncmp
# define strncmp PBLIBC(strncmp)
# undef strchr
# define strchr PBLIBC(strchr)
# undef strrchr
# define strrchr PBLIBC(strrchr)
# undef strcspn
# define strcspn PBLIBC(strcspn)
# undef strspn
# define strspn PBLIBC(strspn)
# undef strstr
# define strstr PBLIBC(strstr)

int memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *s1, const void *s2, size_t n);
void *memmove(void *s1, const void *s2, size_t n);
void *memset(void *s, int c, size_t n);
void *memchr(const void *s, int c, size_t n);
int atoi(const char *nptr);
long int atol(const char *nptr);
long int strtol(const char *nptr, char **endptr, int base);
char *strcat(char *s1, const char *s2);
char *strncat(char *s1, const char *s2, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
char *strcpy(char *s1, const char *s2);
char *strncpy(char *s1, const char *s2, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
size_t strcspn(const char *s1, const char *s2);
size_t strspn(const char *s1, const char *s2);
char *strstr(const char *s1, const char *s2);


// Math functions
# undef floor
# define floor PBLIBC(floor)
# undef log
# define log PBLIBC(log)
# undef pow
# define pow PBLIBC(pow)
# undef scalbn
# define scalbn PBLIBC(scalbn)
# undef sqrt
# define sqrt PBLIBC(sqrt)

double floor(double x);
double log(double x);
double pow(double x, double y);
double round(double d);
double scalbn(double x, int n);
double sqrt(double x);


// ctype
# undef __ctype_lookup
# undef isalpha
# undef isupper
# undef islower
# undef isdigit
# undef isxdigit
# undef isspace
# undef ispunct
# undef isalnum
# undef isprint
# undef isgraph
# undef iscntrl

# undef isascii
# undef toascii

# undef toupper
# undef tolower


// printf
# undef vsprintf
# define vsprintf PBLIBC(vsprintf)
# undef vsnprintf
# define vsnprintf PBLIBC(vsnprintf)
# undef sprintf
# define sprintf PBLIBC(sprintf)
# undef snprintf
# define snprintf PBLIBC(snprintf)

int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vsprintf(char *str, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

#endif
