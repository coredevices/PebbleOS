/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Safety check to ensure the Pebble libc build isn't accidentally pulling in
// newlib headers. Only meaningful when the in-tree Pebble libc is selected;
// other libcs (newlib, picolibc) legitimately use the toolchain headers.
#include <_ansi.h>

#if defined(__NEWLIB_H__)
#error "Newlib headers being included rather than pblibc"
#endif
