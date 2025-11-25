/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdlib.h>
#include <string.h>

static void* applib_zalloc(size_t size) {
  void* result = malloc(size);
  if (result) {
    memset(result, 0, size);
  }
  return result;
}

#define applib_type_zalloc(Type) applib_zalloc(sizeof(Type))
#define applib_type_malloc(Type) malloc(sizeof(Type))
#define applib_type_size(Type) sizeof(Type)
#define applib_malloc(size) malloc(size)
#define applib_free(ptr) free(ptr)
