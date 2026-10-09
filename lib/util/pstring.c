/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <string.h>

#include <pbl/util/pstring.h>

void pbl_pstring16_to_cstring(const struct pbl_pstring16 *pstring, char *string_out) {
  memcpy(string_out, pstring->str_value, pstring->str_length);
  string_out[pstring->str_length] = '\0';
}

bool pbl_pstring16_equal(const struct pbl_pstring16 *ps1, const struct pbl_pstring16 *ps2) {
  return ps1 && ps2 && (ps1->str_length == ps2->str_length) &&
         (memcmp(ps1->str_value, ps2->str_value, ps1->str_length) == 0);
}

bool pbl_pstring16_equal_cstring(const struct pbl_pstring16 *pstr, const char *cstr) {
  return pstr && cstr && (pstr->str_length == strlen(cstr)) &&
         (memcmp(pstr->str_value, cstr, pstr->str_length) == 0);
}

static uint16_t prv_entry_length(const uint8_t *entry) {
  uint16_t length;
  memcpy(&length, entry, sizeof(length));
  return length;
}

static bool prv_has_entry(const struct pbl_serialized_array *array, const uint8_t *cursor) {
  return (&array->data[array->data_size] - cursor) >= (ptrdiff_t)sizeof(uint16_t);
}

void pbl_pstring16_list_init(struct pbl_pstring16_list *list,
                             struct pbl_serialized_array *serialized_array) {
  list->pstrings = serialized_array;

  uint16_t count = 0;
  uint16_t trailing_empty = 0;
  const uint8_t *cursor = serialized_array->data;
  while (prv_has_entry(serialized_array, cursor)) {
    const uint16_t length = prv_entry_length(cursor);
    if (length == 0) {
      trailing_empty++;
    } else {
      count += trailing_empty + 1;
      trailing_empty = 0;
    }
    cursor += sizeof(uint16_t) + length;
  }
  if (count != 0) {
    count += trailing_empty;
  }
  list->count = count;
}

struct pbl_pstring16 *pbl_pstring16_list_get(const struct pbl_pstring16_list *list,
                                             uint16_t index) {
  if (index >= list->count) {
    return nullptr;
  }

  uint8_t *cursor = list->pstrings->data;
  for (uint16_t i = 0; i < index; i++) {
    if (!prv_has_entry(list->pstrings, cursor)) {
      return nullptr;
    }
    cursor += sizeof(uint16_t) + prv_entry_length(cursor);
  }
  return prv_has_entry(list->pstrings, cursor) ? (struct pbl_pstring16 *)cursor : nullptr;
}
