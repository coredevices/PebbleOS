/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "canopen_internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <pbl/kernel/malloc.h>
#include <pbl/logging/logging.h>
#include <pbl/settings/settings.h>
#include <pbl/util/byteorder.h>

PBL_LOG_MODULE_DECLARE(canopen, CONFIG_CANOPEN_LOG_LEVEL);

/* "save" and "load", as little-endian words */
#define SIGNATURE_SAVE 0x65766173U
#define SIGNATURE_LOAD 0x64616f6cU

#define SUB_ALL  1U
#define SUB_COMM 2U
#define SUB_APP  3U

#define RECORD_HDR 4U

static const char *const s_keys[] = {"comm", "app"};

/* Parameters: what the bus can write, but the error history and the storage commands. */
static bool prv_stored(const struct pbl_canopen_od_entry *e, bool app) {
  if ((e->attr & PBL_CANOPEN_ATTR_WRITE) == 0U || e->size > UINT8_MAX ||
      (e->index >= 0x2000U) != app) {
    return false;
  }

  return e->index != PBL_CANOPEN__IDX_ERRORS && e->index != PBL_CANOPEN__IDX_STORE &&
         e->index != PBL_CANOPEN__IDX_RESTORE;
}

static int prv_open(struct pbl_canopen *co, struct pbl_settings_file *file) {
  char name[16];

  snprintf(name, sizeof(name), "/canopen%u", co->cfg.node_id);
  return pbl_settings_open(file, name, CONFIG_CANOPEN_STORAGE_SIZE);
}

static int prv_save(struct pbl_canopen *co, struct pbl_settings_file *file, bool app) {
  const struct pbl_canopen_od *od = co->cfg.od;
  const char *key = s_keys[app];
  size_t len = 0U;
  size_t off = 0U;
  uint8_t *blob;
  int ret;

  for (size_t i = 0U; i < od->num_entries; i++) {
    if (prv_stored(&od->entries[i], app)) {
      len += RECORD_HDR + od->entries[i].size;
    }
  }
  if (len == 0U) {
    return 0;
  }
  if (len > PBL_SETTINGS_VAL_MAX_LEN) {
    return -ENOSPC;
  }

  blob = pbl_malloc(len);
  if (blob == nullptr) {
    return -ENOMEM;
  }

  for (size_t i = 0U; i < od->num_entries; i++) {
    const struct pbl_canopen_od_entry *e = &od->entries[i];

    if (!prv_stored(e, app)) {
      continue;
    }
    pbl_put_le16(e->index, &blob[off]);
    blob[off + 2U] = e->subindex;
    blob[off + 3U] = (uint8_t)e->size;
    pbl_canopen__od_to_wire(e, &blob[off + RECORD_HDR], e->size);
    off += RECORD_HDR + e->size;
  }

  ret = pbl_settings_set(file, key, strlen(key), blob, len);
  pbl_free(blob);

  return ret;
}

static void prv_load(struct pbl_canopen *co, struct pbl_settings_file *file, bool app) {
  const char *key = s_keys[app];
  size_t len = pbl_settings_get_len(file, key, strlen(key));
  uint8_t *blob;

  if (len == 0U) {
    return;
  }

  blob = pbl_malloc(len);
  if (blob == nullptr) {
    PBL_LOG_WRN("no memory to load the %s parameters", key);
    return;
  }

  if (pbl_settings_get(file, key, strlen(key), blob, len) == 0) {
    for (size_t off = 0U; off + RECORD_HDR <= len;) {
      uint16_t index = pbl_get_le16(&blob[off]);
      uint8_t size = blob[off + 3U];
      const struct pbl_canopen_od_entry *e =
          pbl_canopen__od_find(co->cfg.od, index, blob[off + 2U]);

      if (off + RECORD_HDR + size > len) {
        break;
      }
      /* Entries a new dictionary dropped or resized keep their defaults. */
      if (e != nullptr && prv_stored(e, app) && e->size == size) {
        pbl_canopen__od_from_wire(e, e->data, &blob[off + RECORD_HDR], size);
      }
      off += RECORD_HDR + size;
    }
  }

  pbl_free(blob);
}

uint32_t pbl_canopen__storage_write(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e,
                                    uint32_t val) {
  bool save = e->index == PBL_CANOPEN__IDX_STORE;
  struct pbl_settings_file file;
  int ret = 0;

  if (val != (save ? SIGNATURE_SAVE : SIGNATURE_LOAD)) {
    return PBL_CANOPEN__ABORT_DATA;
  }

  if (prv_open(co, &file) != 0) {
    return PBL_CANOPEN__ABORT_DATA;
  }

  for (size_t app = 0U; app < 2U && ret == 0; app++) {
    if (e->subindex != SUB_ALL && e->subindex != (app ? SUB_APP : SUB_COMM)) {
      continue;
    }
    if (save) {
      ret = prv_save(co, &file, app != 0U);
    } else {
      ret = pbl_settings_delete(&file, s_keys[app], strlen(s_keys[app]));
      ret = ret == -ENOENT ? 0 : ret;
    }
  }

  pbl_settings_close(&file);

  if (ret != 0) {
    PBL_LOG_WRN("parameters not %s (%d)", save ? "saved" : "restored", ret);
    return PBL_CANOPEN__ABORT_DATA;
  }

  return 0U;
}

void pbl_canopen__storage_load(struct pbl_canopen *co, bool app) {
  struct pbl_settings_file file;

  if (pbl_canopen__od_find(co->cfg.od, PBL_CANOPEN__IDX_STORE, 1U) == nullptr ||
      prv_open(co, &file) != 0) {
    return;
  }

  prv_load(co, &file, false);
  if (app) {
    prv_load(co, &file, true);
  }

  pbl_settings_close(&file);
}
