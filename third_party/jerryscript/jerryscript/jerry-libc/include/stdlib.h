/* SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef JERRY_LIBC_STDLIB_H
#define JERRY_LIBC_STDLIB_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/**
 * Maximum integer that could be returned by random number generator
 *
 * See also:
 *          rand
 */
#define RAND_MAX (0x7fffffffu)

void __attribute__ ((noreturn)) exit (int);
void __attribute__ ((noreturn)) abort (void);
int rand (void);
void srand (unsigned int);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRY_LIBC_STDLIB_H */
