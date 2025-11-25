/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

// For some reason the string.h that newlib generates doesn't have this definition,
// but if you just extern this the libc still has the right symbol. Weirdddd.
size_t strnlen(const char *, size_t);

const char *string_strip_leading_whitespace(const char * string);

void string_strip_trailing_whitespace(const char *string, char *string_out);

#define IS_EMPTY_STRING(s) (s[0] == '\0')

// Stolen from http://stackoverflow.com/a/8488201
#define GET_FILE_NAME(file) (strrchr(file, '/') ? (strrchr(file, '/') + 1) : (file))

//! Converts an unsigned integer value to a null-terminated hex-value string and stores the result
//! in buffer.
void itoa(uint32_t num, char *buffer, int buffer_length);

//! Converts a signed integer value to a null-terminated string and stores the result in buffer.
//! NOTE: Buffer must be long enough to fit a string 12 bytes long.
void itoa_int(int n, char *str, int base);

//! Reverses a string in place
void string_reverse(char *str);

uintptr_t str_to_address(const char *address_str);

const char *bool_to_str(bool b);

//! @param hex 12-digit hex string representing a BT address
//! @param addr Points to a SS1 BD_ADDR_t as defined in BTBTypes.h
//! @return True on success
bool convert_bt_addr_hex_str_to_bd_addr(const char *hex_str, uint8_t *bd_addr, const unsigned int bd_addr_size);

//! Concatenates a simple string and a number.
//! NOTE: Buffer must be long enough to fit the largest number value (12 bytes) and the string, plus
//! A null-terminated character
void concat_str_int(const char *str, uint32_t num, char *buf, uint8_t buf_len);

//! Convert an ASCII string to uppercase
void toupper_str(char *str);

//! Converts a byte stream to a hex string, i.e ({0xaa, 0xbb, 0xcc} -> "aabbcc")
void byte_stream_to_hex_string(char *out_buf, size_t out_buf_len,
    const uint8_t *byte_stream, size_t byte_stream_len, bool stream_backward);

//! Appends the src string to dst, taking the overall size of the dst buffer into account
//! @param dst Destination string to append to
//! @param src String to append
//! @param dst_space Total size of dst buffer
void safe_strcat(char* dst, const char* src, int dst_space);
