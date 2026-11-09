/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/i18n/i18n.h>

/**
 * @defgroup services_language_pack Language packs
 * @ingroup services
 * @brief Manages the installed language packs and the one translations come from.
 *
 * A language pack is a file of system resources: a translation catalog and fonts. The catalog of
 * the selected pack is given to @ref i18n, and is given again whenever its file changes.
 * Changing the language puts a @c PEBBLE_LANGUAGE_CHANGE_EVENT.
 * @{
 */

/**
 * @brief Select the system resource holding the translation catalog.
 *
 * If the user chose English, the catalog is not used.
 *
 * @param resource_id System resource ID of the catalog.
 */
void language_pack_set_resource(uint32_t resource_id);

/**
 * @brief Use or stop using the selected translation catalog.
 *
 * @param enable true to translate with the catalog, false to fall back to English.
 */
void language_pack_enable(bool enable);

/**
 * @brief Read the metadata of an installed pack without changing the language.
 *
 * @param slot Pack slot.
 * @param[out] info Metadata, with its strings zero-padded.
 * @return true if the pack has a translation catalog.
 */
bool language_pack_get_info(uint8_t slot, struct pbl_i18n_catalog_info *info);

/**
 * @brief Check whether a pack carries at least one valid font, with or without a translation.
 *
 * @param slot Pack slot.
 */
bool language_pack_has_fonts(uint8_t slot);

/**
 * @brief Remove an installed pack, falling back to English if it was selected.
 *
 * @param slot Pack slot.
 * @return true if the pack was removed.
 */
bool language_pack_remove(uint8_t slot);

/**
 * @brief Resolve the file a PutBytes file transfer writes to.
 *
 * Additive pack installs go to the first free pack slot; other files are unchanged.
 *
 * @param filename Path of the file being transferred.
 * @return Path to write, or NULL if every pack slot is occupied.
 */
const char *language_pack_install_begin(const char *filename);

/**
 * @brief Finish a transfer started with language_pack_install_begin().
 *
 * @param success Whether the transfer completed.
 */
void language_pack_install_end(bool success);

/** @} */
