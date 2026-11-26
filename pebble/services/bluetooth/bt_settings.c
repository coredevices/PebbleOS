/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/services/bluetooth/bt_settings.h>
#include <pbl/settings/settings.h>
#include <pbl/util/units.h>

PBL_LOG_MODULE_DECLARE(service_bluetooth, CONFIG_SERVICE_BLUETOOTH_LOG_LEVEL);

#define FILE_NAME "/bt_settings"
#define FILE_SIZE PBL_KIB(1)

#define LEGACY_FILE_NAME "/gap_bonding_db"
#define LEGACY_FILE_SIZE PBL_KIB(4)

static const char s_key_airplane[] = "airplane";
// With its terminator.
static const char s_legacy_key_airplane[] = "AIRPLANE_MODE";

static PBL_MUTEX_DEFINE(s_mutex);

// s_mutex must be held.
static void prv_set(uint8_t value) {
  struct pbl_settings_file file;
  int ret;

  ret = pbl_settings_open(&file, FILE_NAME, FILE_SIZE);
  if (ret == 0) {
    ret = pbl_settings_set(&file, s_key_airplane, strlen(s_key_airplane), &value, sizeof(value));
    pbl_settings_close(&file);
  }

  if (ret != 0) {
    PBL_LOG_ERR("Could not store the airplane mode (%d)", ret);
  }
}

// The setting was kept with the bondings, which the Bluetooth stack stores now. s_mutex must be
// held.
static void prv_migrate(void) {
  struct pbl_settings_file legacy;
  struct pbl_fs_dirent entry;
  uint8_t value;

  if (pbl_fs_stat(LEGACY_FILE_NAME, &entry) != 0) {
    return;
  }

  if (pbl_settings_open(&legacy, LEGACY_FILE_NAME, LEGACY_FILE_SIZE) == 0) {
    if (pbl_settings_get(&legacy, s_legacy_key_airplane, sizeof(s_legacy_key_airplane), &value,
                         sizeof(value)) == 0) {
      prv_set(value);
    }

    pbl_settings_close(&legacy);
  }

  (void)pbl_fs_unlink(LEGACY_FILE_NAME);
}

bool bt_settings_get_airplane_mode(void) {
  struct pbl_settings_file file;
  uint8_t value = 0U;

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);

  prv_migrate();

  if (pbl_settings_open(&file, FILE_NAME, FILE_SIZE) == 0) {
    if (pbl_settings_get(&file, s_key_airplane, strlen(s_key_airplane), &value, sizeof(value)) !=
        0) {
      value = 0U;
    }
    pbl_settings_close(&file);
  }

  pbl_mutex_unlock(&s_mutex);

  return value == 1U;
}

void bt_settings_set_airplane_mode(bool enable) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  prv_set(enable);
  pbl_mutex_unlock(&s_mutex);
}
