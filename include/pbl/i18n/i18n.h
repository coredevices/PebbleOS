/* SPDX-FileCopyrightText: 2000 Citrus Project */
/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup i18n Internationalization
 * @ingroup subsys
 * @brief Translates strings with a gettext message catalog.
 *
 * Translations come from a catalog, a gettext MO file read through a caller-provided reader, set
 * with pbl_i18n_set_catalog(). Strings are looked up by their English original (the msgid); when
 * no translation exists, or no catalog is set, the original is returned. A context can be attached
 * to disambiguate identical originals with the @c _ctx_ variants. Mark strings with pbl_i18n_noop()
 * or pbl_i18n_ctx_noop() where they cannot be translated in place, so they are still extracted for
 * translation.
 *
 * Translations returned by pbl_i18n_get() are cached per owner and stay valid until freed:
 *
 * @code{.c}
 * text_layer_set_text(&data->title, pbl_i18n_get("Settings", data));
 * ...
 * pbl_i18n_free_all(data);
 * @endcode
 *
 * For short-lived use, translate into a buffer instead:
 *
 * @code{.c}
 * char buf[32];
 * pbl_i18n_ctx_get_with_buffer("Alarm", "Snooze", buf, sizeof(buf));
 * @endcode
 *
 * The functions may be called from any thread, but not from ISRs. A catalog reader is called with
 * the i18n lock held, so it must not call back into i18n.
 * @{
 */

/** @brief Size of an ISO locale string such as "en_US", including the terminator. */
#define PBL_I18N_LOCALE_LEN 6
/** @brief Size of a language name buffer, including the terminator. */
#define PBL_I18N_NAME_LEN 30

/** @brief Metadata of a catalog. */
struct pbl_i18n_catalog_info {
  /** ISO locale such as "fr_FR". */
  char locale[PBL_I18N_LOCALE_LEN];
  /** Language name, the locale if the catalog has none. */
  char name[PBL_I18N_NAME_LEN];
  /** Catalog version. */
  uint16_t version;
};

/** @brief Catalog of translations, a gettext MO file. */
struct pbl_i18n_catalog {
  /**
   * @brief Read from the catalog.
   *
   * @param data @ref pbl_i18n_catalog::data.
   * @param offset Offset in bytes from the start of the catalog.
   * @param[out] buf Output buffer.
   * @param length Number of bytes to read.
   * @return 0 if @p length bytes were read, negative errno otherwise.
   */
  int (*read)(void *data, uint32_t offset, void *buf, size_t length);
  /** Data passed to @ref pbl_i18n_catalog::read. */
  void *data;
  /** Size of the catalog in bytes. */
  size_t size;
};

/**
 * @brief Set the catalog translations come from.
 *
 * Frees every cached translation, so pointers returned by pbl_i18n_get() become invalid. If the
 * catalog cannot be used no catalog is set. The catalog is copied, but
 * @ref pbl_i18n_catalog::data must stay valid until another catalog is set.
 *
 * @param catalog Catalog, NULL for none.
 * @return 0 on success, negative errno otherwise.
 */
int pbl_i18n_set_catalog(const struct pbl_i18n_catalog *catalog);

/**
 * @brief Read the metadata of a catalog without setting it.
 *
 * @param catalog Catalog, @ref pbl_i18n_catalog::size is not used.
 * @param[out] info Metadata, with its strings zero-padded.
 * @return 0 on success, negative errno otherwise.
 */
int pbl_i18n_catalog_get_info(const struct pbl_i18n_catalog *catalog,
                              struct pbl_i18n_catalog_info *info);

/**
 * @brief Tag a string for extraction without translating it.
 *
 * For places where pbl_i18n_get() cannot be called, e.g. constant initializers. Translate the
 * string later with pbl_i18n_get().
 *
 * @param string String literal.
 */
#define pbl_i18n_noop(string) (string)

/**
 * @brief Tag a string with a context for extraction without translating it.
 *
 * For places where pbl_i18n_ctx_get() cannot be called, e.g. constant initializers. The result
 * embeds the context, so translate it later with pbl_i18n_get() rather than pbl_i18n_ctx_get().
 *
 * @param ctx Context string literal.
 * @param string String literal.
 */
#define pbl_i18n_ctx_noop(ctx, string) (ctx "\4" string)

/**
 * @brief Translate a string, caching the result for an owner.
 *
 * The translation is truncated to 199 characters. There is no reference counting: when the same
 * string is requested several times for the same owner, all returned pointers become invalid
 * once pbl_i18n_free() is called on any of them. Setting a catalog also invalidates them.
 *
 * @param string Original string, possibly with a context from pbl_i18n_ctx_noop().
 * @param owner Owner of the cached translation, must not be NULL.
 * @return Translation, or the original string without its context if there is none.
 */
const char *pbl_i18n_get(const char *string, const void *owner);

/**
 * @brief pbl_i18n_get() with a context.
 *
 * @param ctx Context string literal.
 * @param string Original string literal.
 * @param owner Owner of the cached translation.
 */
#define pbl_i18n_ctx_get(ctx, string, owner) pbl_i18n_get(pbl_i18n_ctx_noop(ctx, string), owner)

/**
 * @brief Translate a string into a buffer.
 *
 * Nothing is cached. The result is truncated to fit and always terminated, unless @p length is 0.
 *
 * @param string Original string, possibly with a context from pbl_i18n_ctx_noop().
 * @param[out] buffer Output buffer.
 * @param length Size of @p buffer in bytes.
 */
void pbl_i18n_get_with_buffer(const char *string, char *buffer, size_t length);

/**
 * @brief pbl_i18n_get_with_buffer() with a context.
 *
 * @param ctx Context string literal.
 * @param string Original string literal.
 * @param buffer Output buffer.
 * @param length Size of @p buffer in bytes.
 */
#define pbl_i18n_ctx_get_with_buffer(ctx, string, buffer, length) \
  pbl_i18n_get_with_buffer(pbl_i18n_ctx_noop(ctx, string), buffer, length)

/**
 * @brief Get the length of a translated string.
 *
 * @param string Original string, possibly with a context from pbl_i18n_ctx_noop().
 * @return Length of the translation without terminator, or of the original if there is none.
 */
size_t pbl_i18n_get_length(const char *string);

/**
 * @brief pbl_i18n_get_length() with a context.
 *
 * @param ctx Context string literal.
 * @param string Original string literal.
 */
#define pbl_i18n_ctx_get_length(ctx, string) pbl_i18n_get_length(pbl_i18n_ctx_noop(ctx, string))

/**
 * @brief Free a cached translation.
 *
 * @param string Original string passed to pbl_i18n_get().
 * @param owner Owner passed to pbl_i18n_get(), must not be NULL.
 */
void pbl_i18n_free(const char *string, const void *owner);

/**
 * @brief pbl_i18n_free() with a context.
 *
 * @param ctx Context string literal.
 * @param string Original string literal.
 * @param owner Owner passed to pbl_i18n_ctx_get().
 */
#define pbl_i18n_ctx_free(ctx, string, owner) pbl_i18n_free(pbl_i18n_ctx_noop(ctx, string), owner)

/**
 * @brief Free all cached translations of an owner.
 *
 * @param owner Owner passed to pbl_i18n_get().
 */
void pbl_i18n_free_all(const void *owner);

/**
 * @brief Get the ISO locale of the catalog.
 *
 * @return Locale such as "fr_FR", "en_US" when no catalog is set.
 */
char *pbl_i18n_get_locale(void);

/**
 * @brief Get the version of the catalog.
 *
 * @return Catalog version, 1 when no catalog is set.
 */
uint16_t pbl_i18n_get_version(void);

/**
 * @brief Get the language name of the catalog.
 *
 * @return Language name, "English" when no catalog is set.
 */
char *pbl_i18n_get_lang_name(void);

/** @} */
