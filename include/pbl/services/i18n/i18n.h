/* SPDX-FileCopyrightText: 2000 Citrus Project */
/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#include <inttypes.h>
#include <stddef.h>
#include "pbl/util/list.h"

/**
 * @defgroup services_i18n Internationalization
 * @ingroup services
 * @brief Translates firmware strings with the installed language pack.
 *
 * The language pack is a gettext MO file stored as a system resource. Strings are looked up by
 * their English original (the msgid); when no translation exists, or no language pack is
 * installed, the original is returned. A context can be attached to disambiguate identical
 * originals with the @c _ctx_ variants. Mark strings with i18n_noop() or i18n_ctx_noop() where
 * they cannot be translated in place, so they are still extracted for translation. Changing the
 * language puts a @c PEBBLE_LANGUAGE_CHANGE_EVENT.
 *
 * Translations returned by i18n_get() are cached per owner and stay valid until freed:
 *
 * @code{.c}
 * text_layer_set_text(&data->title, i18n_get("Settings", data));
 * ...
 * i18n_free_all(data);
 * @endcode
 *
 * For short-lived use, translate into a buffer instead:
 *
 * @code{.c}
 * char buf[32];
 * i18n_ctx_get_with_buffer("Alarm", "Snooze", buf, sizeof(buf));
 * @endcode
 * @{
 */

/** @brief Size of an ISO locale string such as "en_US", including the terminator. */
#define ISO_LOCALE_LENGTH 6
/** @brief Size of a language name buffer, including the terminator. */
#define LOCALE_NAME_LENGTH 30

/** @brief Cached translation, stored in a list per language pack. */
typedef struct {
  /** Linked list node. */
  ListNode node;
  /** Owner the translation was requested for. */
  const void *owner;
  /** Hash of the original string. */
  uint32_t original_hash;
  /** Original string, stored right after @ref translated_string. */
  char *original_string;
  /** Translated string, followed by the storage of the original string. */
  char translated_string[];
} I18nString;

/**
 * @brief Tag a string for extraction without translating it.
 *
 * For places where i18n_get() cannot be called, e.g. constant initializers. Translate the string
 * later with i18n_get().
 *
 * @param string String literal.
 */
#define i18n_noop(string) (string)

/**
 * @brief Tag a string with a context for extraction without translating it.
 *
 * For places where i18n_ctx_get() cannot be called, e.g. constant initializers. The result
 * embeds the context, so translate it later with i18n_get() rather than i18n_ctx_get().
 *
 * @param ctx Context string literal.
 * @param string String literal.
 */
#define i18n_ctx_noop(ctx, string) (ctx "\4" string)

/**
 * @brief Translate a string, caching the result for an owner.
 *
 * The translation is truncated to 199 characters. There is no reference counting: when the same
 * string is requested several times for the same owner, all returned pointers become invalid
 * once i18n_free() is called on any of them. A language change also invalidates them.
 *
 * @param string Original string, possibly with a context from i18n_ctx_noop().
 * @param owner Owner of the cached translation, must not be NULL.
 * @return Translation, or the original string without its context if there is none.
 */
const char *i18n_get(const char *string, const void *owner);

/**
 * @brief i18n_get() with a context.
 *
 * @param ctx Context string literal.
 * @param string Original string literal.
 * @param owner Owner of the cached translation.
 */
#define i18n_ctx_get(ctx, string, owner) i18n_get(i18n_ctx_noop(ctx, string), owner)

/**
 * @brief Translate a string into a buffer.
 *
 * Nothing is cached. The result is truncated to fit and always terminated, unless @p length is 0.
 *
 * @param string Original string, possibly with a context from i18n_ctx_noop().
 * @param[out] buffer Output buffer.
 * @param length Size of @p buffer in bytes.
 */
void i18n_get_with_buffer(const char *string, char *buffer, size_t length);

/**
 * @brief i18n_get_with_buffer() with a context.
 *
 * @param ctx Context string literal.
 * @param string Original string literal.
 * @param buffer Output buffer.
 * @param length Size of @p buffer in bytes.
 */
#define i18n_ctx_get_with_buffer(ctx, string, buffer, length) \
  i18n_get_with_buffer(i18n_ctx_noop(ctx, string), buffer, length)

/**
 * @brief Get the length of a translated string.
 *
 * @param string Original string, possibly with a context from i18n_ctx_noop().
 * @return Length of the translation without terminator, or of the original if there is none.
 */
size_t i18n_get_length(const char *string);

/**
 * @brief i18n_get_length() with a context.
 *
 * @param ctx Context string literal.
 * @param string Original string literal.
 */
#define i18n_ctx_get_length(ctx, string) i18n_get_length(i18n_ctx_noop(ctx, string))

/**
 * @brief Free a cached translation.
 *
 * @param string Original string passed to i18n_get().
 * @param owner Owner passed to i18n_get(), must not be NULL.
 */
void i18n_free(const char *string, const void *owner);

/**
 * @brief i18n_free() with a context.
 *
 * @param ctx Context string literal.
 * @param string Original string literal.
 * @param owner Owner passed to i18n_ctx_get().
 */
#define i18n_ctx_free(ctx, string, owner) i18n_free(i18n_ctx_noop(ctx, string), owner)

/**
 * @brief Free all cached translations of an owner.
 *
 * @param owner Owner passed to i18n_get().
 */
void i18n_free_all(const void *owner);

/**
 * @brief Select the system resource holding the language pack.
 *
 * The resource is watched, so installing a new language pack reloads it. If the user chose
 * English, the language pack is not loaded.
 *
 * @param resource_id System resource ID of the language pack.
 */
void i18n_set_resource(uint32_t resource_id);

/**
 * @brief Get the ISO locale of the installed language.
 *
 * @return Locale such as "en_US", "en_US" when no language pack is loaded.
 */
char *i18n_get_locale(void);

/**
 * @brief Get the version of the installed language pack.
 *
 * @return Language pack version, 1 when none is loaded.
 */
uint16_t i18n_get_version(void);

/**
 * @brief Get the name of the installed language.
 *
 * @return Language name, "English" when no language pack is loaded.
 */
char *i18n_get_lang_name(void);

/**
 * @brief Load or unload the language pack.
 *
 * @param enable true to load the language pack, false to fall back to English.
 */
void i18n_enable(bool enable);

/** @} */
