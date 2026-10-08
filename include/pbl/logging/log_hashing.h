/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/logging/logging.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pbl/kernel/compiler.h>

/**
 * @defgroup logging_log_hashing Log hashing
 * @ingroup logging
 * @brief Compile-time replacement of log strings by tokens (@c CONFIG_LOG_HASHED).
 *
 * Each PBL_LOG call site places "file:line:level:color:format" in the @c .log_strings section,
 * which is linked at @ref LOG_STRINGS_SECTION_ADDRESS, outside the firmware image, and logs a
 * token instead of the string. The host turns tokens back into messages with the dictionary
 * extracted from the ELF file.
 *
 * Token layout:
 * - bits 31-29: number of format conversions, 0 to 7;
 * - bits 28-23: 1-based indices of up to two @c \%s conversions, 3 bits each, 0 for none;
 * - bits 22-20: level, 0 to 5 for @ref LOG_LEVEL_ALWAYS to @ref LOG_LEVEL_DEBUG_VERBOSE;
 * - bit 19: reserved;
 * - bits 18-0: offset of the string in @c .log_strings (512 KiB).
 *
 * The @c .log_strings section holds:
 * - @c .log_string.header: "NL\<major\>\<minor\>:\<offset mask\>=\<token list\>", the format
 *   version and description of the entries;
 * - @c .log_core_number: "CORE\<number\>", the core the strings belong to;
 * - @c .log_string: the strings of every call site, plus "MODULE:\<file\>:\<module\>" entries
 *   emitted by PBL_LOG_MODULE_DEFINE() and PBL_LOG_MODULE_DECLARE() that map files to log
 *   modules and are never referenced by a token.
 *
 * Limitations: at most 7 conversions per message, at most 2 of them @c \%s, without flags or
 * width; no @c \%\%; only arguments of 32 bits or less (see SPLIT_64_BIT_ARG()). Formats
 * breaking them are not detected at build time. The code must be built with @c -Os, or the
 * token computation is not folded.
 * @{
 */

/** @brief Version of the @c .log_strings format, major and minor on two digits each. */
#define NEW_LOG_VERSION "0102"

/** @brief Address the @c .log_strings section is linked at; tokens are offsets from it. */
#define LOG_STRINGS_SECTION_ADDRESS 0xC0000000

/** @brief Position of the core number in a message ID. */
#define PACKED_CORE_OFFSET 30 // 2 bits - Core number
/** @brief Mask of the core number, 2 bits. */
#define PACKED_CORE_MASK 0x03

/** @brief Position of the number of format conversions in a token. */
#define PACKED_NUM_FMT_OFFSET 29 // 3 bits - Number format conversions
/** @brief Mask of the number of format conversions, 3 bits. */
#define PACKED_NUM_FMT_MASK 0x07
/** @brief Position of the first string conversion index in a token. */
#define PACKED_STR1FMT_OFFSET 26 // 3 bits - indicies of string parameter 1 format conversion
/** @brief Mask of the first string conversion index, 3 bits. */
#define PACKED_STR1FMT_MASK 0x07
/** @brief Position of the second string conversion index in a token. */
#define PACKED_STR2FMT_OFFSET 23 // 3 bits - indicies of string parameter 2 format conversion
/** @brief Mask of the second string conversion index, 3 bits. */
#define PACKED_STR2FMT_MASK 0x07
/** @brief Position of both string conversion indices in a token. */
#define PACKED_STRFMTS_OFFSET 23 // 6 bits - indicies of string parameters 1 & 2.
/** @brief Mask of both string conversion indices, 6 bits. */
#define PACKED_STRFMTS_MASK 0x3f
/** @brief Position of the packed level in a token. */
#define PACKED_LEVEL_OFFSET 20 // 3 bits  - log level
/** @brief Mask of the packed level, 3 bits. */
#define PACKED_LEVEL_MASK 0x07
/** @brief Position of the string offset in a token. */
#define PACKED_HASH_OFFSET 0
/** @brief Mask of the string offset, 19 bits. */
#define PACKED_HASH_MASK 0x7FFFF // 19 bits - string table offset (512 KB)

/** @brief Mask of the string indices and string offset of a message ID. */
#define MSGID_STR_AND_HASH_MASK \
  ((PACKED_STRFMTS_MASK << PACKED_STRFMTS_OFFSET) | (PACKED_HASH_MASK << PACKED_HASH_OFFSET))
/** @brief Mask of the core number and string offset of a message ID. */
#define MSGID_CORE_AND_HASH_MASK \
  ((PACKED_CORE_MASK << PACKED_CORE_OFFSET) | (PACKED_HASH_MASK << PACKED_HASH_OFFSET))

#ifndef STRINGIFY
/**
 * @brief Turn the argument into a string literal without expanding it.
 *
 * @param a Argument.
 */
#define STRINGIFY_NX(a) #a
/**
 * @brief Turn the expansion of the argument into a string literal.
 *
 * @param a Argument.
 */
#define STRINGIFY(a) STRINGIFY_NX(a)
#endif // STRINGIFY

/** @cond INTERNAL_HIDDEN */
/* Printf Format argument checking.
 *
 * NB: it's critical that the 'if (false)' tag is included before the call to
 * PBL_LOG_x_printf_arg_check(). Without this obviously useless check, PBL_LOG_x_printf_arg_check()
 * would not be optimised out and cause a) a linker error (missing function body), b) take up
 * code space & time, and c) would cause the arguments passed to PBL_LOG (NEW_LOG_HASH) to be
 * evaluated twice. This is fine with normal parameters, but could result in macros or functions
 * being called twice and messing up globals in unexpected ways.
 */
void PBL_LOG_x_printf_arg_check(const char *fmt, ...) PBL_FORMAT_PRINTF(1, 2);

#define NEW_LOG_HASH(logfunc, level, color, fmt, ...)                                    \
  {                                                                                      \
    static const char str[] PBL_NOCOMMON PBL_SECTION(".log_strings") =                   \
        __FILE__ ":" STRINGIFY(__LINE__) ":" STRINGIFY(level) ":" color ":" fmt;         \
    logfunc((uint32_t)((uintptr_t)str + LOG_SECTION_OFFSET(level, fmt)), ##__VA_ARGS__); \
    if (0)                                                                               \
      PBL_LOG_x_printf_arg_check(fmt, ##__VA_ARGS__);                                    \
  }

PBL_ALWAYS_INLINE static uint32_t LOG_SECTION_OFFSET(const uint8_t level, const char *fmt) {
  const char *p1 = NULL, *p2 = NULL, *p3 = NULL, *p4 = NULL;
  const char *p5 = NULL, *p6 = NULL, *p7 = NULL, *p8 = NULL;
  const char *s1 = NULL, *s2 = NULL, *s3 = NULL, *s4 = NULL;
  const char *s5 = NULL, *s6 = NULL, *s7 = NULL;

  // Search for % characters in fmt. p1-p8 point to the character immediately succeeding the first
  // 8 % characters in fmt (or NULL, if there aren't 8 % characters in fmt).
  p1 = strchr(fmt, '%') ? (strchr(fmt, '%') + 1) : NULL;
  if (p1)
    p2 = strchr(p1, '%') ? (strchr(p1, '%') + 1) : NULL;
  if (p2)
    p3 = strchr(p2, '%') ? (strchr(p2, '%') + 1) : NULL;
  if (p3)
    p4 = strchr(p3, '%') ? (strchr(p3, '%') + 1) : NULL;
  if (p4)
    p5 = strchr(p4, '%') ? (strchr(p4, '%') + 1) : NULL;
  if (p5)
    p6 = strchr(p5, '%') ? (strchr(p5, '%') + 1) : NULL;
  if (p6)
    p7 = strchr(p6, '%') ? (strchr(p6, '%') + 1) : NULL;
  if (p7)
    p8 = strchr(p7, '%') ? (strchr(p7, '%') + 1) : NULL;

  // Check that fmt doesn't contain the escaped % symbol, '%%'. It's too hard to handle correctly
  // in every case.
  if ((p1 + 1 == p2) || (p2 + 1 == p3) || (p3 + 1 == p4) || (p4 + 1 == p5) || (p5 + 1 == p6) ||
      (p6 + 1 == p7) || (p7 + 1 == p8)) {
    return 0;
  }

  // Count number of valid pointers by bool-inversion-twice
  uint8_t num_params = !!p1 + !!p2 + !!p3 + !!p4 + !!p5 + !!p6 + !!p7 + !!p8;

  // Check that there aren't more than 7 format conversions. We have only 3 bits per string index.
  if (num_params > 7) {
    return 0;
  }

  // Search for an 's' character succeeding the % characters in fmt. s1-s7 point to the first 's'
  // charactres in fmt after the previously found % characters (or NULL if there aren't 7 's'
  // characters in fmt).
  if (p1)
    s1 = strchr(p1, 's');
  if (p2)
    s2 = strchr(p2, 's');
  if (p3)
    s3 = strchr(p3, 's');
  if (p4)
    s4 = strchr(p4, 's');
  if (p5)
    s5 = strchr(p5, 's');
  if (p6)
    s6 = strchr(p6, 's');
  if (p7)
    s7 = strchr(p7, 's');

  // See if the 's' characters immediately succeed the '%' characters. If so, set flag psX.
  const int ps1 = p1 ? (p1 == s1) : 0;
  const int ps2 = p2 ? (p2 == s2) : 0;
  const int ps3 = p3 ? (p3 == s3) : 0;
  const int ps4 = p4 ? (p4 == s4) : 0;
  const int ps5 = p5 ? (p5 == s5) : 0;
  const int ps6 = p6 ? (p6 == s6) : 0;
  const int ps7 = p7 ? (p7 == s7) : 0;

  // Count the number of '%s' parameters
  const int num_s_params = ps1 + ps2 + ps3 + ps4 + ps5 + ps6 + ps7;

  // We currently support only 2 string parameters.
  if (num_s_params > 2) {
    return 0;
  }

  // Format the (maximum) two string parameter indicies as:
  // (1-based index of first %s << 3) | (1-based index of second %s << 0)
  // If there is only one %s parameter, the index will be formatted as:
  // (1-based index of first %s << 0)
  const int a1 = ps1 ? 1 : 0;
  const int a2 = ps2 ? (a1 << 3) + 2 : a1;
  const int a3 = ps3 ? (a2 << 3) + 3 : a2;
  const int a4 = ps4 ? (a3 << 3) + 4 : a3;
  const int a5 = ps5 ? (a4 << 3) + 5 : a4;
  const int a6 = ps6 ? (a5 << 3) + 6 : a5;
  const int a7 = ps7 ? (a6 << 3) + 7 : a6;
  const int string_indicies = a7;

  // Convert level to packed_level
  int packed_level = LOG_LEVEL_ALWAYS;
  if (level == LOG_LEVEL_ERROR) {
    packed_level = 1;
  } else if (level == LOG_LEVEL_WARNING) {
    packed_level = 2;
  } else if (level == LOG_LEVEL_INFO) {
    packed_level = 3;
  } else if (level == LOG_LEVEL_DEBUG) {
    packed_level = 4;
  } else if (level == LOG_LEVEL_DEBUG_VERBOSE) {
    packed_level = 5;
  }

  const uint32_t offset = (((num_params & PACKED_NUM_FMT_MASK) << PACKED_NUM_FMT_OFFSET) |
                           ((packed_level & PACKED_LEVEL_MASK) << PACKED_LEVEL_OFFSET) |
                           ((string_indicies & PACKED_STRFMTS_MASK) << PACKED_STRFMTS_OFFSET));

  return (offset - LOG_STRINGS_SECTION_ADDRESS);
}
/** @endcond */

/** @} */
