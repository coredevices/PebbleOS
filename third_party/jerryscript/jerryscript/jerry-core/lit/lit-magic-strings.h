/* SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef LIT_MAGIC_STRINGS_H
#define LIT_MAGIC_STRINGS_H

#include "lit-globals.h"

/**
 * Limit for magic string length
 */
#define LIT_MAGIC_STRING_LENGTH_LIMIT 32

/**
 * Identifiers of ECMA and implementation-defined magic string constants
 */
typedef enum
{
#define LIT_MAGIC_STRING_DEF(id, ascii_zt_string) \
     id,
#include "lit-magic-strings.inc.h"
#undef LIT_MAGIC_STRING_DEF

  LIT_MAGIC_STRING__COUNT, /**< number of magic strings */
  LIT_MAGIC_STRING__FORCE_LARGE = INT32_MAX,
} lit_magic_string_id_t;

/**
 * Identifiers of implementation-defined external magic string constants
 */
typedef uint32_t lit_magic_string_ex_id_t;

extern uint32_t lit_get_magic_string_ex_count (void);

extern const lit_utf8_byte_t *lit_get_magic_string_utf8 (lit_magic_string_id_t);
extern lit_utf8_size_t lit_get_magic_string_size (lit_magic_string_id_t);

extern const lit_utf8_byte_t *lit_get_magic_string_ex_utf8 (lit_magic_string_ex_id_t);
extern lit_utf8_size_t lit_get_magic_string_ex_size (lit_magic_string_ex_id_t);

extern void lit_magic_strings_ex_set (const lit_utf8_byte_t **, uint32_t, const lit_utf8_size_t *);

extern bool lit_is_utf8_string_magic (const lit_utf8_byte_t *, lit_utf8_size_t, lit_magic_string_id_t *);
extern bool lit_is_ex_utf8_string_magic (const lit_utf8_byte_t *, lit_utf8_size_t, lit_magic_string_ex_id_t *);

extern bool lit_compare_utf8_string_and_magic_string (const lit_utf8_byte_t *, lit_utf8_size_t,
                                                      lit_magic_string_id_t);

extern bool lit_compare_utf8_string_and_magic_string_ex (const lit_utf8_byte_t *, lit_utf8_size_t,
                                                         lit_magic_string_ex_id_t);

extern lit_utf8_byte_t *lit_copy_magic_string_to_buffer (lit_magic_string_id_t, lit_utf8_byte_t *, lit_utf8_size_t);

#endif /* !LIT_MAGIC_STRINGS_H */
