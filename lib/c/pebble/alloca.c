/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <include/alloca.h>

# define __alloca(size) __builtin_alloca (size)

void *alloca(size_t size) {
  /* This never worked to begin with, so let's at least make it obvious when
   * someone calls it (while we shut up the compiler warning).  */
  while (1);
  /*return __alloca(size);*/
}
