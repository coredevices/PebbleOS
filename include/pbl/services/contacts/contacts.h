/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "attributes_address.h"

#include <pbl/util/uuid.h>

/**
 * @defgroup services_contacts Contacts
 * @ingroup services
 * @brief Contacts stored in the contacts database by the phone.
 *
 * @code{.c}
 * Contact *contact = contacts_get_contact_by_uuid(&uuid);
 * if (contact) {
 *   for (int i = 0; i < contact->addr_list.num_addresses; i++) {
 *     const Address *addr = &contact->addr_list.addresses[i];
 *     // ...
 *   }
 *   contacts_free_contact(contact);
 * }
 * @endcode
 * @{
 */

/**
 * @brief Contact, allocated in a single block with its attributes, addresses and strings.
 */
typedef struct {
  /** Contact UUID. */
  Uuid id;
  /** Flags as stored in the contacts database. */
  uint32_t flags;
  /** Contact attributes (name, ...). */
  AttributeList attr_list;
  /** Addresses of the contact. */
  AddressList addr_list;
} Contact;

/**
 * @brief Look up a contact.
 *
 * @param uuid Contact UUID.
 * @return Contact, to be freed with contacts_free_contact(), or NULL if not found or invalid.
 */
Contact *contacts_get_contact_by_uuid(const Uuid *uuid);

/**
 * @brief Free a contact.
 *
 * @param contact Contact returned by contacts_get_contact_by_uuid().
 */
void contacts_free_contact(Contact *contact);

/** @} */
