/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

/**
 * @defgroup util_string Strings
 * @ingroup util
 * @brief String helpers.
 * @{
 */

/**
 * @brief POSIX strnlen(), declared here because newlib's string.h does not always declare it.
 *
 * @return Length of the string, at most the given maximum.
 */
size_t strnlen(const char *, size_t);

/**
 * @brief Skip leading spaces and newlines.
 *
 * @param string String.
 * @return First character of @p string that is not a space or newline.
 */
const char *string_strip_leading_whitespace(const char *string);

/**
 * @brief Copy a string without its trailing spaces and newlines.
 *
 * @param string String.
 * @param[out] string_out Destination, at least <tt>strlen(string) + 1</tt> bytes.
 */
void string_strip_trailing_whitespace(const char *string, char *string_out);

/**
 * @brief Check whether a string is empty.
 *
 * @param s String, not NULL.
 */
#define IS_EMPTY_STRING(s) (s[0] == '\0')

/**
 * @brief Get the file name part of a path.
 *
 * @param file Path.
 * @return Part of @p file after its last '/', or @p file itself.
 */
#define GET_FILE_NAME(file) (strrchr(file, '/') ? (strrchr(file, '/') + 1) : (file))

/**
 * @brief Format an unsigned integer as "0x" followed by 8 hex digits.
 *
 * @param num Value.
 * @param[out] buffer Destination, untouched if smaller than 11 bytes.
 * @param buffer_length Size of @p buffer.
 */
void string_itoa(uint32_t num, char *buffer, int buffer_length);

/**
 * @brief Format a signed integer.
 *
 * @param n Value.
 * @param[out] str Destination, at least 12 bytes for base 10.
 * @param base Base, 2 to 10.
 */
void itoa_int(int n, char *str, int base);

/**
 * @brief Reverse a string in place.
 *
 * @param[in,out] str String, shorter than 128 characters.
 */
void string_reverse(char *str);

/**
 * @brief Parse an address.
 *
 * @param address_str Address, in decimal, hex (0x prefix) or octal (0 prefix).
 * @return Address, or @c (uintptr_t)-1 if @p address_str has trailing characters.
 */
uintptr_t str_to_address(const char *address_str);

/**
 * @brief Format a boolean.
 *
 * @param b Value.
 * @return "yes" or "no".
 */
const char *bool_to_str(bool b);

/**
 * @brief Parse a Bluetooth address from a hex string.
 *
 * The string is in display order; the address is stored least significant byte first.
 *
 * @param hex_str 12 hex digits.
 * @param[out] bd_addr Address.
 * @param bd_addr_size Size of @p bd_addr in bytes.
 * @return true on success, false if @p hex_str is not 12 characters long.
 */
bool convert_bt_addr_hex_str_to_bd_addr(const char *hex_str, uint8_t *bd_addr,
                                        const unsigned int bd_addr_size);

/**
 * @brief Concatenate a string and a decimal number.
 *
 * @param str String.
 * @param num Number.
 * @param[out] buf Destination, truncated and NUL-terminated if too small.
 * @param buf_len Size of @p buf.
 */
void concat_str_int(const char *str, uint32_t num, char *buf, uint8_t buf_len);

/**
 * @brief Convert an ASCII string to uppercase in place.
 *
 * @param[in,out] str String.
 */
void toupper_str(char *str);

/**
 * @brief Format bytes as lowercase hex, e.g. {0xaa, 0xbb, 0xcc} as "aabbcc".
 *
 * Output stops when @p out_buf is full.
 *
 * @param[out] out_buf Destination, NUL-terminated if at least one byte is written.
 * @param out_buf_len Size of @p out_buf.
 * @param byte_stream Bytes.
 * @param byte_stream_len Number of bytes.
 * @param stream_backward Format the bytes from last to first.
 */
void byte_stream_to_hex_string(char *out_buf, size_t out_buf_len, const uint8_t *byte_stream,
                               size_t byte_stream_len, bool stream_backward);

/**
 * @brief Append a string, truncating to the size of the destination buffer.
 *
 * @param[in,out] dst String to append to.
 * @param src String to append.
 * @param dst_space Total size of @p dst in bytes.
 */
void safe_strcat(char *dst, const char *src, int dst_space);

/** @} */
