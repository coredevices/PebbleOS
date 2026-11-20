/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/bluetooth/prf_store.h>
#include <pbl/bluetooth/prf_store_layout.h>
#include <pbl/bluetooth/sm_types.h>
#include <pbl/board/partitions.h>
#include <pbl/btutil/sm_util.h>
#include <pbl/drivers/flash.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/util/size.h>

#include <clar.h>

// Fakes
//////////////////////////////////////////////////////////
#include <fake_spi_flash.h>

// Externs
//////////////////////////////////////////////////////////
extern uint32_t pbl_bt_prf_store__get_page(void);
extern void pbl_bt_prf_store__set_page(uint32_t page_num);

// Defines
//////////////////////////////////////////////////////////
#define SPRF_REGION_SIZE PART_SHARED_PRF_STORAGE_SIZE
#define SPRF_NUM_PAGES   (SPRF_REGION_SIZE / sizeof(SharedPRFData))

#define SPRF_PAGE_FLASH_OFFSET(idx) (PART_SHARED_PRF_STORAGE_OFFSET + (idx * sizeof(SharedPRFData)))

// Stubs
//////////////////////////////////////////////////////////
static bool s_mutex_locked;
void pbl_mutex_init(struct pbl_mutex *m) {
}

int pbl_mutex_lock_lr(struct pbl_mutex *m, pbl_timeout_t timeout, uintptr_t lr) {
  cl_assert_equal_b(s_mutex_locked, false);
  s_mutex_locked = true;
  return 0;
}

void pbl_mutex_unlock(struct pbl_mutex *m) {
  cl_assert_equal_b(s_mutex_locked, true);
  s_mutex_locked = false;
}

static const char DEVICE_NAME[PBL_BT_DEVICE_NAME_BUFFER_SIZE] = "ABCDEFGHIJKLMNOPQRS";
static const char *PAIRING_NAME = "Blah123";

static const struct pbl_bt_sm_pairing_info PAIRING_INFO = (const struct pbl_bt_sm_pairing_info){
  .local_encryption_info =
      {
        .ediv = 123,
        .ltk =
            (const struct pbl_bt_sm_key){
              .data =
                  {
                    0x44,
                    0x55,
                    0x66,
                    0x77,
                    0x00,
                    0x11,
                    0x22,
                    0x33,
                    0xcc,
                    0xdd,
                    0xee,
                    0xff,
                    0x88,
                    0x99,
                    0xaa,
                    0xbb,
                  },
            },
        .rand = 0x11223344,
      },

  .remote_encryption_info =
      {
        .ltk =
            (const struct pbl_bt_sm_key){
              .data =
                  {
                    0x00,
                    0x11,
                    0x22,
                    0x33,
                    0x44,
                    0x55,
                    0x66,
                    0x77,
                    0x88,
                    0x99,
                    0xaa,
                    0xbb,
                    0xcc,
                    0xdd,
                    0xee,
                    0xff,
                  },
            },
        .rand = 0x11223344,
        .ediv = 9876,
      },

  .irk =
      (const struct pbl_bt_sm_key){
        .data =
            {
              0x88,
              0x99,
              0xaa,
              0xbb,
              0xcc,
              0xdd,
              0xee,
              0xff,
              0x00,
              0x11,
              0x22,
              0x33,
              0x44,
              0x55,
              0x66,
              0x77,
            },
      },
  .identity =
      (const struct pbl_bt_device_internal){
        .opaque.opaque_64 = 0x1122334455667788,
      },

  .csrk =
      {
        .data =
            {
              0xcc,
              0xdd,
              0xee,
              0xff,
              0x88,
              0x99,
              0xaa,
              0xbb,
              0x44,
              0x55,
              0x66,
              0x77,
              0x00,
              0x11,
              0x22,
              0x33,
            },
      },

  .is_local_encryption_info_valid = true,
  .is_remote_encryption_info_valid = true,
  .is_remote_identity_info_valid = true,
  .is_remote_signing_info_valid = true,
  .is_mitm_protection_enabled = true,
};

// Helpers
///////////////////////////////////////////////////////////
// A field to set and read back as a flag: the root keys, as one of two values.
static const struct pbl_bt_sm_key ROOT_KEYS_SET[PBL_BT_SM_ROOT_KEY_TYPE_NUM] = {
  {.data = {0x11}},
  {.data = {0x22}},
};
static const struct pbl_bt_sm_key ROOT_KEYS_CLEAR[PBL_BT_SM_ROOT_KEY_TYPE_NUM] = {
  {.data = {0x33}},
  {.data = {0x44}},
};

static void prv_set_field(bool set) {
  struct pbl_bt_sm_key keys[PBL_BT_SM_ROOT_KEY_TYPE_NUM];

  memcpy(keys, set ? ROOT_KEYS_SET : ROOT_KEYS_CLEAR, sizeof(keys));
  pbl_bt_prf_store_set_root_keys(keys);
}

static bool prv_get_field(void) {
  struct pbl_bt_sm_key key;

  if (!pbl_bt_prf_store_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY, &key)) {
    return false;
  }

  return memcmp(&key, &ROOT_KEYS_SET[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY], sizeof(key)) == 0;
}

static void prv_fill_flash_random_data(void) {
  uint8_t *buf = pbl_malloc_check(SPRF_REGION_SIZE);
  fake_spi_flash_erase();
  memset(buf, 0x17, SPRF_REGION_SIZE);
  pbl_flash_write(FLASH, PART_SHARED_PRF_STORAGE_OFFSET, buf, SPRF_REGION_SIZE);
  pbl_free(buf);
}

static void prv_assert_mutexes_unlocked(void) {
  cl_assert_equal_b(s_mutex_locked, false);
}

// Tests
///////////////////////////////////////////////////////////
void test_prf_store__initialize(void) {
  fake_spi_flash_init(PART_SHARED_PRF_STORAGE_OFFSET, SPRF_REGION_SIZE);
  pbl_bt_prf_store_init();
}

void test_prf_store__cleanup(void) {
  fake_spi_flash_cleanup();
  prv_assert_mutexes_unlocked();
}

void test_prf_store__init_all_zeros(void) {
  uint8_t *flash_buf = pbl_zalloc(SPRF_REGION_SIZE);
  pbl_flash_write(FLASH, PART_SHARED_PRF_STORAGE_OFFSET, flash_buf, SPRF_REGION_SIZE);
  pbl_bt_prf_store_init();
  prv_set_field(true);
  cl_assert_equal_b(prv_get_field(), true);
}

void test_prf_store__find_first_valid_sector(void) {
  SharedPRFData data;

  // These are the pages that this test will place a valid header. It will write invalid pages
  // before the valid one.
  uint8_t page_idx[] = {0, 1, SPRF_NUM_PAGES / 2, SPRF_NUM_PAGES - 1};

  for (uint32_t i = 0; i < PBL_ARRAY_SIZE(page_idx); i++) {
    fake_spi_flash_erase();

    // Invalidate all entries before it to simulate logging style
    for (uint32_t j = 0; j < page_idx[i]; j++) {
      SprfMagic inv_magic = SprfMagic_InvalidatedEntry;
      pbl_flash_write(FLASH, SPRF_PAGE_FLASH_OFFSET(j), (uint8_t *)&inv_magic, sizeof(inv_magic));
    }

    // Write the valid page
    pbl_flash_read(FLASH, SPRF_PAGE_FLASH_OFFSET(page_idx[i]), (uint8_t *)&data, sizeof(data));
    data.magic = SprfMagic_ValidEntry;
    pbl_flash_write(FLASH, SPRF_PAGE_FLASH_OFFSET(page_idx[i]), (uint8_t *)&data, sizeof(data));

    // Call init and see if it found the valid page
    pbl_bt_prf_store_init();
    uint32_t desired_page_idx = page_idx[i];
    if (page_idx[i] > SPRF_MAX_NUM_PAGES_MULT(SPRF_NUM_PAGES)) {
      desired_page_idx = 0;
    }
    cl_assert_equal_i(pbl_bt_prf_store__get_page(), desired_page_idx);
  }

  // Erase the entire region and test that it picks the first empty page
  fake_spi_flash_erase();
  pbl_bt_prf_store_init();
  cl_assert_equal_i(pbl_bt_prf_store__get_page(), 0);

  // Test that it sees all pages are invalid, rewrites everything, and picks the first empty page
  prv_fill_flash_random_data();
  pbl_bt_prf_store_init();
  cl_assert_equal_i(pbl_bt_prf_store__get_page(), 0);
}

void test_prf_store__wipe_all(void) {
  struct pbl_bt_sm_pairing_info sm_pairing_info;
  memset(&sm_pairing_info, 0xaa, sizeof(sm_pairing_info));
  sm_pairing_info.is_local_encryption_info_valid = true;
  sm_pairing_info.is_remote_signing_info_valid = true;
  sm_pairing_info.is_remote_identity_info_valid = true;
  pbl_bt_prf_store_set_pairing(&sm_pairing_info, PAIRING_NAME, 0 /* flags */);
  cl_assert_equal_b(pbl_bt_prf_store_get_pairing(nullptr, nullptr, nullptr), true);
  pbl_bt_prf_store_wipe();

  cl_assert_equal_b(pbl_bt_prf_store_get_pairing(nullptr, nullptr, nullptr), false);
}

void test_prf_store__ble_pairing(void) {
  pbl_bt_prf_store_wipe();
  cl_assert_equal_b(pbl_bt_prf_store_get_pairing(nullptr, nullptr, nullptr), false);

  pbl_bt_prf_store_set_pairing(&PAIRING_INFO, DEVICE_NAME, 0 /* flags */);

  char name_out[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  memset(name_out, 0, sizeof(name_out));
  struct pbl_bt_sm_pairing_info pairing_info_out = {};
  uint8_t flags = 0;
  cl_assert_equal_b(pbl_bt_prf_store_get_pairing(&pairing_info_out, name_out, &flags), true);
  cl_assert_equal_i(flags, 0);
  cl_assert_equal_i(strcmp(DEVICE_NAME, name_out), 0);
  cl_assert_equal_b(PAIRING_INFO.is_mitm_protection_enabled,
                    pairing_info_out.is_mitm_protection_enabled);
  cl_assert_equal_b(PAIRING_INFO.is_remote_signing_info_valid,
                    pairing_info_out.is_remote_signing_info_valid);
  cl_assert_equal_b(PAIRING_INFO.is_remote_identity_info_valid,
                    pairing_info_out.is_remote_identity_info_valid);
  cl_assert_equal_b(PAIRING_INFO.is_remote_encryption_info_valid,
                    pairing_info_out.is_remote_encryption_info_valid);
  cl_assert_equal_b(PAIRING_INFO.is_local_encryption_info_valid,
                    pairing_info_out.is_local_encryption_info_valid);
  cl_assert_equal_i(PAIRING_INFO.local_encryption_info.ediv,
                    pairing_info_out.local_encryption_info.ediv);
  cl_assert_equal_i(PAIRING_INFO.local_encryption_info.div,
                    pairing_info_out.local_encryption_info.div);
  cl_assert(PAIRING_INFO.identity.opaque.opaque_64 == pairing_info_out.identity.opaque.opaque_64);
  cl_assert(PAIRING_INFO.remote_encryption_info.rand ==
            pairing_info_out.remote_encryption_info.rand);
  cl_assert_equal_i(PAIRING_INFO.remote_encryption_info.ediv,
                    pairing_info_out.remote_encryption_info.ediv);
  cl_assert_equal_i(
      memcmp(&PAIRING_INFO.remote_encryption_info.ltk, &pairing_info_out.remote_encryption_info.ltk,
             sizeof(struct pbl_bt_sm_key)),
      0);
  cl_assert_equal_i(memcmp(&PAIRING_INFO.irk, &pairing_info_out.irk, sizeof(struct pbl_bt_sm_key)),
                    0);
  cl_assert_equal_i(
      memcmp(&PAIRING_INFO.csrk, &pairing_info_out.csrk, sizeof(struct pbl_bt_sm_key)), 0);

  pbl_bt_prf_store_erase_pairing();
  cl_assert_equal_b(pbl_bt_prf_store_get_pairing(nullptr, nullptr, nullptr), false);
}

void test_prf_store__root_keys(void) {
  pbl_bt_prf_store_wipe();

  cl_assert_equal_b(pbl_bt_prf_store_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY, nullptr),
                    false);
  cl_assert_equal_b(pbl_bt_prf_store_get_root_key(PBL_BT_SM_ROOT_KEY_TYPE_ENCRYPTION, nullptr),
                    false);

  struct pbl_bt_sm_key keys[2];
  for (int i = 0; i < sizeof(keys); ++i) {
    ((uint8_t *)keys)[i] = i;
  }

  pbl_bt_prf_store_set_root_keys(keys);

  struct pbl_bt_sm_key keys_out[2];
  for (enum pbl_bt_sm_root_key_type key_type = 0; key_type < PBL_BT_SM_ROOT_KEY_TYPE_NUM;
       ++key_type) {
    cl_assert_equal_b(pbl_bt_prf_store_get_root_key(key_type, &keys_out[key_type]), true);
    // It's a byte array inside, so memcmp should be OK to use:
    cl_assert_equal_i(memcmp(&keys[key_type], &keys_out[key_type], sizeof(keys[0])), 0);
  }
}

void test_prf_store__rewrite_pages_and_wrap_around(void) {
  bool toggle = false;

  pbl_bt_prf_store_wipe();

  // Make sure to wrap around a few times and confirm that works
  for (uint32_t iter = 0; iter < 3; iter++) {
    toggle = false;
    // Iterate through all possible pages and keep writing new data, confirm it's the right data.
    for (uint32_t i = 0; i < SPRF_NUM_PAGES; i++) {
      prv_set_field(toggle);
      cl_assert_equal_b(prv_get_field(), toggle);

      cl_assert_equal_i(pbl_bt_prf_store__get_page(), i);

      toggle = !toggle;
    }
  }
}

void test_prf_store__save_all_data_confirm_all_data_correct(void) {
  const bool FIELD_SET = true;

  pbl_bt_prf_store_set_pairing(&PAIRING_INFO, DEVICE_NAME, 0xff /* flags */);
  prv_set_field(FIELD_SET);

  char device_name_out[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  // Check pairing info
  struct pbl_bt_sm_pairing_info pairing_info_out;
  uint8_t flags = 0;
  pbl_bt_prf_store_get_pairing(&pairing_info_out, device_name_out, &flags);
  cl_assert_equal_i(flags, 0xff);
  cl_assert_equal_b(sm_is_pairing_info_equal_identity(&PAIRING_INFO, &pairing_info_out), true);
  cl_assert_equal_s(device_name_out, DEVICE_NAME);

  // Check the root keys
  cl_assert_equal_b(prv_get_field(), FIELD_SET);
}

void test_prf_store__write_in_loop_confirm_data_still_intact(void) {
  bool FIELD_SET = true;

  pbl_bt_prf_store_set_pairing(&PAIRING_INFO, DEVICE_NAME, 0xff /* flags */);
  prv_set_field(FIELD_SET);

  for (uint32_t i = 0; i < 50; i++) {
    FIELD_SET = !FIELD_SET;

    cl_assert_equal_i(pbl_bt_prf_store__get_page(), i % SPRF_NUM_PAGES);
    prv_set_field(FIELD_SET);
    cl_assert_equal_i(pbl_bt_prf_store__get_page(), (i + 1) % SPRF_NUM_PAGES);
  }

  // Check if our old information is still in tact after looping and rewriting a ton of times

  char device_name_out[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  // Check pairing info
  struct pbl_bt_sm_pairing_info pairing_info_out;
  uint8_t flags = 0;

  pbl_bt_prf_store_get_pairing(&pairing_info_out, device_name_out, &flags);
  cl_assert_equal_i(flags, 0xff);
  cl_assert_equal_b(sm_is_pairing_info_equal_identity(&PAIRING_INFO, &pairing_info_out), true);
  cl_assert_equal_s(device_name_out, DEVICE_NAME);
}

// Sets the root keys, then corrupts their crc
void test_prf_store__handle_corrupt_field_same(void) {
  bool FIELD_SET = true;
  prv_set_field(FIELD_SET);
  cl_assert_equal_i(pbl_bt_prf_store__get_page(), 0);

  SharedPRFData data;
  pbl_flash_read(FLASH, SPRF_PAGE_FLASH_OFFSET(pbl_bt_prf_store__get_page()), (uint8_t *)&data,
                 sizeof(data));
  cl_assert(data.root_keys.crc != 0xFFFFFFFF);

  uint32_t new_crc = 0;
  pbl_flash_write(FLASH,
                  SPRF_PAGE_FLASH_OFFSET(pbl_bt_prf_store__get_page()) +
                      offsetof(SharedPRFData, root_keys) + offsetof(SprfRootKeys, crc),
                  (uint8_t *)&new_crc, sizeof(new_crc));

  // Confirm new CRC was written
  pbl_flash_read(FLASH, SPRF_PAGE_FLASH_OFFSET(pbl_bt_prf_store__get_page()), (uint8_t *)&data,
                 sizeof(data));
  cl_assert_equal_i(data.root_keys.crc, new_crc);

  // Should be corrupt, so it should return false
  cl_assert_equal_b(prv_get_field(), false);
  // Should have moved to the next page
  cl_assert_equal_b(pbl_bt_prf_store__get_page(), 1);

  // Let's do it again, but move the valid page to index NUM_PAGES - 1 so we force a wrap around
  fake_spi_flash_erase();
  pbl_bt_prf_store__set_page(SPRF_NUM_PAGES - 1);
  prv_set_field(FIELD_SET);
  pbl_flash_write(FLASH,
                  SPRF_PAGE_FLASH_OFFSET(pbl_bt_prf_store__get_page()) +
                      offsetof(SharedPRFData, root_keys) + offsetof(SprfRootKeys, crc),
                  (uint8_t *)&new_crc, sizeof(new_crc));
  // Should be corrupt, so it should return false
  cl_assert_equal_b(prv_get_field(), false);
  // Should have moved to the next page, which is ZERO since we had to wrap around.
  cl_assert_equal_b(pbl_bt_prf_store__get_page(), 0);
}

// Sets the root keys, then corrupts the ble_pairing_data crc
// This tests that when setting a value, all fields in the struct must be valid.
void test_prf_store__handle_corrupt_field_during_setting(void) {
  bool FIELD_SET = true;
  prv_set_field(FIELD_SET);
  cl_assert_equal_i(pbl_bt_prf_store__get_page(), 0);

  SharedPRFData data;
  pbl_flash_read(FLASH, SPRF_PAGE_FLASH_OFFSET(pbl_bt_prf_store__get_page()), (uint8_t *)&data,
                 sizeof(data));
  cl_assert(data.root_keys.crc != 0xFFFFFFFF);

  uint32_t new_crc = 0;
  pbl_flash_write(FLASH,
                  SPRF_PAGE_FLASH_OFFSET(pbl_bt_prf_store__get_page()) +
                      offsetof(SharedPRFData, ble_pairing_data) + offsetof(SprfBlePairingData, crc),
                  (uint8_t *)&new_crc, sizeof(new_crc));

  // Confirm new CRC was written
  pbl_flash_read(FLASH, SPRF_PAGE_FLASH_OFFSET(pbl_bt_prf_store__get_page()), (uint8_t *)&data,
                 sizeof(data));
  cl_assert_equal_i(data.ble_pairing_data.crc, new_crc);

  // Should be corrupt, so after a 'set', the page number should increment even though we are
  // setting the same value
  prv_set_field(true);
  // Should have moved to the next page
  cl_assert_equal_b(pbl_bt_prf_store__get_page(), 1);
}

// Test that when we write the ble_data and the ble_name separately, a page rewrite isn't triggered
void test_prf_store__write_ble_data_and_ble_name_separately(void) {
  pbl_bt_prf_store_set_pairing(&PAIRING_INFO, nullptr, true /* auto_accept_re_pairing */);
  pbl_bt_prf_store_set_pairing(&PAIRING_INFO, DEVICE_NAME, true /* auto_accept_re_pairing */);
  // confirm we wrote to the same "Page"
  cl_assert_equal_i(pbl_bt_prf_store__get_page(), 0);
}

// Test that when deleting the ble_data, both the data and name are deleted.
// Test that when rewriting the ble_data, the same page is used (since they were previously
// marked with 0xFF's
void test_prf_store__write_ble_data_name_delete_rewrite(void) {
  pbl_bt_prf_store_set_pairing(&PAIRING_INFO, DEVICE_NAME, true /* auto_accept_re_pairing */);
  cl_assert_equal_i(pbl_bt_prf_store__get_page(), 0);
  pbl_bt_prf_store_erase_pairing();
  cl_assert_equal_i(pbl_bt_prf_store__get_page(), 2);

  char device_name_out[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  struct pbl_bt_sm_pairing_info pairing_info_out;
  const bool rv = pbl_bt_prf_store_get_pairing(&pairing_info_out, device_name_out, nullptr);
  cl_assert_equal_b(rv, false);

  pbl_bt_prf_store_set_pairing(&PAIRING_INFO, DEVICE_NAME, true /* auto_accept_re_pairing */);
  // It should detect the fields were already blank in the current page so the index should not
  // increment
  cl_assert_equal_i(pbl_bt_prf_store__get_page(), 2);
}

// Test that if we try to write the same data, the system does not force a rewrite of the page
void test_prf_store__write_repeated_data_same_page(void) {
  prv_set_field(false);
  cl_assert_equal_i(pbl_bt_prf_store__get_page(), 0);
  prv_set_field(false);
  cl_assert_equal_i(pbl_bt_prf_store__get_page(), 0);
  prv_set_field(true);
  cl_assert_equal_i(pbl_bt_prf_store__get_page(), 1);
  prv_set_field(true);
  cl_assert_equal_i(pbl_bt_prf_store__get_page(), 1);
  prv_set_field(false);
  cl_assert_equal_i(pbl_bt_prf_store__get_page(), 2);
}
