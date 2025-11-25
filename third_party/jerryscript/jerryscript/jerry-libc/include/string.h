/* SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef JERRY_LIBC_STRING_H
#define JERRY_LIBC_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

void *memcpy (void *dest, const void *src, size_t n);
void *memset (void *s, int c, size_t n);
void *memmove (void *dest, const void *src, size_t n);
int memcmp (const void *s1, const void *s2, size_t n);
int strcmp (const char *s1, const char *s2);
int strncmp (const char *s1, const char *s2, size_t n);
char *strncpy (char *dest, const char *src, size_t n);
size_t strlen (const char *s);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRY_LIBC_STRING_H */
