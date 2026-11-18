/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <pbl/logging/logging.h>
#include <pbl/settings/settings.h>

#include <host/ble_hs.h>
#include <host/ble_store.h>
#include <syscfg/syscfg.h>

// NimBLE's private header, which needs the ones above.
// clang-format off
#include "ble_store_config_priv.h"
// clang-format on

PBL_LOG_MODULE_DECLARE(nimble, CONFIG_NIMBLE_LOG_LEVEL);

#define FILE_NAME "/nimble_store"
#define IRK_SIZE  16U

/* The arrays are saved as they are in memory: a new layout drops them. */
struct layout {
  uint16_t sec_size;
  uint16_t cccd_size;
};

static const char s_key_layout[] = "layout";
static const char s_key_our_secs[] = "our_secs";
static const char s_key_peer_secs[] = "peer_secs";
static const char s_key_cccds[] = "cccds";
static const char s_key_irk[] = "irk";

static const struct layout s_layout = {
  .sec_size = sizeof(struct ble_store_value_sec),
  .cccd_size = sizeof(struct ble_store_value_cccd),
};

static int prv_open(struct pbl_settings_file *file) {
  int ret;

  ret = pbl_settings_open(file, FILE_NAME, CONFIG_NIMBLE_STORE_SETTINGS_FILE_SIZE);
  if (ret != 0) {
    PBL_LOG_ERR("Could not open the store (%d)", ret);
  }

  return ret;
}

static int prv_save(const char *key, const void *values, size_t size) {
  struct pbl_settings_file file;
  int ret;

  ret = prv_open(&file);
  if (ret != 0) {
    return BLE_HS_ESTORE_FAIL;
  }

  if (size == 0U) {
    ret = pbl_settings_delete(&file, key, strlen(key));
    if (ret == -ENOENT) {
      ret = 0;
    }
  } else {
    ret = pbl_settings_set(&file, key, strlen(key), values, size);
  }

  pbl_settings_close(&file);

  if (ret != 0) {
    PBL_LOG_ERR("Could not save %s (%d)", key, ret);
    return BLE_HS_ESTORE_FAIL;
  }

  return 0;
}

/* Returns how many values were loaded. */
static int prv_load(struct pbl_settings_file *file, const char *key, void *values, size_t size,
                    int max) {
  size_t len = pbl_settings_get_len(file, key, strlen(key));

  if (len == 0U) {
    return 0;
  }

  if (len % size != 0U || len / size > (size_t)max) {
    PBL_LOG_WRN("Dropping %s: %u bytes do not fit", key, (unsigned int)len);
    return 0;
  }

  if (pbl_settings_get(file, key, strlen(key), values, len) != 0) {
    return 0;
  }

  return (int)(len / size);
}

int ble_store_config_persist_our_secs(void) {
  return prv_save(s_key_our_secs, ble_store_config_our_secs,
                  ble_store_config_num_our_secs * sizeof(ble_store_config_our_secs[0]));
}

int ble_store_config_persist_peer_secs(void) {
  return prv_save(s_key_peer_secs, ble_store_config_peer_secs,
                  ble_store_config_num_peer_secs * sizeof(ble_store_config_peer_secs[0]));
}

int ble_store_config_persist_cccds(void) {
  return prv_save(s_key_cccds, ble_store_config_cccds,
                  ble_store_config_num_cccds * sizeof(ble_store_config_cccds[0]));
}

/* The local IRK, generated once. LTKs and CSRKs are left to NimBLE. */
static int prv_gen_key(uint8_t key, struct ble_store_gen_key *gen_key, uint16_t conn_handle) {
  struct pbl_settings_file file;
  int ret;

  if (key != BLE_STORE_GEN_KEY_IRK) {
    return BLE_HS_ENOTSUP;
  }

  ret = prv_open(&file);
  if (ret != 0) {
    return BLE_HS_ESTORE_FAIL;
  }

  if (pbl_settings_get_len(&file, s_key_irk, strlen(s_key_irk)) == IRK_SIZE &&
      pbl_settings_get(&file, s_key_irk, strlen(s_key_irk), gen_key->irk, IRK_SIZE) == 0) {
    pbl_settings_close(&file);
    return 0;
  }

  ret = ble_hs_hci_rand(gen_key->irk, IRK_SIZE);
  if (ret == 0) {
    ret = pbl_settings_set(&file, s_key_irk, strlen(s_key_irk), gen_key->irk, IRK_SIZE);
    if (ret != 0) {
      PBL_LOG_ERR("Could not save the IRK (%d)", ret);
      ret = BLE_HS_ESTORE_FAIL;
    }
  }

  pbl_settings_close(&file);

  return ret;
}

void ble_store_config_conf_init(void) {
  struct pbl_settings_file file;
  struct layout layout;

  ble_hs_cfg.store_gen_key_cb = prv_gen_key;

  if (prv_open(&file) != 0) {
    return;
  }

  if (pbl_settings_get(&file, s_key_layout, strlen(s_key_layout), &layout, sizeof(layout)) != 0 ||
      memcmp(&layout, &s_layout, sizeof(layout)) != 0) {
    if (pbl_settings_exists(&file, s_key_layout, strlen(s_key_layout))) {
      PBL_LOG_WRN("The store has another layout; dropping its bonds");
    }
    (void)pbl_settings_delete(&file, s_key_our_secs, strlen(s_key_our_secs));
    (void)pbl_settings_delete(&file, s_key_peer_secs, strlen(s_key_peer_secs));
    (void)pbl_settings_delete(&file, s_key_cccds, strlen(s_key_cccds));
    (void)pbl_settings_set(&file, s_key_layout, strlen(s_key_layout), &s_layout, sizeof(s_layout));
    pbl_settings_close(&file);
    return;
  }

  ble_store_config_num_our_secs =
      prv_load(&file, s_key_our_secs, ble_store_config_our_secs,
               sizeof(ble_store_config_our_secs[0]), MYNEWT_VAL(BLE_STORE_MAX_BONDS));
  ble_store_config_num_peer_secs =
      prv_load(&file, s_key_peer_secs, ble_store_config_peer_secs,
               sizeof(ble_store_config_peer_secs[0]), MYNEWT_VAL(BLE_STORE_MAX_BONDS));
  ble_store_config_num_cccds =
      prv_load(&file, s_key_cccds, ble_store_config_cccds, sizeof(ble_store_config_cccds[0]),
               MYNEWT_VAL(BLE_STORE_MAX_CCCDS));

  pbl_settings_close(&file);
}
