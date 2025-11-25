/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "stringlist.h"

#include "util/math.h"
#include "util/string.h"

#include <stdbool.h>
#include <string.h>

size_t string_list_count(StringList *list) {
  if (!list || list->serialized_byte_length == 0) {
    return 0;
  }

  size_t result = 1;
  for (int i = 0; i < list->serialized_byte_length; i++) {
    if (list->data[i] == '\0') {
      result++;
    }
  }
  return result;
}

char *string_list_get_at(StringList *list, size_t index) {
  if (!list) {
    return NULL;
  }

  char *ptr = list->data;
  const char *max_ptr = ptr + list->serialized_byte_length;
  while (index > 0 && ptr < max_ptr) {
    ptr += strlen(ptr) + 1;
    index--;
  }

  if (index > 0) {
    return NULL;
  } else {
    return ptr;
  }
}

int string_list_add_string(StringList *list, size_t max_list_size, const char *buffer,
                           size_t max_str_size) {
  if (!list) {
    return 0;
  }

  const size_t str_length = strnlen(buffer, max_str_size);
  const int size_remaining =
      max_list_size - (sizeof(StringList) + list->serialized_byte_length + 1);
  if (size_remaining <= 0) {
    return 0;
  }
  const bool has_last_terminator = (list->serialized_byte_length > 0);
  char *cursor = &list->data[list->serialized_byte_length + (has_last_terminator ? 1 : 0)];
  const size_t bytes_written = MIN(str_length, (size_t)size_remaining - 1);
  strncpy(cursor, buffer, bytes_written);
  cursor[bytes_written] = '\0';
  list->serialized_byte_length += (has_last_terminator ? 1 : 0) + bytes_written;
  return bytes_written;
}
