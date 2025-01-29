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

#include <include/alloca.h>

# define __alloca(size) __builtin_alloca (size)

void *alloca(size_t size) {
  /* This never worked to begin with, so let's at least make it obvious when
   * someone calls it (while we shut up the compiler warning).  */
  while (1);
  /*return __alloca(size);*/
}
