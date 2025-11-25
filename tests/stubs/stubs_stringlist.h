/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"
#include "util/stringlist.h"

int WEAK string_list_add_string(StringList *list, size_t max_list_size, const char *str,
                                size_t max_str_size) {
  return 0;
}

size_t WEAK string_list_count(StringList *list) {
  return 0;
}

char * WEAK string_list_get_at(StringList *list, size_t index) {
  return NULL;
}
