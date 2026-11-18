/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/prf_store.h>
#include <pbl/bluetooth/prf_store_layout.h>
#include <pbl/board/partitions.h>
#include <pbl/crc/crc.h>
#include <pbl/drivers/flash.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>

PBL_LOG_MODULE_DECLARE(nimble, CONFIG_NIMBLE_LOG_LEVEL);

#define SPRF_NUM_PAGES (PART_SHARED_PRF_STORAGE_SIZE / sizeof(SharedPRFData))

#define SPRF_PAGE_FLASH_ADDR(idx) (PART_SHARED_PRF_STORAGE_OFFSET + (idx * sizeof(SharedPRFData)))
// CRC Unwritten state and size
#define SPRF_UNWRITTEN_CRC ((uint32_t)0xFFFFFFFF)
#define SPRF_CRC_SIZE      (sizeof(uint32_t))

// Accessors to a fields data and size. Basically skips over the CRC for the field.
#define SPRF_FIELD_DATA(f)       (((uint8_t *)f) + SPRF_CRC_SIZE)
#define SPRF_FIELD_DATA_SIZE(sz) (sz - SPRF_CRC_SIZE)

// Accessors for the CRC of a field
// A field always has the CRC as the first element, and the CRC is 4 bytes long
#define FIELD_CRC_FROM_FIELD(f) (*(uint32_t *)f)
// 'DATA', or 'd', is SharedPRFData struct. Offset, or 'off', is the offset of the field from the
// data struct.
#define FIELD_CRC_FROM_DATA(d, off) *((uint32_t *)(((uint8_t *)d) + off))

// Tests if a bit is set in the flags variable
#define SPRF_FLAG_IS_SET(flags, bit) (flags & bit)
// Returns a bit in the correct place
#define SPRF_FLAG_GET_BIT(b, bit) ((b) ? bit : 0)

#define SPRF_CUR_VERSION 0x02

// Keeps track of the current page within the region that the valid (or empty) page is
static uint32_t s_valid_page_idx;
static PBL_MUTEX_DEFINE(s_sprf_mutex);

//
// Helper functions
//

static void prv_lock(void) {
  pbl_mutex_lock(&s_sprf_mutex, PBL_FOREVER);
}

static void prv_unlock(void) {
  pbl_mutex_unlock(&s_sprf_mutex);
}

static bool prv_buffer_empty(const uint8_t *buf, size_t num_bytes) {
  for (uint32_t i = 0; i < num_bytes; i++) {
    if (buf[i] != 0xFF) {
      return false;
    }
  }
  return true;
}

static uint32_t prv_current_page_flash_addr(void) {
  return SPRF_PAGE_FLASH_ADDR(s_valid_page_idx);
}

static SprfMagic prv_get_magic_for_page(uint32_t page) {
  SprfMagic magic;
  pbl_flash_read(FLASH, SPRF_PAGE_FLASH_ADDR(page), (uint8_t *)&magic, sizeof(magic));
  return magic;
}

//
// Struct validators
//

//! Pass a field (starting at address of CRC) and a field size. Return whether the field is valid.
//! A valid struct means that:
//    1. The field has not been written to (both CRC and field data are blank)
//    2. The CRC and the written data match
static bool prv_field_valid(const uint8_t *field, size_t field_size) {
  const bool empty = prv_buffer_empty(field, field_size);
  if (empty) {
    return true;
  }

  const uint32_t field_crc = *(uint32_t *)field;
  const bool valid_crc =
      (field_crc == pbl_crc32(0, SPRF_FIELD_DATA(field), SPRF_FIELD_DATA_SIZE(field_size)));
  return valid_crc;
}

//! Return whether the entire SharedPRFData given as an argument is valid.
//! It checks:
//!   1. That the struct header is either ValidEntry or UnpopulatedEntry.
//!   2. If the struct is an UnpopulatedEntry, it ensures the struct is entirely empty
//!   3. If the struct is *not* an UnpopulatedEntry, it ensures that each field is either empty
//!      or written with a valid CRC.
static bool prv_valid_struct(SharedPRFData *data) {
  if (data->magic == SprfMagic_UnpopulatedEntry &&
      prv_buffer_empty((uint8_t *)data, sizeof(*data))) {
    return true;
  }

  if (data->magic == SprfMagic_ValidEntry &&
      (prv_field_valid((uint8_t *)&data->root_keys, sizeof(data->root_keys)) &&
       prv_field_valid((uint8_t *)&data->ble_pairing_data, sizeof(data->ble_pairing_data)) &&
       prv_field_valid((uint8_t *)&data->ble_pairing_name, sizeof(data->ble_pairing_name)) &&
       prv_field_valid((uint8_t *)&data->pinned_address, sizeof(data->pinned_address)) &&
       prv_field_valid((uint8_t *)&data->local_name, sizeof(data->local_name)) &&
       prv_field_valid((uint8_t *)&data->getting_started, sizeof(data->getting_started)))) {
    return true;
  }

  return false;
}

// Stored struct setters

static void prv_write_to_current_page(SharedPRFData *data, bool write_metadata) {
  if (data) {
    if (write_metadata) {
      data->magic = SprfMagic_ValidEntry;
      data->version = SPRF_CUR_VERSION;
    }
    pbl_flash_write(FLASH, prv_current_page_flash_addr(), (uint8_t *)data, sizeof(*data));
  }
}

static void prv_erase_region_and_save(SharedPRFData *data) {
  pbl_flash_partition_erase(PART_SHARED_PRF_STORAGE, 0U, PART_SHARED_PRF_STORAGE_SIZE);
  s_valid_page_idx = 0;
  prv_write_to_current_page(data, false);
}

static void prv_invalidate_current_page(void) {
  PBL_LOG_DBG("Invalidating current page: #%" PRIu32, s_valid_page_idx);
  // First, check if the page is Unpopulated
  SprfMagic magic = prv_get_magic_for_page(s_valid_page_idx);
  if (magic == SprfMagic_UnpopulatedEntry) {
    // This page is already Unpopulated. No need for invalidating
    return;
  }

  // Invalidate current page
  SprfMagic new_magic = SprfMagic_InvalidatedEntry;
  pbl_flash_write(FLASH, prv_current_page_flash_addr(), (uint8_t *)&new_magic, sizeof(SprfMagic));
  s_valid_page_idx++;

  // Sanity check to make sure that the page we are moving to is actually empty.
  if ((s_valid_page_idx >= SPRF_NUM_PAGES) ||
      (prv_get_magic_for_page(s_valid_page_idx) != SprfMagic_UnpopulatedEntry)) {
    PBL_LOG_WRN("Ran out of pages or found corrupted next page, erasing region");
    // NOTE: This should not happen often. On boot, we delete and rewrite the region if >75% of
    // regions are filled. In the worst case, this will happen if the user pair/repairs
    // NUM_REGIONS * .25 times without rebooting in between. (e.g. 16 pages.
    // We boot up on sector 12. User pairs 4 times, we now want to access page 16,
    // that is past our region, we need to clean up.
    //
    // We've run out of blank pages. Delete the entire region and roll around to the front.
    // This will take some time.
    prv_erase_region_and_save(nullptr);
    s_valid_page_idx = 0;
  }
}

//
// SharedPRFData allocators, deallocators, and getters
//

static void prv_fetch_struct(SharedPRFData *data_out) {
  pbl_flash_read(FLASH, prv_current_page_flash_addr(), (uint8_t *)data_out, sizeof(*data_out));

  if (!prv_valid_struct(data_out)) {
    PBL_LOG_WRN("Shared PRF Storage sector # %" PRIu32
                " is corrupted. Invalidating"
                " and starting a new one",
                s_valid_page_idx);
    prv_invalidate_current_page();
    memset(data_out, 0xFF, sizeof(*data_out));
  }
}

static SharedPRFData *prv_alloc_and_fetch_struct(void) {
  SharedPRFData *data = pbl_zalloc_check(sizeof(SharedPRFData));
  prv_fetch_struct(data);
  return data;
}

static void prv_dealloc_struct(SharedPRFData *data) {
  pbl_free(data);
}

static void prv_persist_field(uint8_t *field, size_t offset, size_t field_size, bool calc_crc) {
  SharedPRFData *data = prv_alloc_and_fetch_struct();

  const size_t field_data_size = SPRF_FIELD_DATA_SIZE(field_size);
  const uint32_t old_crc = FIELD_CRC_FROM_DATA(data, offset);
  const uint32_t new_crc =
      (calc_crc) ? pbl_crc32(0, SPRF_FIELD_DATA(field), field_data_size) : SPRF_UNWRITTEN_CRC;
  const bool same_data =
      (0 == memcmp(SPRF_FIELD_DATA(field), SPRF_FIELD_DATA(((uint8_t *)data) + offset),
                   field_data_size));

  if (data->magic == SprfMagic_UnpopulatedEntry) {
    // Struct that is written is currently unpopulated. Set it up and write
    // the struct's Magic and Version
    prv_write_to_current_page(data, true);
  } else if ((old_crc == new_crc) && same_data) {
    // We are trying to write the same data, ignore the write
    goto cleanup;
  } else if (old_crc != SPRF_UNWRITTEN_CRC) {
    // We are writing different data. Clear the field, write the struct again, and later write
    // the new data in the empty field
    memset(((uint8_t *)data) + offset, 0xFF, field_size);
    prv_invalidate_current_page();
    prv_write_to_current_page(data, true);
  }

  PBL_LOG_DBG("Overwriting SPRF field at offset %d, size %d", (int)offset, (int)field_size);

  // write the crc first so it's easier to detect a non empty field (we can just read if the CRC is
  // not 0xFFFFFFFF instead of comparing all bytes.

  *(uint32_t *)field = new_crc;
  pbl_flash_write(FLASH, prv_current_page_flash_addr() + offset, field, field_size);

cleanup:
  prv_dealloc_struct(data);
}

static void prv_erase_field(size_t offset, size_t field_size) {
  SharedPRFData *data = prv_alloc_and_fetch_struct();
  uint8_t *field_ptr = ((uint8_t *)data) + offset;
  memset(field_ptr, 0xFF, field_size);
  prv_persist_field(field_ptr, offset, field_size, false);
  prv_dealloc_struct(data);
}

static bool prv_fetch_field(uint8_t *field_out, size_t offset, size_t field_size) {
  pbl_flash_read(FLASH, prv_current_page_flash_addr() + offset, field_out, field_size);
  if (!prv_field_valid(field_out, field_size)) {
    // If corrupted field, delete entire page
    PBL_LOG_WRN("Shared PRF Storage sector # %" PRIu32
                " is corrupted. Invalidating"
                " and starting a new one",
                s_valid_page_idx);
    prv_invalidate_current_page();
    return false;
  }

  uint32_t written_crc = FIELD_CRC_FROM_FIELD(field_out);
  if (written_crc == SPRF_UNWRITTEN_CRC) {
    // If empty field, return that we have invalid data too
    return false;
  }

  return true;
}

#define SPRF_PERSIST_FIELD(data, name) \
  prv_persist_field((uint8_t *)&data, offsetof(SharedPRFData, name), sizeof(data), true)
#define SPRF_ERASE_FIELD(name) \
  prv_erase_field(offsetof(SharedPRFData, name), sizeof(((SharedPRFData *)0)->name))
#define SPRF_FETCH_FIELD(data, name) \
  prv_fetch_field((uint8_t *)&data, offsetof(SharedPRFData, name), sizeof(data))

//!
//! SharedPRFStorage API
//!

//! Scans through the shared PRF flash region and finds the valid entry.
//! (Should only ever be one!) If we are > 75% through the shared PRF region,
//! erase the sector and re-write the info at offset 0. We want to make the chance
//! of blocking on an erase ~0, by doing this prep on init.
void pbl_bt_prf_store_init(void) {
  prv_lock();
  {
    s_valid_page_idx = SPRF_PAGE_IDX_INVALID;

    SharedPRFData data = {};
    SprfMagic page_magic = 0;

    for (uint32_t i = 0; i < SPRF_NUM_PAGES; i++) {
      page_magic = prv_get_magic_for_page(i);
      // Check the magic to see if we need to investigate further and read the entire contents.
      if (page_magic == SprfMagic_ValidEntry || page_magic == SprfMagic_UnpopulatedEntry) {
        pbl_flash_read(FLASH, SPRF_PAGE_FLASH_ADDR(i), (uint8_t *)&data, sizeof(data));
        if (prv_valid_struct(&data)) {
          s_valid_page_idx = i;
          break;
        }
      }
    }

    // Keep a write offset, this won't work when we try to roll over to the other 25% of the sectors
    if (s_valid_page_idx == SPRF_PAGE_IDX_INVALID) {
      prv_erase_region_and_save(nullptr);
    } else if (s_valid_page_idx > (SPRF_MAX_NUM_PAGES_MULT(SPRF_NUM_PAGES))) {
      prv_erase_region_and_save(&data);
    }
  }
  prv_unlock();
}

void pbl_bt_prf_store_wipe(void) {
  prv_lock();
  prv_invalidate_current_page();
  prv_unlock();
}

//!
//! BLE Root Key APIs
//!

bool pbl_bt_prf_store_get_root_key(enum pbl_bt_sm_root_key_type key_type,
                                   struct pbl_bt_sm_key *key_out) {
  bool rv;
  prv_lock();
  {
    SprfRootKeys data;
    rv = SPRF_FETCH_FIELD(data, root_keys);
    if (!rv) {
      goto unlock;
    }

    struct pbl_bt_sm_key nil_key = {};
    if (0 == memcmp(&nil_key, &data.keys[key_type], sizeof(nil_key))) {
      rv = false;
      goto unlock;
    }
    if (key_out) {
      memcpy(key_out, &data.keys[key_type], sizeof(*key_out));
    }

    rv = true;
  }

unlock:
  prv_unlock();
  return rv;
}

void pbl_bt_prf_store_set_root_keys(struct pbl_bt_sm_key *keys_in) {
  prv_lock();
  {
    SprfRootKeys data = {};
    if (keys_in) {
      memcpy(&data.keys, keys_in, sizeof(data.keys));
    }
    SPRF_PERSIST_FIELD(data, root_keys);
  }
  prv_unlock();
}

//!
//! BLE Pairing Data APIs
//!

bool pbl_bt_prf_store_get_pairing(struct pbl_bt_sm_pairing_info *pairing_info_out, char *name_out,
                                  uint8_t *flags) {
  bool rv;
  prv_lock();
  {
    SprfBlePairingData data;
    rv = SPRF_FETCH_FIELD(data, ble_pairing_data);
    if (!rv) {
      goto unlock;
    }

    if (!data.fields) {
      // The pairing stored is empty
      rv = false;
      goto unlock;
    }

    if (pairing_info_out) {
      *pairing_info_out = (struct pbl_bt_sm_pairing_info){
        .local_encryption_info.ltk = data.l_ltk,
        .local_encryption_info.ediv = data.l_ediv,
        .local_encryption_info.rand = data.l_rand,

        .remote_encryption_info.ltk = data.r_ltk,
        .remote_encryption_info.ediv = data.r_ediv,
        .remote_encryption_info.rand = data.r_rand,

        .irk = data.irk,
        .identity = data.identity,
        .csrk = data.csrk,

        .is_mitm_protection_enabled = data.is_mitm_protection_enabled,

        .is_local_encryption_info_valid =
            SPRF_FLAG_IS_SET(data.fields, SprfValidFields_LocalEncryptionInfoValid),
        .is_remote_encryption_info_valid =
            SPRF_FLAG_IS_SET(data.fields, SprfValidFields_RemoteEncryptionInfoValid),
        .is_remote_identity_info_valid =
            SPRF_FLAG_IS_SET(data.fields, SprfValidFields_RemoteIdentityInfoValid),
        .is_remote_signing_info_valid =
            SPRF_FLAG_IS_SET(data.fields, SprfValidFields_RemoteSigningInfoValid),
      };
    }

    if (flags) {
      *flags = data.flags;
    }

    if (name_out) {
      SprfBlePairingName name_data = {};
      const bool name_rv = SPRF_FETCH_FIELD(name_data, ble_pairing_name);
      // Should we return a failure on a failed name get?
      if (name_rv) {
        strncpy(name_out, name_data.name, PBL_BT_DEVICE_NAME_BUFFER_SIZE);
      } else {
        name_out[0] = '\0';
      }
    }
    rv = true;
  }

unlock:
  prv_unlock();
  return rv;
}

void pbl_bt_prf_store_set_pairing(const struct pbl_bt_sm_pairing_info *pairing_info,
                                  const char *name, uint8_t flags) {
  if (!pairing_info || !(pairing_info->is_local_encryption_info_valid ||
                         pairing_info->is_remote_encryption_info_valid ||
                         pairing_info->is_remote_identity_info_valid ||
                         pairing_info->is_remote_signing_info_valid)) {
    PBL_LOG_WRN("PRF Storage: Attempting to store an NULL or empty pairing info");
    return;
  }

  prv_lock();
  {
    SprfBlePairingData data = (SprfBlePairingData){
      .l_ediv = pairing_info->local_encryption_info.ediv,
      .l_ltk = pairing_info->local_encryption_info.ltk,
      .l_rand = pairing_info->local_encryption_info.rand,

      .r_ediv = pairing_info->remote_encryption_info.ediv,
      .r_ltk = pairing_info->remote_encryption_info.ltk,
      .r_rand = pairing_info->remote_encryption_info.rand,

      .irk = pairing_info->irk,
      .identity = pairing_info->identity,

      .csrk = pairing_info->csrk,

      .is_mitm_protection_enabled = pairing_info->is_mitm_protection_enabled,

      .flags = flags,

      .fields = SPRF_FLAG_GET_BIT(pairing_info->is_local_encryption_info_valid,
                                  SprfValidFields_LocalEncryptionInfoValid) |
                SPRF_FLAG_GET_BIT(pairing_info->is_remote_encryption_info_valid,
                                  SprfValidFields_RemoteEncryptionInfoValid) |
                SPRF_FLAG_GET_BIT(pairing_info->is_remote_identity_info_valid,
                                  SprfValidFields_RemoteIdentityInfoValid) |
                SPRF_FLAG_GET_BIT(pairing_info->is_remote_signing_info_valid,
                                  SprfValidFields_RemoteSigningInfoValid),
    };

    SPRF_PERSIST_FIELD(data, ble_pairing_data);

    if (name) {
      // only persist name if one is actually included
      SprfBlePairingName name_data = {};
      strncpy(name_data.name, name, sizeof(name_data.name));
      SPRF_PERSIST_FIELD(name_data, ble_pairing_name);
    }
  }
  prv_unlock();
}

void pbl_bt_prf_store_set_pairing_name(const char *name) {
  prv_lock();
  {
    SprfBlePairingData data;
    SprfBlePairingName name_data = {};

    if (SPRF_FETCH_FIELD(data, ble_pairing_data) && data.fields) {
      strncpy(name_data.name, name, sizeof(name_data.name));
      SPRF_PERSIST_FIELD(name_data, ble_pairing_name);
    }
  }
  prv_unlock();
}

void pbl_bt_prf_store_erase_pairing(void) {
  prv_lock();
  {
    SPRF_ERASE_FIELD(ble_pairing_data);
    SPRF_ERASE_FIELD(ble_pairing_name);
  }
  prv_unlock();
}

//!
//! Unit test functions
//!

uint16_t pbl_bt_prf_store__get_page(void) {
  return s_valid_page_idx;
}

void pbl_bt_prf_store__set_page(uint32_t page_num) {
  s_valid_page_idx = page_num;
}
