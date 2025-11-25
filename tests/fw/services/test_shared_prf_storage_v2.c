/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/shared_prf_storage/shared_prf_storage.h"
#include "services/common/system_task.h"
#include "flash_region/flash_region_s29vs.h"

#include <bluetooth/sm_types.h>

#include <string.h>

#include "clar.h"

// Fakes
//////////////////////////////////////////////////////////

#include "fake_spi_flash.h"
#include "fake_regular_timer.h"
#include "stubs_pbl_malloc.h"
#include "stubs_passert.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"

bool system_task_add_callback(SystemTaskEventCallback cb, void *data) {
  cb(data);
  return true;
}

bool sm_is_pairing_info_empty(const SMPairingInfo *p) {
  return false;
}

extern RegularTimerInfo *shared_prf_storage_get_writeback_timer(void);
static void prv_fire_writeback_timer(void) {
  fake_regular_timer_trigger(shared_prf_storage_get_writeback_timer());
}

// Tests
///////////////////////////////////////////////////////////

static SMPairingInfo s_pairing_info = (const SMPairingInfo) {
    .local_encryption_info = {
      .ediv = 123,
      .div = 456,
    },

    .remote_encryption_info = {
      .ltk = (const SMLongTermKey) {
        .data = {
          0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
          0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
        },
      },
      .rand = 0x11223344,
      .ediv = 9876,
    },

    .irk = (const SMIdentityResolvingKey) {
      .data = {
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
      },
    },
    .identity = (const BTDeviceInternal) {
      .opaque.opaque_64 = 0x1122334455667788,
    },

    .csrk = {
      .data = {
        0xcc, 0xdd, 0xee, 0xff, 0x88, 0x99, 0xaa, 0xbb,
        0x44, 0x55, 0x66, 0x77, 0x00, 0x11, 0x22, 0x33,
      },
    },

    .is_local_encryption_info_valid = true,
    .is_remote_encryption_info_valid = true,
    .is_remote_identity_info_valid = true,
    .is_remote_signing_info_valid = true,
};

void test_shared_prf_storage_v2__initialize(void) {
  fake_spi_flash_init(FLASH_REGION_SHARED_PRF_STORAGE_BEGIN,
                      FLASH_REGION_SHARED_PRF_STORAGE_END - FLASH_REGION_SHARED_PRF_STORAGE_BEGIN);
  shared_prf_storage_init();
}

void test_shared_prf_storage_v2__cleanup(void) {
  fake_spi_flash_cleanup();
}

void test_shared_prf_storage_v2__wipe_all(void) {
  SMPairingInfo sm_pairing_info;
  memset(&sm_pairing_info, 0xaa, sizeof(sm_pairing_info));
  sm_pairing_info.is_local_encryption_info_valid = true;
  sm_pairing_info.is_remote_signing_info_valid = true;
  sm_pairing_info.is_remote_identity_info_valid = true;
  shared_prf_storage_store_ble_pairing_data(&sm_pairing_info, "Blabla",
                                            false /* requires_address_pinning */, 0 /* flags */);
  prv_fire_writeback_timer();
  cl_assert_equal_b(shared_prf_storage_get_ble_pairing_data(NULL, NULL, NULL, NULL), true);
  shared_prf_storage_wipe_all();

  cl_assert_equal_b(shared_prf_storage_get_ble_pairing_data(NULL, NULL, NULL, NULL), false);
}

void test_shared_prf_storage_v2__getting_started_complete(void) {
  shared_prf_storage_wipe_all();
  cl_assert_equal_b(shared_prf_storage_get_getting_started_complete(), false);
  shared_prf_storage_set_getting_started_complete(true);
  cl_assert_equal_b(shared_prf_storage_get_getting_started_complete(), true);
  shared_prf_storage_wipe_all();
  cl_assert_equal_b(shared_prf_storage_get_getting_started_complete(), false);
}

static void prv_validate_ble_pairing_info(const SMPairingInfo *pairing_info, char *device_name) {
  char name_out[BT_DEVICE_NAME_BUFFER_SIZE];
  memset(name_out, 0, sizeof(name_out));
  SMPairingInfo pairing_info_out = {};
  cl_assert_equal_b(shared_prf_storage_get_ble_pairing_data(&pairing_info_out, name_out,
                                                            NULL, NULL), true);
  cl_assert_equal_i(strcmp(device_name, name_out), 0);
  cl_assert_equal_b(pairing_info->is_remote_signing_info_valid,
                    pairing_info_out.is_remote_signing_info_valid);
  cl_assert_equal_b(pairing_info->is_remote_identity_info_valid,
                    pairing_info_out.is_remote_identity_info_valid);
  cl_assert_equal_b(pairing_info->is_remote_encryption_info_valid,
                    pairing_info_out.is_remote_encryption_info_valid);
  cl_assert_equal_b(pairing_info->is_local_encryption_info_valid,
                    pairing_info_out.is_local_encryption_info_valid);
  cl_assert_equal_i(pairing_info->local_encryption_info.ediv,
                    pairing_info_out.local_encryption_info.ediv);
  cl_assert_equal_i(pairing_info->local_encryption_info.div,
                    pairing_info_out.local_encryption_info.div);
  cl_assert_equal_i(pairing_info->identity.opaque.opaque_64,
                    pairing_info_out.identity.opaque.opaque_64);
  cl_assert_equal_i(pairing_info->remote_encryption_info.rand,
                    pairing_info_out.remote_encryption_info.rand);
  cl_assert_equal_i(pairing_info->remote_encryption_info.ediv,
                    pairing_info_out.remote_encryption_info.ediv);
  cl_assert_equal_i(memcmp(&pairing_info->remote_encryption_info.ltk,
                           &pairing_info_out.remote_encryption_info.ltk, sizeof(SMLongTermKey)), 0);
  cl_assert_equal_i(memcmp(&pairing_info->irk, &pairing_info_out.irk,
                           sizeof(SMIdentityResolvingKey)), 0);
  cl_assert_equal_i(memcmp(&pairing_info->csrk, &pairing_info_out.csrk,
                           sizeof(SM128BitKey)), 0);

}

void test_shared_prf_storage_v2__bt_classic_and_le_pairing(void) {
  shared_prf_storage_wipe_all();
  cl_assert_equal_b(shared_prf_storage_get_bt_classic_pairing_data(NULL, NULL, NULL, NULL), false);

  // Store a classic pairing
  BTDeviceAddress addr = {
    .octets = { 0x11, 0x22, 0x33, 0x44, 0x55,},
  };
  char device_name_classic[BT_DEVICE_NAME_BUFFER_SIZE] = "CLASSIC";
  SM128BitKey link_key = {
    .data = { 0x55, },
  };
  shared_prf_storage_store_bt_classic_pairing_data(&addr, device_name_classic, &link_key, 0x00);

  // change the classic addr
  addr.octets[0] = 0x99;
  shared_prf_storage_store_bt_classic_pairing_data(&addr, device_name_classic, &link_key, 0x00);
  
  // Store an LE pairing
  char device_name_le[BT_DEVICE_NAME_BUFFER_SIZE] = "LE";
  shared_prf_storage_store_ble_pairing_data(&s_pairing_info, device_name_le,
                                            false /* requires_address_pinning */, 0 /* flags */);

  // Sync LE and Classic data
  prv_fire_writeback_timer();

  // Make sure everything checks out
  prv_validate_ble_pairing_info(&s_pairing_info, device_name_le);

  BTDeviceAddress out_addr = {};
  char device_name_out[BT_DEVICE_NAME_BUFFER_SIZE];
  memset(device_name_out, 0, BT_DEVICE_NAME_BUFFER_SIZE);
  SM128BitKey link_key_out = {};
  uint8_t platform_bits_out = 0;
  cl_assert_equal_p(shared_prf_storage_get_bt_classic_pairing_data(&out_addr, device_name_out,
                                                                   &link_key_out,
                                                                   &platform_bits_out), true);
  cl_assert_equal_i(strcmp(device_name_out, device_name_classic), 0);
  cl_assert_equal_i(memcmp(&out_addr, &addr, sizeof(addr)), 0);
  cl_assert_equal_i(memcmp(&link_key_out, &link_key_out, sizeof(link_key)), 0);
}

void test_shared_prf_storage_v2__bt_classic_pairing(void) {
  shared_prf_storage_wipe_all();
  cl_assert_equal_b(shared_prf_storage_get_bt_classic_pairing_data(NULL, NULL, NULL, NULL), false);

  BTDeviceAddress addr = {
    .octets = {
      0x11, 0x22, 0x33, 0x44, 0x55,
    },
  };
  char device_name[BT_DEVICE_NAME_BUFFER_SIZE] = "ABCDEFGHIJKLMNOPQRS";
  SM128BitKey link_key = {
    .data = {
      0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
      0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11,
    },
  };
  shared_prf_storage_store_bt_classic_pairing_data(&addr, device_name, &link_key, 0x00);
  prv_fire_writeback_timer();
  shared_prf_storage_store_platform_bits(0xaa);

  BTDeviceAddress out_addr = {};
  char device_name_out[BT_DEVICE_NAME_BUFFER_SIZE];
  memset(device_name_out, 0, BT_DEVICE_NAME_BUFFER_SIZE);
  SM128BitKey link_key_out = {};
  uint8_t platform_bits_out = 0;
  cl_assert_equal_p(shared_prf_storage_get_bt_classic_pairing_data(&out_addr, device_name_out,
                                                                   &link_key_out,
                                                                   &platform_bits_out), true);
  cl_assert_equal_i(strcmp(device_name_out, device_name), 0);
  cl_assert_equal_i(memcmp(&out_addr, &addr, sizeof(addr)), 0);
  cl_assert_equal_i(memcmp(&link_key_out, &link_key_out, sizeof(link_key)), 0);
  cl_assert_equal_i(platform_bits_out, 0xaa);

  shared_prf_storage_erase_bt_classic_pairing_data();
  cl_assert_equal_b(shared_prf_storage_get_bt_classic_pairing_data(NULL, NULL, NULL, NULL), false);
}

void test_shared_prf_storage_v2__ble_pairing(void) {
  shared_prf_storage_wipe_all();
  cl_assert_equal_b(shared_prf_storage_get_ble_pairing_data(NULL, NULL, NULL, NULL), false);


  char device_name[BT_DEVICE_NAME_BUFFER_SIZE] = "ABCDEFGHIJKLMNOPQRS";
  shared_prf_storage_store_ble_pairing_data(&s_pairing_info, device_name,
                                            false /* requires_address_pinning */, 0 /* flags */);
  prv_fire_writeback_timer();

  prv_validate_ble_pairing_info(&s_pairing_info, device_name);

  shared_prf_storage_erase_ble_pairing_data();
  cl_assert_equal_b(shared_prf_storage_get_ble_pairing_data(NULL, NULL, NULL, NULL), false);
}

void test_shared_prf_storage_v2__root_keys(void) {
  shared_prf_storage_wipe_all();

  cl_assert_equal_b(shared_prf_storage_get_root_key(SMRootKeyTypeEncryption, NULL), false);
  cl_assert_equal_b(shared_prf_storage_get_root_key(SMRootKeyTypeIdentity, NULL), false);

  SM128BitKey keys[2];
  for (int i = 0; i < sizeof(keys); ++i) {
    ((uint8_t *) keys)[i] = i;
  }

  shared_prf_storage_set_root_keys(keys);

  SM128BitKey keys_out[2];
  for (SMRootKeyType key_type = 0; key_type < SMRootKeyTypeNum; ++key_type) {
    cl_assert_equal_b(shared_prf_storage_get_root_key(key_type, &keys_out[key_type]), true);
    // It's a byte array inside, so memcmp should be OK to use:
    cl_assert_equal_i(memcmp(&keys[key_type], &keys_out[key_type], sizeof(keys[0])), 0);
  }
}

void test_shared_prf_storage_v2__local_device_name(void) {
  shared_prf_storage_wipe_all();

  cl_assert_equal_b(shared_prf_storage_get_local_device_name(NULL, 0), false);

  char device_name[BT_DEVICE_NAME_BUFFER_SIZE] = "ABCDEFGHIJKLMNOPQRS";
  shared_prf_storage_set_local_device_name(device_name);

  char device_name_out[BT_DEVICE_NAME_BUFFER_SIZE];
  cl_assert_equal_b(shared_prf_storage_get_local_device_name(device_name_out,
                                                             sizeof(device_name_out)), true);
  cl_assert_equal_s(device_name, device_name_out);
}

void test_shared_prf_storage_v2__dont_rewrite_if_no_changes(void) {
  shared_prf_storage_wipe_all();
  uint32_t write_count = fake_flash_write_count();
  uint32_t erase_count = fake_flash_erase_count();

  shared_prf_storage_wipe_all();
  // Already wiped, so no change,
  cl_assert_equal_i(write_count, fake_flash_write_count());
  cl_assert_equal_i(erase_count, fake_flash_erase_count());

  write_count = fake_flash_write_count();
  erase_count = fake_flash_erase_count();
  shared_prf_storage_set_getting_started_complete(true);
  // Expect flash getting touched:
  cl_assert(write_count < fake_flash_write_count());
  cl_assert(erase_count < fake_flash_erase_count());

  write_count = fake_flash_write_count();
  erase_count = fake_flash_erase_count();
  shared_prf_storage_set_getting_started_complete(true);
  // Expect flash NOT getting touched:
  cl_assert_equal_i(write_count, fake_flash_write_count());
  cl_assert_equal_i(erase_count, fake_flash_erase_count());
}
