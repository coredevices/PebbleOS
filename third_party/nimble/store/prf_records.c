/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "prf_records_layout.h"

#include <string.h>

#include <pbl/bluetooth/prf_store.h>
#include <pbl/board/partitions.h>
#include <pbl/crc/crc.h>
#include <pbl/drivers/flash.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>

#include <host/ble_store.h>

PBL_LOG_MODULE_DECLARE(nimble, CONFIG_NIMBLE_LOG_LEVEL);

#define NUM_SLOTS (PART_SHARED_PRF_STORAGE_SIZE / sizeof(struct prf_record))

// Past this many slots the partition is erased at start, so that a change rarely waits for it.
#define COMPACT_SLOTS (NUM_SLOTS * 3U / 4U)

#define NO_SLOT UINT32_MAX

#define PAIRING_FLAGS                                                              \
  (PRF_RECORD_FLAG_PAIRING | PRF_RECORD_FLAG_PEER_LTK | PRF_RECORD_FLAG_PEER_IRK | \
   PRF_RECORD_FLAG_OUR_LTK | PRF_RECORD_FLAG_SECURE_CONNECTIONS | PRF_RECORD_FLAG_AUTHENTICATED)

static_assert(PRF_RECORD_NAME_SIZE == PBL_BT_DEVICE_NAME_BUFFER_SIZE);
static_assert(PRF_RECORD_NUM_ROOT_KEYS == PBL_BT_SM_ROOT_KEY_TYPE_NUM);
static_assert(PRF_RECORD_KEY_SIZE == sizeof(struct pbl_bt_sm_key));

static PBL_MUTEX_DEFINE(s_lock);

// Under s_lock: the slot of the last valid record, and the first one nothing was written to.
static uint32_t s_cur = NO_SLOT;
static uint32_t s_free;

static uint32_t prv_slot_addr(uint32_t slot) {
  return PART_SHARED_PRF_STORAGE_OFFSET + slot * sizeof(struct prf_record);
}

static uint32_t prv_crc(const struct prf_record *record) {
  const size_t skip = offsetof(struct prf_record, crc) + sizeof(record->crc);

  return pbl_crc32(0, (const uint8_t *)record + skip, sizeof(*record) - skip);
}

static bool prv_is_erased(const struct prf_record *record) {
  const uint8_t *bytes = (const uint8_t *)record;

  for (size_t i = 0U; i < sizeof(*record); i++) {
    if (bytes[i] != 0xFFU) {
      return false;
    }
  }

  return true;
}

static bool prv_is_intact(const struct prf_record *record) {
  return record->magic == PRF_RECORD_MAGIC && record->crc == prv_crc(record);
}

static void prv_erase(void) {
  pbl_flash_partition_erase(PART_SHARED_PRF_STORAGE, 0U, PART_SHARED_PRF_STORAGE_SIZE);
  s_cur = NO_SLOT;
  s_free = 0U;
}

// The store as it is: nothing in it if there is no record.
static void prv_load(struct prf_record *record) {
  if (s_cur == NO_SLOT) {
    memset(record, 0, sizeof(*record));
    return;
  }

  pbl_flash_read(FLASH, prv_slot_addr(s_cur), record, sizeof(*record));
}

static void prv_append(const struct prf_record *record) {
  if (s_free >= NUM_SLOTS) {
    prv_erase();
  }

  pbl_flash_write(FLASH, prv_slot_addr(s_free), record, sizeof(*record));
  s_cur = s_free++;
}

static void prv_save(struct prf_record *record) {
  struct prf_record current;

  record->magic = PRF_RECORD_MAGIC;
  record->version = PRF_RECORD_VERSION;
  memset(record->rsvd, 0, sizeof(record->rsvd));
  record->crc = prv_crc(record);

  prv_load(&current);
  if (s_cur == NO_SLOT) {
    if (record->flags == 0U) {
      return;
    }
  } else if (memcmp(&current, record, sizeof(current)) == 0) {
    return;
  }

  prv_append(record);
}

static void prv_clear_pairing(struct prf_record *record) {
  record->flags &= (uint8_t)~PAIRING_FLAGS;
  record->peer_addr_type = 0U;
  memset(record->peer_addr, 0, sizeof(record->peer_addr));
  record->peer_ediv = 0U;
  record->our_ediv = 0U;
  record->peer_rand = 0U;
  record->our_rand = 0U;
  memset(record->peer_ltk, 0, sizeof(record->peer_ltk));
  memset(record->peer_irk, 0, sizeof(record->peer_irk));
  memset(record->our_ltk, 0, sizeof(record->our_ltk));
}

static void prv_set_name(struct prf_record *record, const char *name) {
  memset(record->name, 0, sizeof(record->name));
  strncpy(record->name, name, sizeof(record->name) - 1U);
}

void pbl_bt_prf_store_init(void) {
  struct prf_record record;
  bool has_record;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  s_cur = NO_SLOT;
  s_free = 0U;

  for (uint32_t slot = 0U; slot < NUM_SLOTS; slot++) {
    pbl_flash_read(FLASH, prv_slot_addr(slot), &record, sizeof(record));
    if (prv_is_erased(&record)) {
      continue;
    }

    s_free = slot + 1U;
    if (!prv_is_intact(&record)) {
      continue;
    }

    // One of another version cannot be read, but still replaces those before it.
    s_cur = (record.version == PRF_RECORD_VERSION) ? slot : NO_SLOT;
  }

  if (s_free > COMPACT_SLOTS) {
    has_record = (s_cur != NO_SLOT);
    prv_load(&record);
    prv_erase();
    if (has_record) {
      prv_append(&record);
    }
  }

  pbl_mutex_unlock(&s_lock);
}

void pbl_bt_prf_store_wipe(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  prv_erase();
  pbl_mutex_unlock(&s_lock);
}

bool pbl_bt_prf_store_get_root_key(enum pbl_bt_sm_root_key_type key_type,
                                   struct pbl_bt_sm_key *key_out) {
  static const uint8_t s_no_key[PRF_RECORD_KEY_SIZE];
  struct prf_record record;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  prv_load(&record);
  pbl_mutex_unlock(&s_lock);

  if ((record.flags & PRF_RECORD_FLAG_ROOT_KEYS) == 0U ||
      memcmp(record.root_keys[key_type], s_no_key, sizeof(s_no_key)) == 0) {
    return false;
  }

  if (key_out != nullptr) {
    memcpy(key_out->data, record.root_keys[key_type], sizeof(key_out->data));
  }

  return true;
}

void pbl_bt_prf_store_set_root_keys(struct pbl_bt_sm_key *keys_in) {
  struct prf_record record;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  prv_load(&record);
  record.flags |= PRF_RECORD_FLAG_ROOT_KEYS;
  memset(record.root_keys, 0, sizeof(record.root_keys));
  if (keys_in != nullptr) {
    memcpy(record.root_keys, keys_in, sizeof(record.root_keys));
  }
  prv_save(&record);

  pbl_mutex_unlock(&s_lock);
}

static void prv_fill_sec(struct ble_store_value_sec *sec, const struct prf_record *record) {
  memset(sec, 0, sizeof(*sec));
  sec->peer_addr.type = record->peer_addr_type;
  memcpy(sec->peer_addr.val, record->peer_addr, sizeof(sec->peer_addr.val));
  sec->key_size = PRF_RECORD_KEY_SIZE;
  sec->sc = (record->flags & PRF_RECORD_FLAG_SECURE_CONNECTIONS) != 0U;
  sec->authenticated = (record->flags & PRF_RECORD_FLAG_AUTHENTICATED) != 0U;
}

bool pbl_bt_prf_store_get_pairing(struct ble_store_value_sec *peer, struct ble_store_value_sec *our,
                                  char *name) {
  struct prf_record record;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  prv_load(&record);
  pbl_mutex_unlock(&s_lock);

  if ((record.flags & PRF_RECORD_FLAG_PAIRING) == 0U) {
    return false;
  }

  if (peer != nullptr) {
    prv_fill_sec(peer, &record);
    if ((record.flags & PRF_RECORD_FLAG_PEER_LTK) != 0U) {
      peer->ediv = record.peer_ediv;
      peer->rand_num = record.peer_rand;
      peer->ltk_present = 1;
      memcpy(peer->ltk, record.peer_ltk, sizeof(peer->ltk));
    }
    if ((record.flags & PRF_RECORD_FLAG_PEER_IRK) != 0U) {
      peer->irk_present = 1;
      memcpy(peer->irk, record.peer_irk, sizeof(peer->irk));
    }
  }

  if (our != nullptr) {
    prv_fill_sec(our, &record);
    if ((record.flags & PRF_RECORD_FLAG_OUR_LTK) != 0U) {
      our->ediv = record.our_ediv;
      our->rand_num = record.our_rand;
      our->ltk_present = 1;
      memcpy(our->ltk, record.our_ltk, sizeof(our->ltk));
    }
  }

  if (name != nullptr) {
    memcpy(name, record.name, sizeof(record.name));
  }

  return true;
}

void pbl_bt_prf_store_set_pairing(const struct ble_store_value_sec *peer,
                                  const struct ble_store_value_sec *our, const char *name) {
  const bool has_our_ltk = (our != nullptr) && our->ltk_present;
  struct prf_record record;

  if (peer == nullptr || !(peer->ltk_present || peer->irk_present || has_our_ltk)) {
    PBL_LOG_WRN("Not storing a pairing without keys");
    return;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  prv_load(&record);

  // The name there is another peer's.
  if (record.peer_addr_type != peer->peer_addr.type ||
      memcmp(record.peer_addr, peer->peer_addr.val, sizeof(record.peer_addr)) != 0) {
    memset(record.name, 0, sizeof(record.name));
  }

  prv_clear_pairing(&record);

  record.flags |= PRF_RECORD_FLAG_PAIRING;
  record.peer_addr_type = peer->peer_addr.type;
  memcpy(record.peer_addr, peer->peer_addr.val, sizeof(record.peer_addr));
  if (peer->sc) {
    record.flags |= PRF_RECORD_FLAG_SECURE_CONNECTIONS;
  }
  if (peer->authenticated) {
    record.flags |= PRF_RECORD_FLAG_AUTHENTICATED;
  }

  if (peer->ltk_present) {
    record.flags |= PRF_RECORD_FLAG_PEER_LTK;
    record.peer_ediv = peer->ediv;
    record.peer_rand = peer->rand_num;
    memcpy(record.peer_ltk, peer->ltk, sizeof(record.peer_ltk));
  }

  if (peer->irk_present) {
    record.flags |= PRF_RECORD_FLAG_PEER_IRK;
    memcpy(record.peer_irk, peer->irk, sizeof(record.peer_irk));
  }

  if (has_our_ltk) {
    record.flags |= PRF_RECORD_FLAG_OUR_LTK;
    record.our_ediv = our->ediv;
    record.our_rand = our->rand_num;
    memcpy(record.our_ltk, our->ltk, sizeof(record.our_ltk));
  }

  if (name != nullptr) {
    prv_set_name(&record, name);
  }

  prv_save(&record);

  pbl_mutex_unlock(&s_lock);
}

void pbl_bt_prf_store_set_pairing_name(const char *name) {
  struct prf_record record;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  prv_load(&record);
  if ((record.flags & PRF_RECORD_FLAG_PAIRING) != 0U) {
    prv_set_name(&record, name);
    prv_save(&record);
  }

  pbl_mutex_unlock(&s_lock);
}

void pbl_bt_prf_store_erase_pairing(void) {
  struct prf_record record;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  prv_load(&record);
  prv_clear_pairing(&record);
  memset(record.name, 0, sizeof(record.name));
  prv_save(&record);

  pbl_mutex_unlock(&s_lock);
}
