/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system/status_codes.h"
#include "util/attributes.h"
#include "util/uuid.h"

typedef struct PACKED {
  Uuid uuid;
  uint32_t flags;
  uint8_t num_attributes;
  uint8_t num_addresses;
  uint8_t data[]; // Serialized attributes followed by serialized addresses
} SerializedContact;

//! Given a contact's uuid, return the serialized data for that contact. This should probably only
//! be called by the contacts service. You probably want contacts_get_contact_by_uuid() instead
//! @param uuid The contact's uuid.
//! @param contact_out A pointer to the serialized contact data, NULL if the contact isn't found.
//! @return The length of the data[] field.
//! @note The caller must cleanup with contacts_db_free_serialized_contact().
int contacts_db_get_serialized_contact(const Uuid *uuid, SerializedContact **contact_out);

//! Frees the serialized contact data returned by contacts_db_get_serialized_contact().
void contacts_db_free_serialized_contact(SerializedContact *contact);


///////////////////////////////////////////
// BlobDB Boilerplate (see blob_db/api.h)
///////////////////////////////////////////

void contacts_db_init(void);

status_t contacts_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

int contacts_db_get_len(const uint8_t *key, int key_len);

status_t contacts_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

status_t contacts_db_delete(const uint8_t *key, int key_len);

status_t contacts_db_flush(void);
