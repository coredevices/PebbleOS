/* SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef JERRY_LIBC_STDIO_H
#define JERRY_LIBC_STDIO_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/**
 * File descriptor type
 */
typedef void FILE;

/**
 * Standard file descriptors
 */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/**
 * I/O routines
 */
int vfprintf (FILE *stream, const char *format, va_list ap);
FILE *fopen (const char *path, const char *mode);
int fclose (FILE *fp);
size_t fread (void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite (const void *ptr, size_t size, size_t nmemb, FILE *stream);
int printf (const char *format, ...);
int fprintf (FILE *stream, const char *format, ...);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRY_LIBC_STDIO_H */
