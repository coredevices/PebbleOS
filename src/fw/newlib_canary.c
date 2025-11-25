/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <_ansi.h>

// Safety check to ensure we're not including newlib headers.

#if defined(__NEWLIB_H__)
#error "Newlib headers being included rather than pblibc"
#endif
