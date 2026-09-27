/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"
#include "util/stringlist.h"

int PBL_WEAK string_list_add_string(StringList *list, size_t max_list_size, const char *str,
                                    size_t max_str_size) {
  return 0;
}

size_t PBL_WEAK string_list_count(StringList *list) {
  return 0;
}

char *PBL_WEAK string_list_get_at(StringList *list, size_t index) {
  return NULL;
}
