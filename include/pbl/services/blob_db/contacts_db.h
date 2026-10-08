/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <system/status_codes.h>
#include <pbl/kernel/compiler.h>
#include <pbl/util/uuid.h>

/**
 * @defgroup services_blob_db_contacts_db Contacts database
 * @ingroup services_blob_db
 * @brief Contacts (::BlobDBIdContacts), keyed by contact UUID.
 * @{
 */

/** @brief Serialized contact, the value of a contacts database record. */
typedef struct PBL_PACKED {
  /** Contact UUID. */
  Uuid uuid;
  /** Contact flags. */
  uint32_t flags;
  /** Number of serialized attributes in @ref data. */
  uint8_t num_attributes;
  /** Number of serialized addresses in @ref data. */
  uint8_t num_addresses;
  /** Serialized attributes followed by serialized addresses. */
  uint8_t data[];
} SerializedContact;

/**
 * @brief Read the serialized record of a contact.
 *
 * Meant for the contacts service; other callers likely want contacts_get_contact_by_uuid().
 *
 * @param uuid Contact UUID.
 * @param[out] contact_out Task heap allocated record, NULL if not found. Free it with
 * contacts_db_free_serialized_contact().
 * @return Length of the @ref SerializedContact::data field in bytes, 0 if not found.
 */
int contacts_db_get_serialized_contact(const Uuid *uuid, SerializedContact **contact_out);

/**
 * @brief Free a record returned by contacts_db_get_serialized_contact().
 *
 * @param contact Record to free.
 */
void contacts_db_free_serialized_contact(SerializedContact *contact);

/** @brief Initialize the contacts database. */
void contacts_db_init(void);

/**
 * @brief Insert or replace a record in the contacts database.
 *
 * The key is the contact UUID and the value a ::SerializedContact.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param val Value data.
 * @param val_len Length of @p val in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t contacts_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

/**
 * @brief Get the length of a record in the contacts database.
 *
 * The key is the contact UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return Length of the value in bytes, 0 if not found, or a negative error code.
 */
int contacts_db_get_len(const uint8_t *key, int key_len);

/**
 * @brief Read a record from the contacts database.
 *
 * The key is the contact UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param[out] val_out Buffer for the value.
 * @param val_out_len Size of @p val_out in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t contacts_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

/**
 * @brief Delete a record from the contacts database.
 *
 * The key is the contact UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t contacts_db_delete(const uint8_t *key, int key_len);

/**
 * @brief Delete all records of the contacts database.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t contacts_db_flush(void);

/**
 * @brief Compact the settings file backing the contacts database.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t contacts_db_compact(void);

/** @} */
