/*
 * Copyright (c) 2000 Citrus Project,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <inttypes.h>
#include <stddef.h>
#include "util/list.h"

#define ISO_LOCALE_LENGTH 6
#define LOCALE_NAME_LENGTH 30

typedef struct {
  ListNode node;              //!< Linked list node
  const void *owner;          //!< pointer to owner object
  uint32_t original_hash;     //!< hashed original string
  char *original_string;      //!< original string. Stored following translated_string below
  char translated_string[];   //!< i18n'ed string. Storage for original string comes after this
} I18nString;

//! macro used to tag strings for extractions. Needed when we
//! can't call i18n_get directly (i.e constant initializers)
#define i18n_noop(string) (string)

//! macro used to tag strings for extractions. Needed when we
//! can't call i18n_ctx_get directly (i.e constant initializers)
//! The resulting string should be used with i18n_get instead of i18n_ctx_get.
#define i18n_ctx_noop(ctx, string) (ctx "\4" string)

//! Look up and return i18n'ed string (or original string if not found)
//! Tags it as owned by owner
//! NOTE: Currently, we don't do reference counting, so bad things will happen if the caller
//!   calls i18n_get() on the same string more than once and assumes that any of those return
//!   pointers will still be valid after i18n_free() is called on one of them.
const char *i18n_get(const char *string, const void *owner);

#define i18n_ctx_get(ctx, string, owner) i18n_get(i18n_ctx_noop(ctx, string), owner)

//! Look up an i18n'ed string and copy it into a provided buffer.
void i18n_get_with_buffer(const char *string, char *buffer, size_t length);

#define i18n_ctx_get_with_buffer(ctx, string, buffer, length) \
            i18n_get_with_buffer(i18n_ctx_noop(ctx, string), buffer, length)

//! Look up an i18n'ed string and return the length of it.
size_t i18n_get_length(const char *string);

#define i18n_ctx_get_length(ctx, string) i18n_get_length(i18n_ctx_noop(ctx, string))

//! Free an i18n'ed string and it's associated metadata
void i18n_free(const char *string, const void *owner);

#define i18n_ctx_free(ctx, string, owner) i18n_free(i18n_ctx_noop(ctx, string), owner)

//! Free all i18n'ed strings associated with owner
void i18n_free_all(const void *owner);
void i18n_set_resource(uint32_t resource_id);

//! return the ISO language string for the currently installed language
char *i18n_get_locale(void);

//! return the version number for the currently installed language
uint16_t i18n_get_version(void);

char *i18n_get_lang_name(void);

void i18n_enable(bool enable);
