/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/normal/filesystem/pfs.h"
#include "services/normal/contacts/attributes_address.h"
#include "services/normal/contacts/contacts.h"
#include "services/normal/blob_db/contacts_db.h"

#include "util/size.h"

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_spi_flash.h"
#include "fake_rtc.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_fonts.h"
#include "stubs_hexdump.h"
#include "stubs_layout_layer.h"
#include "stubs_passert.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_prompt.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_rand_ptr.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"

#define CONTACT_1_UUID 0x60, 0xcd, 0x45, 0x67, 0x2b, 0xcf, 0x45, 0xb3,  0x8d, 0x4c, 0x75, 0x34, 0xda, 0x6f, 0x16, 0xe3
#define ADDRESS_1_UUID 0xc2, 0x77, 0x31, 0x10, 0xcc, 0x01, 0x44, 0x4b,  0xaa, 0x46, 0xe0, 0xa8, 0xbe, 0xd6, 0x6e, 0x43

static const uint8_t s_contact_1[] = {
  // Uuid
  CONTACT_1_UUID,
  // Flags
  0x00, 0x00, 0x00, 0x00,
  // Number of Attributes,
  0x01,
  // Number of Addresses,
  0x01,

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
  0x04, 0x00,               // Attribute Length
  // Attribute text:
  'h', 'o', 'm', 'e',
  0x27,                     // Attribute ID - Address
  0x0a, 0x00,               // Attribute Length
  // Attribute text:
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
};

void test_contacts__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  contacts_db_init();
}

void test_contacts__cleanup(void) {
}

static Attribute address1_attributes[] = {
  {.id = AttributeIdTitle, .cstring = "home"},
  {.id = AttributeIdAddress, .cstring = "0123456789"},
};

static Address addresses[] = {
  {
    .id = {ADDRESS_1_UUID},
    .type = AddressTypePhoneNumber,
    .attr_list = {
      .num_attributes = ARRAY_LENGTH(address1_attributes),
      .attributes = address1_attributes}
  },
};

static Attribute attributes[] = {
    {.id = AttributeIdTitle, .cstring = "John Doe"},
};

void test_contacts__get_contact_by_uuid(void) {
  const Uuid uuid = (Uuid){CONTACT_1_UUID};

  uint8_t serialized_contact[sizeof(s_contact_1)];
  memcpy(serialized_contact, s_contact_1, sizeof(s_contact_1));

  cl_assert_equal_i(contacts_db_insert((uint8_t *)&uuid, UUID_SIZE,
                                       (uint8_t *)&serialized_contact,
                                       sizeof(serialized_contact)), 0);
  cl_assert(contacts_db_get_len((uint8_t *)&uuid, UUID_SIZE) > 0);

  Contact *contact = contacts_get_contact_by_uuid(&uuid);
  cl_assert(contact);
  cl_assert_equal_m(&contact->id, &uuid, UUID_SIZE);
  cl_assert_equal_i(contact->flags, 0);
  cl_assert_equal_i(contact->attr_list.num_attributes, 1);
  cl_assert_equal_i(contact->addr_list.num_addresses, 1);

  contacts_free_contact(contact);
}
