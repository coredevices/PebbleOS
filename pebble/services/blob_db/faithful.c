/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/bluetooth/bond.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/services/blob_db/faithful.h>
#include <pbl/settings/settings.h>
#include <pbl/util/units.h>

PBL_LOG_MODULE_DECLARE(service_blob_db, CONFIG_SERVICE_BLOB_DB_LOG_LEVEL);

#define FILE_NAME "/blob_db_state"
#define FILE_SIZE PBL_KIB(1)

static const char s_key[] = "faithful";

static PBL_MUTEX_DEFINE(s_mutex);

bool blob_db_is_faithful(void) {
  struct pbl_settings_file file;
  uint8_t value = 0U;

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  if (pbl_settings_open(&file, FILE_NAME, FILE_SIZE) == 0) {
    if (pbl_settings_get(&file, s_key, strlen(s_key), &value, sizeof(value)) != 0) {
      value = 0U;
    }
    pbl_settings_close(&file);
  }
  pbl_mutex_unlock(&s_mutex);

  return value == 1U;
}

void blob_db_set_faithful(bool faithful) {
  const uint8_t value = faithful;
  struct pbl_settings_file file;
  int ret;

  PBL_LOG_INFO("Marking the watch as %s", faithful ? "faithful" : "unfaithful");

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  ret = pbl_settings_open(&file, FILE_NAME, FILE_SIZE);
  if (ret == 0) {
    ret = pbl_settings_set(&file, s_key, strlen(s_key), &value, sizeof(value));
    pbl_settings_close(&file);
  }
  pbl_mutex_unlock(&s_mutex);

  if (ret != 0) {
    PBL_LOG_ERR("Could not store the faithfulness (%d)", ret);
  }
}

// Only one bond is kept, so a new one means a new phone.
static void prv_bond_added(const struct pbl_bt_bond_info *info) {
  blob_db_set_faithful(false);
}

PBL_BT_BOND_LISTENER_DEFINE(blob_db_faithful, .added = prv_bond_added);
