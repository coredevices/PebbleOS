/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

//! A string list is a serialized array of NULL terminated strings
//! It is used to pass groups of strings as a single attribute on the wire.
//! E.g. canned responses for notifications
//! @note the serialized_byte_length does not include the last terminated byte

//! Calculate the maximum string list size given the number of values and their max length
#define StringListSize(num_values, max_value_size) \
    (sizeof(StringList) + ((num_values) * (max_value_size)))

typedef struct {
  uint16_t serialized_byte_length;
  char data[];
} StringList;

//! Retrieve a string from a string list
//! @param list a pointer to the string list
//! @param index of the desired string
//! @note string lists are zero indexed
//! @return a pointer to the start of the string, NULL if index out of bounds
char *string_list_get_at(StringList *list, size_t index);

//! Count the number of strings in a string list
//! @param list a pointer to the string list
//! @return the number of strings in a list
size_t string_list_count(StringList *list);

//! Adds a string to a string list
//! @param list a pointer to the string list
//! @param max_list_size the max size of the list (includes the header and last terminated byte)
//! @param str the string to add
//! @param max_str_size the string to add
//! @return the number of bytes written not including the null terminator
int string_list_add_string(StringList *list, size_t max_list_size, const char *str,
                           size_t max_str_size);
