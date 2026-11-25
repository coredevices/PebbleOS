/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>

#include <pbl/bluetooth/prf_store.h>
#include <pbl/bluetooth/sm_types.h>
#include <pbl/board/partitions.h>
#include <pbl/crc/crc.h>
#include <pbl/drivers/flash.h>

#include <clar.h>
#include <fake_spi_flash.h>
#include <host/ble_store.h>
#include <prf_records_layout.h>

#define NUM_SLOTS (PART_SHARED_PRF_STORAGE_SIZE / sizeof(struct prf_record))

#define SLOT_ADDR(slot) (PART_SHARED_PRF_STORAGE_OFFSET + (slot) * sizeof(struct prf_record))

#define PEER_ADDR                                \
  {                                              \
    .type = BLE_ADDR_RANDOM,                     \
    .val = {0x88, 0x77, 0x66, 0x55, 0x44, 0x33}, \
  }

static const struct ble_store_value_sec PEER = {
  .peer_addr = PEER_ADDR,
  .key_size = 16,
  .ediv = 9876,
  .rand_num = 0x1122334455667788,
  .ltk =
      {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee,
       0xff},
  .ltk_present = 1,
  .irk =
      {0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
       0x00},
  .irk_present = 1,
  .authenticated = 1,
  .sc = 1,
};

static const struct ble_store_value_sec OUR = {
  .peer_addr = PEER_ADDR,
  .key_size = 16,
  .ediv = 123,
  .rand_num = 0x8877665544332211,
  .ltk =
      {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae,
       0xaf},
  .ltk_present = 1,
  .authenticated = 1,
  .sc = 1,
};

static const char NAME[] = "Phone";

static struct pbl_bt_sm_key s_root_keys[PBL_BT_SM_ROOT_KEY_TYPE_NUM] = {
  {.data = {0x11}},
  {.data = {0x22}},
};

static void prv_read_slot(uint32_t slot, struct prf_record *record) {
  pbl_flash_read(FLASH, SLOT_ADDR(slot), record, sizeof(*record));
}

static bool prv_slot_is_erased(uint32_t slot) {
  struct prf_record record;
  const uint8_t *bytes = (const uint8_t *)&record;

  prv_read_slot(slot, &record);
  for (size_t i = 0; i < sizeof(record); i++) {
    if (bytes[i] != 0xff) {
      return false;
    }
  }

  return true;
}

static uint32_t prv_num_written(void) {
  uint32_t num = 0;

  for (uint32_t slot = 0; slot < NUM_SLOTS; slot++) {
    if (!prv_slot_is_erased(slot)) {
      num++;
    }
  }

  return num;
}

static void prv_assert_pairing(const char *expected_name) {
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  struct ble_store_value_sec peer;
  struct ble_store_value_sec our;

  memset(name, 0xaa, sizeof(name));
  cl_assert(pbl_bt_prf_store_get_pairing(&peer, &our, name));
  cl_assert_equal_s(name, expected_name);

  cl_assert_equal_i(ble_addr_cmp(&peer.peer_addr, &PEER.peer_addr), 0);
  cl_assert_equal_i(peer.key_size, 16);
  cl_assert_equal_i(peer.ltk_present, 1);
  cl_assert_equal_i(peer.ediv, PEER.ediv);
  cl_assert(peer.rand_num == PEER.rand_num);
  cl_assert_equal_m(peer.ltk, PEER.ltk, sizeof(peer.ltk));
  cl_assert_equal_i(peer.irk_present, 1);
  cl_assert_equal_m(peer.irk, PEER.irk, sizeof(peer.irk));
  cl_assert_equal_i(peer.csrk_present, 0);
  cl_assert_equal_i(peer.sc, 1);
  cl_assert_equal_i(peer.authenticated, 1);

  cl_assert_equal_i(ble_addr_cmp(&our.peer_addr, &PEER.peer_addr), 0);
  cl_assert_equal_i(our.key_size, 16);
  cl_assert_equal_i(our.ltk_present, 1);
  cl_assert_equal_i(our.ediv, OUR.ediv);
  cl_assert(our.rand_num == OUR.rand_num);
  cl_assert_equal_m(our.ltk, OUR.ltk, sizeof(our.ltk));
  cl_assert_equal_i(our.irk_present, 0);
  cl_assert_equal_i(our.sc, 1);
  cl_assert_equal_i(our.authenticated, 1);
}

static void prv_assert_root_keys(void) {
  struct pbl_bt_sm_key key;

  for (int type = 0; type < PBL_BT_SM_ROOT_KEY_TYPE_NUM; type++) {
    cl_assert(pbl_bt_prf_store_get_root_key(type, &key));
    cl_assert_equal_m(&key, &s_root_keys[type], sizeof(key));
  }
}

void test_prf_store_records__initialize(void) {
  fake_spi_flash_init(PART_SHARED_PRF_STORAGE_OFFSET, PART_SHARED_PRF_STORAGE_SIZE);
  pbl_bt_prf_store_init();
}

void test_prf_store_records__cleanup(void) {
  fake_spi_flash_cleanup();
}

void test_prf_store_records__empty(void) {
  cl_assert(!pbl_bt_prf_store_get_pairing(nullptr, nullptr, nullptr));
  cl_assert(!pbl_bt_prf_store_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION, nullptr));
  cl_assert(!pbl_bt_prf_store_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY, nullptr));

  // Nothing to erase or name: nothing is written.
  pbl_bt_prf_store_erase_pairing();
  pbl_bt_prf_store_set_pairing_name(NAME);
  cl_assert_equal_i(prv_num_written(), 0);
}

void test_prf_store_records__pairing(void) {
  pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME);
  prv_assert_pairing(NAME);

  pbl_bt_prf_store_erase_pairing();
  cl_assert(!pbl_bt_prf_store_get_pairing(nullptr, nullptr, nullptr));
}

void test_prf_store_records__record_layout(void) {
  struct prf_record record;

  pbl_bt_prf_store_set_root_keys(s_root_keys);
  pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME);
  prv_read_slot(1, &record);

  cl_assert_equal_i(record.magic, PRF_RECORD_MAGIC);
  cl_assert_equal_i(record.version, PRF_RECORD_VERSION);
  cl_assert_equal_i(record.crc, pbl_crc32(0, (const uint8_t *)&record + 8, sizeof(record) - 8));
  cl_assert_equal_i(record.flags, PRF_RECORD_FLAG_ROOT_KEYS | PRF_RECORD_FLAG_PAIRING |
                                      PRF_RECORD_FLAG_PEER_LTK | PRF_RECORD_FLAG_PEER_IRK |
                                      PRF_RECORD_FLAG_OUR_LTK | PRF_RECORD_FLAG_SECURE_CONNECTIONS |
                                      PRF_RECORD_FLAG_AUTHENTICATED);
  cl_assert_equal_i(record.peer_addr_type, BLE_ADDR_RANDOM);
  cl_assert_equal_m(record.peer_addr, PEER.peer_addr.val, sizeof(record.peer_addr));
  cl_assert_equal_i(record.peer_ediv, PEER.ediv);
  cl_assert_equal_i(record.our_ediv, OUR.ediv);
  cl_assert(record.peer_rand == PEER.rand_num);
  cl_assert(record.our_rand == OUR.rand_num);
  cl_assert_equal_m(record.peer_ltk, PEER.ltk, sizeof(record.peer_ltk));
  cl_assert_equal_m(record.peer_irk, PEER.irk, sizeof(record.peer_irk));
  cl_assert_equal_m(record.our_ltk, OUR.ltk, sizeof(record.our_ltk));
  cl_assert_equal_m(record.root_keys, s_root_keys, sizeof(record.root_keys));
  cl_assert_equal_s(record.name, NAME);
}

// A peer that distributed no LTK, and a watch that has none for it.
void test_prf_store_records__pairing_without_ltks(void) {
  struct ble_store_value_sec peer = PEER;
  struct ble_store_value_sec peer_out;
  struct ble_store_value_sec our_out;

  peer.ltk_present = 0;
  pbl_bt_prf_store_set_pairing(&peer, nullptr, NAME);

  cl_assert(pbl_bt_prf_store_get_pairing(&peer_out, &our_out, nullptr));
  cl_assert_equal_i(peer_out.ltk_present, 0);
  cl_assert_equal_i(peer_out.irk_present, 1);
  cl_assert_equal_i(our_out.ltk_present, 0);
  cl_assert_equal_i(ble_addr_cmp(&our_out.peer_addr, &PEER.peer_addr), 0);
}

void test_prf_store_records__pairing_without_keys_is_ignored(void) {
  struct ble_store_value_sec peer = PEER;

  peer.ltk_present = 0;
  peer.irk_present = 0;
  pbl_bt_prf_store_set_pairing(&peer, nullptr, NAME);

  cl_assert(!pbl_bt_prf_store_get_pairing(nullptr, nullptr, nullptr));
  cl_assert_equal_i(prv_num_written(), 0);
}

// The keys of a pairing do not stay under the next one.
void test_prf_store_records__pairing_replaces_the_previous(void) {
  struct ble_store_value_sec other = {
    .peer_addr.val = {1, 2, 3, 4, 5, 6},
    .key_size = 16,
    .irk = {0x77},
    .irk_present = 1,
  };
  struct ble_store_value_sec peer;
  struct ble_store_value_sec our;
  struct prf_record record;
  static const uint8_t no_key[16];

  pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME);
  pbl_bt_prf_store_set_pairing(&other, nullptr, nullptr);

  cl_assert(pbl_bt_prf_store_get_pairing(&peer, &our, nullptr));
  cl_assert_equal_i(ble_addr_cmp(&peer.peer_addr, &other.peer_addr), 0);
  cl_assert_equal_i(peer.ltk_present, 0);
  cl_assert_equal_i(peer.sc, 0);
  cl_assert_equal_i(our.ltk_present, 0);

  prv_read_slot(1, &record);
  cl_assert_equal_m(record.peer_ltk, no_key, sizeof(no_key));
  cl_assert_equal_m(record.our_ltk, no_key, sizeof(no_key));
}

void test_prf_store_records__name(void) {
  struct ble_store_value_sec other = PEER;
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  pbl_bt_prf_store_set_pairing(&PEER, &OUR, nullptr);
  prv_assert_pairing("");

  pbl_bt_prf_store_set_pairing_name(NAME);
  prv_assert_pairing(NAME);

  // No name keeps the one there.
  pbl_bt_prf_store_set_pairing(&PEER, &OUR, nullptr);
  prv_assert_pairing(NAME);

  // Unless it is the name of another peer.
  other.peer_addr.val[0] ^= 0xff;
  pbl_bt_prf_store_set_pairing(&other, nullptr, nullptr);
  cl_assert(pbl_bt_prf_store_get_pairing(nullptr, nullptr, name));
  cl_assert_equal_s(name, "");
  pbl_bt_prf_store_set_pairing(&PEER, &OUR, nullptr);

  // One that does not fit is cut, and stays terminated.
  pbl_bt_prf_store_set_pairing_name("A name of more than nineteen characters");
  cl_assert(pbl_bt_prf_store_get_pairing(nullptr, nullptr, name));
  cl_assert_equal_s(name, "A name of more than");

  pbl_bt_prf_store_erase_pairing();
  pbl_bt_prf_store_set_pairing(&PEER, &OUR, nullptr);
  prv_assert_pairing("");
}

void test_prf_store_records__root_keys(void) {
  pbl_bt_prf_store_set_root_keys(s_root_keys);
  prv_assert_root_keys();

  // They outlive the pairing.
  pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME);
  pbl_bt_prf_store_erase_pairing();
  prv_assert_root_keys();

  pbl_bt_prf_store_set_root_keys(nullptr);
  cl_assert(!pbl_bt_prf_store_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION, nullptr));
  cl_assert(!pbl_bt_prf_store_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY, nullptr));
}

void test_prf_store_records__same_data_is_not_written_again(void) {
  pbl_bt_prf_store_set_root_keys(s_root_keys);
  pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME);
  cl_assert_equal_i(prv_num_written(), 2);

  pbl_bt_prf_store_set_root_keys(s_root_keys);
  pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME);
  pbl_bt_prf_store_set_pairing(&PEER, &OUR, nullptr);
  pbl_bt_prf_store_set_pairing_name(NAME);
  cl_assert_equal_i(prv_num_written(), 2);
}

void test_prf_store_records__survives_a_restart(void) {
  pbl_bt_prf_store_set_root_keys(s_root_keys);
  pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME);

  pbl_bt_prf_store_init();

  prv_assert_pairing(NAME);
  prv_assert_root_keys();

  // The next change goes after what is there.
  pbl_bt_prf_store_set_pairing_name("Other");
  cl_assert_equal_i(prv_num_written(), 3);
  prv_assert_pairing("Other");
}

void test_prf_store_records__wipe(void) {
  pbl_bt_prf_store_set_root_keys(s_root_keys);
  pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME);

  pbl_bt_prf_store_wipe();

  cl_assert_equal_i(prv_num_written(), 0);
  cl_assert(!pbl_bt_prf_store_get_pairing(nullptr, nullptr, nullptr));
  cl_assert(!pbl_bt_prf_store_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY, nullptr));

  pbl_bt_prf_store_init();
  cl_assert(!pbl_bt_prf_store_get_pairing(nullptr, nullptr, nullptr));
}

// More changes than the partition has slots: it is erased and goes on.
void test_prf_store_records__wraps_when_full(void) {
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  pbl_bt_prf_store_set_root_keys(s_root_keys);
  pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME);

  for (uint32_t i = 0; i < 3 * NUM_SLOTS; i++) {
    snprintf(name, sizeof(name), "Phone %u", (unsigned int)i);
    pbl_bt_prf_store_set_pairing_name(name);
    prv_assert_pairing(name);
    prv_assert_root_keys();
    cl_assert(prv_num_written() <= NUM_SLOTS);
  }

  pbl_bt_prf_store_init();
  prv_assert_pairing(name);
  prv_assert_root_keys();
}

// A partition that is mostly used is erased at start, keeping the store.
void test_prf_store_records__compacts_at_start(void) {
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  pbl_bt_prf_store_set_root_keys(s_root_keys);
  pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME);
  for (uint32_t i = 2; i < NUM_SLOTS - 1; i++) {
    snprintf(name, sizeof(name), "Phone %u", (unsigned int)i);
    pbl_bt_prf_store_set_pairing_name(name);
  }
  cl_assert_equal_i(prv_num_written(), NUM_SLOTS - 1);

  pbl_bt_prf_store_init();

  cl_assert_equal_i(prv_num_written(), 1);
  prv_assert_pairing(name);
  prv_assert_root_keys();
}

// A record that was not written whole is skipped: the one before it is the store.
void test_prf_store_records__torn_record(void) {
  struct prf_record record;

  pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME);
  pbl_bt_prf_store_set_pairing_name("Other");

  // Flash only clears bits: clear some of the last record.
  prv_read_slot(1, &record);
  memset(record.name, 0, sizeof(record.name));
  pbl_flash_write(FLASH, SLOT_ADDR(1), &record, sizeof(record));

  pbl_bt_prf_store_init();
  prv_assert_pairing(NAME);

  // The next change goes after the torn record.
  pbl_bt_prf_store_set_pairing_name("Third");
  cl_assert(!prv_slot_is_erased(2));
  prv_assert_pairing("Third");

  pbl_bt_prf_store_init();
  prv_assert_pairing("Third");
}

// A record of a version to come is not read.
void test_prf_store_records__unknown_version(void) {
  struct prf_record record;

  pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME);
  prv_read_slot(0, &record);
  record.version = PRF_RECORD_VERSION + 1;
  record.crc = pbl_crc32(0, (const uint8_t *)&record + 8, sizeof(record) - 8);

  fake_spi_flash_erase();
  pbl_flash_write(FLASH, SLOT_ADDR(0), &record, sizeof(record));
  pbl_bt_prf_store_init();

  cl_assert(!pbl_bt_prf_store_get_pairing(nullptr, nullptr, nullptr));
}

// Nor is the record before it, which it replaced.
void test_prf_store_records__unknown_version_after_known(void) {
  struct prf_record record;

  pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME);
  prv_read_slot(0, &record);
  record.version = PRF_RECORD_VERSION + 1;
  record.crc = pbl_crc32(0, (const uint8_t *)&record + 8, sizeof(record) - 8);
  pbl_flash_write(FLASH, SLOT_ADDR(1), &record, sizeof(record));
  pbl_bt_prf_store_init();

  cl_assert(!pbl_bt_prf_store_get_pairing(nullptr, nullptr, nullptr));

  // What is written from then on is read again.
  pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME);
  cl_assert_equal_i(prv_num_written(), 3);
  pbl_bt_prf_store_init();
  prv_assert_pairing(NAME);
}

// A slot that does not read back as written is skipped.
void test_prf_store_records__slot_that_does_not_take_is_skipped(void) {
  static const uint8_t worn[sizeof(struct prf_record)];
  struct prf_record record;

  cl_assert_equal_i(pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME), 0);

  // Bits that are programmed stay so until an erase.
  pbl_flash_write(FLASH, SLOT_ADDR(1), worn, sizeof(worn));

  cl_assert_equal_i(pbl_bt_prf_store_set_pairing_name("Other"), 0);
  prv_assert_pairing("Other");
  prv_read_slot(2, &record);
  cl_assert_equal_s(record.name, "Other");

  pbl_bt_prf_store_init();
  prv_assert_pairing("Other");
}

// What another layout, or nothing, left in the partition.
void test_prf_store_records__garbage(void) {
  static uint8_t garbage[PART_SHARED_PRF_STORAGE_SIZE];

  memset(garbage, 0x17, sizeof(garbage));
  pbl_flash_write(FLASH, PART_SHARED_PRF_STORAGE_OFFSET, garbage, sizeof(garbage));

  pbl_bt_prf_store_init();

  cl_assert_equal_i(prv_num_written(), 0);
  cl_assert(!pbl_bt_prf_store_get_pairing(nullptr, nullptr, nullptr));

  pbl_bt_prf_store_set_pairing(&PEER, &OUR, NAME);
  prv_assert_pairing(NAME);
}
