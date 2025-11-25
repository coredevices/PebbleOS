/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/normal/filesystem/pfs.h"
#include "services/normal/blob_db/contacts_db.h"
#include "services/normal/contacts/contacts.h"
#include "util/uuid.h"

// Fixture
////////////////////////////////////////////////////////////////

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_spi_flash.h"
#include "fake_system_task.h"
#include "fake_kernel_services_notifications.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_layout_layer.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"

#define CONTACT_1_UUID 0x0a, 0x04, 0x98, 0x00, 0x39, 0x18, 0x47, 0xaa,  0x9c, 0x16, 0x8e, 0xa0, 0xa8, 0x2a, 0x2e, 0xb8
#define ADDRESS_1_UUID 0xd3, 0x72, 0x2d, 0x75, 0x6b, 0x21, 0x49, 0x2a,  0x9c, 0xc7, 0x5f, 0xf8, 0x4d, 0xd2, 0x5a, 0x9c
#define ADDRESS_2_UUID 0x43, 0x03, 0x91, 0x06, 0x80, 0x39, 0x48, 0xea,  0x92, 0x72, 0xf3, 0x4c, 0xd5, 0x35, 0x9c, 0xcf

static const uint8_t s_contact_1[] = {
  // Uuid
  CONTACT_1_UUID,
  // Flags
  0x00, 0x00, 0x00, 0x00,
  // Number of Attributes,
  0x01,
  // Number of Addresses,
  0x02,

  // Attribute 1
  0x01,                     // Attribute ID - Title
  0x08, 0x00,               // Attribute Length
  // Attribute text: "John Doe"
  'J', 'o', 'h', 'n', ' ', 'D', 'o', 'e',

  // Address 1
  // Uuid
  ADDRESS_1_UUID,
  0x01,                     // AddressType - PhoneNumber
  0x02,                     // Number of attributes
  // Address Attributes
  0x01,                     // Attribute ID - Title
  0x06, 0x00,               // Attribute Length
  // Attribute text:
  'm', 'o', 'b', 'i', 'l', 'e',
  0x27,                     // Attribute ID - Address
  0x0a, 0x00,               // Attribute Length
  // Attribute text:
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',

  // Address 2
  // Uuid
  ADDRESS_2_UUID,
  0x02,                     // AddressType - Email
  0x02,                     // Number of attributes
  // Address Attributes
  0x01,                     // Attribute ID - Title
  0x04, 0x00,               // Attribute Length
  // Attribute text:
  'h', 'o', 'm', 'e',
  0x27,                     // Attribute ID - Address
  0x10, 0x00,               // Attribute Length
  // Attribute text:
  'n', 'a', 'm', 'e', '@', 'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm',
};


// Setup
////////////////////////////////////////////////////////////////

void test_contacts_db__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  contacts_db_init();
}

void test_contacts_db__cleanup(void) {
}

// Tests
////////////////////////////////////////////////////////////////

void test_contacts_db__insert(void) {
  uint8_t contact[sizeof(s_contact_1)];
  memcpy(contact, s_contact_1, sizeof(s_contact_1));

  cl_assert_equal_i(contacts_db_insert((uint8_t *)&contact, UUID_SIZE,
                                       (uint8_t *)&contact, sizeof(contact)), 0);
  cl_assert_equal_i(contacts_db_get_len((uint8_t *)&contact, UUID_SIZE), sizeof(contact));

  uint8_t *contact_out = kernel_malloc(sizeof(contact));
  cl_assert_equal_i(contacts_db_read((uint8_t *)&contact, UUID_SIZE,
                                     contact_out, sizeof(contact)), 0);
  cl_assert_equal_m((uint8_t *)s_contact_1, contact_out, sizeof(contact));
  kernel_free(contact_out);
}

void test_contacts_db__insert_remove(void) {
  uint8_t contact[sizeof(s_contact_1)];
  memcpy(contact, s_contact_1, sizeof(s_contact_1));

  cl_assert_equal_i(contacts_db_insert((uint8_t *)&contact, UUID_SIZE, (uint8_t *)&contact, sizeof(contact)), 0);
  cl_assert_equal_i(contacts_db_delete((uint8_t *)&contact, UUID_SIZE), 0);
  cl_assert_equal_i(contacts_db_get_len((uint8_t *)&contact, UUID_SIZE), 0);
}

void test_contacts_db__flush(void) {
  uint8_t contact[sizeof(s_contact_1)];
  memcpy(contact, s_contact_1, sizeof(s_contact_1));

  cl_assert_equal_i(contacts_db_insert((uint8_t *)&contact, UUID_SIZE, (uint8_t *)&contact, sizeof(contact)), 0);
  cl_assert_equal_i(contacts_db_flush(), 0);
  fake_system_task_callbacks_invoke_pending();
  cl_assert_equal_i(contacts_db_get_len((uint8_t *)&contact, UUID_SIZE), 0);
}

void test_contacts_db__get_serialized_contact(void) {
  uint8_t contact[sizeof(s_contact_1)];
  memcpy(contact, s_contact_1, sizeof(s_contact_1));

  cl_assert_equal_i(contacts_db_insert((uint8_t *)&contact, UUID_SIZE,
                                       (uint8_t *)&contact, sizeof(contact)), 0);
  cl_assert_equal_i(contacts_db_get_len((uint8_t *)&contact, UUID_SIZE), sizeof(contact));

  SerializedContact *serialized_contact;
  contacts_db_get_serialized_contact((Uuid *)&contact, &serialized_contact);
  cl_assert_equal_i(serialized_contact->flags, 0);
  cl_assert_equal_i(serialized_contact->num_attributes, 1);
  cl_assert_equal_i(serialized_contact->num_addresses, 2);
  contacts_db_free_serialized_contact(serialized_contact);
}
