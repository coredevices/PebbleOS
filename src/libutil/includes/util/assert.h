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

#include "util/attributes.h"
#include "util/likely.h"

#ifndef __FILE_NAME__
#ifdef __FILE_NAME_LEGACY__
#define __FILE_NAME__ __FILE_NAME_LEGACY__
#else
#define __FILE_NAME__ __FILE__
#endif
#endif

NORETURN util_assertion_failed(const char *filename, int line);

#define UTIL_ASSERT(expr) \
  do { \
    if (UNLIKELY(!(expr))) { \
      util_assertion_failed(__FILE_NAME__, __LINE__); \
    } \
  } while (0)
