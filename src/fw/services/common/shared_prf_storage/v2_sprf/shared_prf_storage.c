/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/shared_prf_storage/shared_prf_storage.h"
#include "shared_prf_storage_private.h"

#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "kernel/pbl_malloc.h"
#include "services/common/regular_timer.h"
#include "services/common/system_task.h"
#include "system/logging.h"
#include "util/size.h"

#include <bluetooth/sm_types.h>
#include <btutil/sm_util.h>
#include <os/mutex.h>

static RegularTimerInfo s_shared_prf_writeback_timer;
typedef struct {
  SharedPRFData pending_data;
  bool has_bt_classic_pairing_pending;
  bool has_ble_pairing_pending;
} SharedPRFPendingBondings;

static SharedPRFPendingBondings s_pending_bondings = { };

static PebbleMutex *s_pending_data_mutex = NULL;

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Private functions

static void prv_lock_pending_bonding(void) {
  mutex_lock(s_pending_data_mutex);
}

static void prv_unlock_pending_bonding(void) {
  mutex_unlock(s_pending_data_mutex);
}

static void prv_shared_prf_reschedule_writeback_timer(void) {
  if (regular_timer_is_scheduled(&s_shared_prf_writeback_timer)) {
    regular_timer_remove_callback(&s_shared_prf_writeback_timer);
  }
  regular_timer_add_multiminute_callback(&s_shared_prf_writeback_timer, 5);
}

// Helper to wipe memory to avoid leaking secrets stored in PRF shared storage through used stack
static void prv_cleanup_struct(SharedPRFData *data_out) {
  *data_out = (SharedPRFData){};
}

static void prv_get_empty_struct(SharedPRFData *data_out) {
  *data_out = (SharedPRFData) { .version = SHARED_PRF_STORAGE_VERSION };
}

static void prv_fetch_struct(SharedPRFData *data_out) {
  flash_read_bytes((uint8_t*) data_out, FLASH_REGION_SHARED_PRF_STORAGE_BEGIN, sizeof(*data_out));

  if (data_out->version != SHARED_PRF_STORAGE_VERSION) {
    // No data present, just return an empty struct with the current version set.
    prv_get_empty_struct(data_out);
  }
}

static void prv_perform_write(SharedPRFData *data, bool should_erase) {
  if (should_erase) {
    flash_erase_subsector_blocking(FLASH_REGION_SHARED_PRF_STORAGE_BEGIN);
  }

  flash_write_bytes((const uint8_t*) data, FLASH_REGION_SHARED_PRF_STORAGE_BEGIN, sizeof(*data));
}

static void prv_apply_patch_to_struct(void *patch, size_t size, size_t offset, bool should_erase) {
  SharedPRFData data;
  prv_fetch_struct(&data);

  uint8_t *unmodified_data_patch_start = ((uint8_t *)&data) + offset;
  // Struct is packed, so it's OK to use memcmp:
  if (memcmp(unmodified_data_patch_start, patch, size) != 0) {
    // There is new data present, so perform a write!
    memcpy(unmodified_data_patch_start, patch, size);
    prv_perform_write(&data, should_erase);
  }

  prv_cleanup_struct(&data);
}

typedef struct {
  size_t size;
  size_t offset;
  uint8_t patch[];
} WriteEraseCBData;

static void prv_perform_flash_erase_write_cb(void *data_in) {
  WriteEraseCBData *cb_data = (WriteEraseCBData*) data_in;
  prv_apply_patch_to_struct(cb_data->patch, cb_data->size, cb_data->offset, true);
  kernel_free(cb_data);
}

// Does the flash write / erase on the background task
static void prv_update_and_cleanup_struct_async(const void *data, size_t size, size_t offset) {
  WriteEraseCBData *callback_data = kernel_malloc_check(sizeof(*callback_data) + size);
  *callback_data = (WriteEraseCBData) {
    .size = size,
    .offset = offset,
  };
  memcpy(callback_data->patch, data, size);
  system_task_add_callback(prv_perform_flash_erase_write_cb, callback_data);
}

// Doesn't perform a flash erase. Should only be used to zero out parts / all of the struct
static void prv_update_and_cleanup_struct_no_erase(void *data, size_t size, size_t offset) {
  prv_apply_patch_to_struct(data, size, offset, false);
}

// Does the flash write / erase on the current task
static void prv_update_and_cleanup_struct(void *data, size_t size, size_t offset) {
  prv_apply_patch_to_struct(data, size, offset, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Custom Local Device Name

bool shared_prf_storage_get_local_device_name(char *local_device_name_out, size_t max_size) {
  SharedPRFData data;
  prv_fetch_struct(&data);

  if (local_device_name_out) {
    strncpy(local_device_name_out, data.local_device_name, max_size);
    local_device_name_out[max_size - 1] = 0;
  }
  const bool result = (data.local_device_name[0] != 0);  // Is not zero length?
  prv_cleanup_struct(&data);
  return result;
}

void shared_prf_storage_set_local_device_name(const char *local_device_name) {
  prv_update_and_cleanup_struct_async(local_device_name,
                                      MEMBER_SIZE(SharedPRFData, local_device_name),
                                      offsetof(SharedPRFData, local_device_name));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! BLE Root Keys

bool shared_prf_storage_get_root_key(SMRootKeyType key_type, SM128BitKey *key_out) {
  SharedPRFData data;
  prv_fetch_struct(&data);
  bool result = false;
  SM128BitKey nil_key = {};
  if (0 == memcmp(&nil_key, &data.root_keys[key_type], sizeof(nil_key))) {
    goto done;
  }
  if (key_out) {
    memcpy(key_out, &data.root_keys[key_type], sizeof(*key_out));
  }
  result = true;
done:
  prv_cleanup_struct(&data);
  return result;
}

void shared_prf_storage_set_root_keys(SM128BitKey *keys_in) {
#ifdef RECOVERY_FW
  // This can't be async because after it is set, sm.c accesses this key right away and will assert
  // if it isn't available yet.
  prv_update_and_cleanup_struct(keys_in, MEMBER_SIZE(SharedPRFData, root_keys),
                                offsetof(SharedPRFData, root_keys));
#else
  // This can be async because the sm.c will read this key from bt_persistent_storage instead
  prv_update_and_cleanup_struct_async(keys_in, MEMBER_SIZE(SharedPRFData, root_keys),
                                      offsetof(SharedPRFData, root_keys));
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! BLE Pairing Data

bool shared_prf_storage_get_ble_pairing_data(SMPairingInfo *pairing_info_out,
                                             char *name_out, bool *requires_address_pinning_out,
                                             uint8_t *flags) {
  SharedPRFData data;
  prv_fetch_struct(&data);

  const BLEPairingData *ble_data = &data.ble_data;

  bool result = false;
  if (!ble_data->is_local_encryption_info_valid &&
      !ble_data->is_remote_encryption_info_valid &&
      !ble_data->is_remote_identity_info_valid &&
      !ble_data->is_remote_signing_info_valid) {
    goto done;
  }

  if (pairing_info_out) {
    *pairing_info_out = (const SMPairingInfo) {};
    pairing_info_out->local_encryption_info.ediv = ble_data->local_ediv;
    pairing_info_out->local_encryption_info.div = ble_data->local_div;
    pairing_info_out->remote_encryption_info.ltk = ble_data->ltk;
    pairing_info_out->remote_encryption_info.rand = ble_data->rand;
    pairing_info_out->remote_encryption_info.ediv = ble_data->ediv;
    pairing_info_out->irk = ble_data->irk;
    pairing_info_out->identity = ble_data->identity;
    pairing_info_out->csrk = ble_data->csrk;
    pairing_info_out->is_local_encryption_info_valid = ble_data->is_local_encryption_info_valid;
    pairing_info_out->is_remote_encryption_info_valid = ble_data->is_remote_encryption_info_valid;
    pairing_info_out->is_remote_identity_info_valid = ble_data->is_remote_identity_info_valid;
    pairing_info_out->is_remote_signing_info_valid = ble_data->is_remote_signing_info_valid;
  }
  if (name_out) {
    strncpy(name_out, data.ble_data.name, BT_DEVICE_NAME_BUFFER_SIZE);
  }
  if (requires_address_pinning_out) {
    // Not supported in v2
    *requires_address_pinning_out = false;
  }
  if (flags) {
    // Not supported in v2
    *flags = 0;
  }
  result = true;
done:
  prv_cleanup_struct(&data);
  return result;
}

static void prv_load_ble_pairing_data(BLEPairingData *data,
                                      const SMPairingInfo *pairing_info,
                                      const char *name) {
  *data = (BLEPairingData) {
    .local_ediv = pairing_info->local_encryption_info.ediv,
    .local_div = pairing_info->local_encryption_info.div,

    .ltk = pairing_info->remote_encryption_info.ltk,
    .rand = pairing_info->remote_encryption_info.rand,
    .ediv = pairing_info->remote_encryption_info.ediv,

    .irk = pairing_info->irk,
    .identity = pairing_info->identity,

    .csrk = pairing_info->csrk,

    .is_local_encryption_info_valid = pairing_info->is_local_encryption_info_valid,
    .is_remote_encryption_info_valid = pairing_info->is_remote_encryption_info_valid,
    .is_remote_identity_info_valid = pairing_info->is_remote_identity_info_valid,
    .is_remote_signing_info_valid = pairing_info->is_remote_signing_info_valid,
  };
  if (name) {
    strncpy(data->name, name, BT_DEVICE_NAME_BUFFER_SIZE);
  }
}

static void prv_shared_prf_storage_store_ble_pairing_data(BLEPairingData *data) {
#ifdef RECOVERY_FW
  // The callers of bt_persistent_storage expect this store to be synchronous.
  // In PRF bt_persistent_storage is just a wrapper for this
  prv_update_and_cleanup_struct(data, sizeof(*data),
                                offsetof(SharedPRFData, ble_data));
#else
  prv_update_and_cleanup_struct_async(data, sizeof(*data),
                                      offsetof(SharedPRFData, ble_data));
#endif
}

void shared_prf_storage_store_ble_pairing_data(
    const SMPairingInfo *pairing_info, const char *name, bool requires_address_pinning,
    uint8_t flags) {
  if (!pairing_info || sm_is_pairing_info_empty(pairing_info)) {
    PBL_LOG(LOG_LEVEL_WARNING, "PRF Storage: Attempting to store an NULL or empty pairing info");
    return;
  }

#ifdef RECOVERY_FW
  BLEPairingData data;
  prv_load_ble_pairing_data(&data, pairing_info, name);
  prv_shared_prf_storage_store_ble_pairing_data(&data);
#else
  prv_lock_pending_bonding();
  {
    shared_prf_storage_erase_ble_pairing_data();
    prv_load_ble_pairing_data(&s_pending_bondings.pending_data.ble_data, pairing_info, name);
    s_pending_bondings.has_ble_pairing_pending = true;
    prv_shared_prf_reschedule_writeback_timer();
  }
  prv_unlock_pending_bonding();
#endif
}

void shared_prf_storage_erase_ble_pairing_data(void) {
  BLEPairingData empty_data = {};

  // Call the version that doesn't do a flash erase because we are only writing zeros.
  prv_update_and_cleanup_struct_no_erase(&empty_data, sizeof(empty_data),
                                                      offsetof(SharedPRFData, ble_data));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! BT Classic Pairing Data

bool shared_prf_storage_get_bt_classic_pairing_data(BTDeviceAddress *addr_out,
                                                    char *device_name_out,
                                                    SM128BitKey *link_key_out,
                                                    uint8_t *platform_bits) {
  SharedPRFData data;
  prv_fetch_struct(&data);

  bool result = false;
  const BTDeviceAddress invalid_address = (BTDeviceAddress) {};
  if (memcmp(&data.bt_classic_data.address, &invalid_address, sizeof(invalid_address)) == 0) {
    PBL_LOG(LOG_LEVEL_WARNING, "Invalid pairing stored");
    goto done;
  }

  if (addr_out) {
    *addr_out = data.bt_classic_data.address;
  }
  if (link_key_out) {
    *link_key_out = data.bt_classic_data.link_key;
  }
  if (platform_bits) {
    *platform_bits = data.bt_classic_data.platform_bits;
  }
  if (device_name_out) {
    strcpy(device_name_out, data.bt_classic_data.name);
  }
  result = true;
done:
  prv_cleanup_struct(&data);
  return result;
}

static void prv_load_bt_classic_pairing_data(
    BTClassicPairingData *data, BTDeviceAddress *addr, const char *device_name,
    SM128BitKey *link_key, uint8_t platform_bits) {
  *data = (BTClassicPairingData) {
    .address = *addr,
    .link_key = *link_key,
    .platform_bits = platform_bits
  };
  strncpy(data->name, device_name, BT_DEVICE_NAME_BUFFER_SIZE);
}

static void prv_shared_prf_storage_store_bt_classic_pairing_data(
    BTDeviceAddress *addr, const char *device_name, SM128BitKey *link_key,
    uint8_t platform_bits) {
  if (link_key) {
    // New pairing
    BTClassicPairingData data;
    prv_load_bt_classic_pairing_data(&data, addr, device_name, link_key, platform_bits);

    prv_update_and_cleanup_struct_async(&data,
                                        sizeof(BTClassicPairingData),
                                        offsetof(SharedPRFData, bt_classic_data));
  } else {
    // Just updating the name
    prv_update_and_cleanup_struct_async(device_name, MEMBER_SIZE(BTClassicPairingData, name),
                offsetof(SharedPRFData, bt_classic_data) + offsetof(BTClassicPairingData, name));
  }
}

void shared_prf_storage_store_bt_classic_pairing_data(
    BTDeviceAddress *addr, const char *device_name, SM128BitKey *link_key, uint8_t platform_bits) {
  if (!addr || !device_name) {
    PBL_LOG(LOG_LEVEL_WARNING, "PRF Storage: Can't store this BT classic pairing");
    return;
  }

#ifdef RECOVERY_FW
  prv_shared_prf_storage_store_bt_classic_pairing_data(addr, device_name, link_key, platform_bits);
#else
  prv_lock_pending_bonding();
  {
    shared_prf_storage_erase_bt_classic_pairing_data();
    prv_load_bt_classic_pairing_data(&s_pending_bondings.pending_data.bt_classic_data, addr,
                                     device_name, link_key, platform_bits);
    s_pending_bondings.has_bt_classic_pairing_pending = true;
    prv_shared_prf_reschedule_writeback_timer();
  }
  prv_unlock_pending_bonding();
#endif
}

void shared_prf_storage_store_platform_bits(uint8_t platform_bits) {
  prv_update_and_cleanup_struct_async(&platform_bits,
                                      sizeof(platform_bits),
                                      offsetof(SharedPRFData, bt_classic_data) +
                                      offsetof(BTClassicPairingData, platform_bits));
}

void shared_prf_storage_erase_bt_classic_pairing_data(void) {
  BTClassicPairingData empty_data = {};

  // Call the version that doesn't do a flash erase because we are only writing zeros.
  prv_update_and_cleanup_struct_no_erase(&empty_data, sizeof(empty_data),
                                                      offsetof(SharedPRFData, bt_classic_data));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Getting Started Is Complete

bool shared_prf_storage_get_getting_started_complete(void) {
  SharedPRFData data;
  prv_fetch_struct(&data);
  const bool is_complete = data.getting_started_is_complete;
  prv_cleanup_struct(&data);
  return is_complete;
}

void shared_prf_storage_set_getting_started_complete(bool is_complete) {
  prv_update_and_cleanup_struct_async(&is_complete,
                                      sizeof(is_complete),
                                      offsetof(SharedPRFData, getting_started_is_complete));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Factory Reset

void shared_prf_storage_wipe_all(void) {
  SharedPRFData data;
  prv_get_empty_struct(&data);
  prv_update_and_cleanup_struct(&data, sizeof(data), 0);
}

static void prv_system_task_prf_update_cb(void *unused) {
  PBL_LOG(LOG_LEVEL_DEBUG, "Syncing pairing information to SPRF");
  prv_lock_pending_bonding();
  {
    bool le_bonding_update = s_pending_bondings.has_ble_pairing_pending;
    bool classic_bonding_update = s_pending_bondings.has_bt_classic_pairing_pending;

    SharedPRFData *data = &s_pending_bondings.pending_data;

    if (le_bonding_update && classic_bonding_update) {
      prv_update_and_cleanup_struct_async(
          &data->ble_data, sizeof(BLEPairingData) + sizeof(BTClassicPairingData),
          offsetof(SharedPRFData, ble_data));
    } else if (classic_bonding_update) {
      prv_update_and_cleanup_struct_async(&data->bt_classic_data, sizeof(BTClassicPairingData),
                                        offsetof(SharedPRFData, bt_classic_data));
    } else if (le_bonding_update) {
      prv_shared_prf_storage_store_ble_pairing_data(&data->ble_data);
    }

    memset(&s_pending_bondings, 0x00, sizeof(s_pending_bondings));
  }
  prv_unlock_pending_bonding();
}

static void prv_async_shared_prf_update_timer_cb(void *data) {
  system_task_add_callback(prv_system_task_prf_update_cb, NULL);

  prv_lock_pending_bonding();
  {
    regular_timer_remove_callback(&s_shared_prf_writeback_timer);
  }
  prv_unlock_pending_bonding();
}

void shared_prf_storage_init(void) {
  s_pending_data_mutex = mutex_create();

  s_shared_prf_writeback_timer = (const RegularTimerInfo) {
    .cb = prv_async_shared_prf_update_timer_cb,
  };
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Pinned Address Stubs

bool shared_prf_storage_get_ble_pinned_address(BTDeviceAddress *address_out) {
  return false;
}

//! Stores the new BLE Pinned Address in the shared storage.
void shared_prf_storage_set_ble_pinned_address(const BTDeviceAddress *address) {
}

void command_force_shared_prf_flush(void) {
  if (regular_timer_is_scheduled(&s_shared_prf_writeback_timer)) {
    regular_timer_remove_callback(&s_shared_prf_writeback_timer);
    s_shared_prf_writeback_timer.cb(NULL);
  }
}

//! For unit tests
RegularTimerInfo *shared_prf_storage_get_writeback_timer(void) {
  return &s_shared_prf_writeback_timer;
}
