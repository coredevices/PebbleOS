/* SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef DEFS_H
#define DEFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * Attributes
 */
#define __attr_unused___   __attribute__((unused))
#define __attr_used___     __attribute__((used))
#define __attr_noreturn___ __attribute__((noreturn))
#define __attr_noinline___ __attribute__((noinline))

#endif /* !DEFS_H */
