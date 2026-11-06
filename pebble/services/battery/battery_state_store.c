/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "battery_state_store.h"

#include <errno.h>
#include <stdint.h>

#include <pbl/logging/logging.h>

#ifdef CONFIG_MFG
#include <pbl/board/partitions.h>
#include <pbl/drivers/flash.h>
#else
#include <pbl/settings/settings.h>
#endif

PBL_LOG_MODULE_DECLARE(service_battery, CONFIG_SERVICE_BATTERY_LOG_LEVEL);

#ifdef CONFIG_MFG
// Manufacturing firmware keeps it in the MFG_BATTERY_STATE flash region
int battery_state_store_erase(void) {
  int ret;

  ret = pbl_flash_partition_erase(PART_MFG_BATTERY_STATE, 0U, PART_MFG_BATTERY_STATE_SIZE);
  if (ret < 0) {
    return ret;
  }

  PBL_LOG_DBG("Fuel gauge state erased");

  return 0;
}

int battery_state_store_load(void *state, size_t size) {
  const uint8_t *bytes = state;
  int ret;

  if (size > PART_MFG_BATTERY_STATE_SIZE) {
    return -EINVAL;
  }

  ret = pbl_flash_partition_read(PART_MFG_BATTERY_STATE, 0U, state, size);
  if (ret < 0) {
    return ret;
  }

  for (size_t i = 0; i < size; i++) {
    if (bytes[i] != 0xFFU) {
      PBL_LOG_DBG("Fuel gauge state loaded");
      return 0;
    }
  }

  return -ENOENT;
}

int battery_state_store_save(const void *state, size_t size) {
  int ret;

  if (size > PART_MFG_BATTERY_STATE_SIZE) {
    return -EINVAL;
  }

  ret = pbl_flash_partition_erase(PART_MFG_BATTERY_STATE, 0U, PART_MFG_BATTERY_STATE_SIZE);
  if (ret < 0) {
    return ret;
  }

  ret = pbl_flash_partition_write(PART_MFG_BATTERY_STATE, 0U, state, size);
  if (ret < 0) {
    return ret;
  }

  PBL_LOG_DBG("Fuel gauge state saved");

  return 0;
}
#else
// Normal firmware keeps it in a settings file
#define FUEL_GAUGE_SETTINGS_FILE_NAME "/fgs"
#define FUEL_GAUGE_SETTINGS_MAX_SIZE  2048

static const uint32_t FUEL_GAUGE_STATE_KEY = 1;

int battery_state_store_erase(void) {
  struct pbl_settings_file file;
  int ret;

  ret = pbl_settings_open(&file, FUEL_GAUGE_SETTINGS_FILE_NAME, FUEL_GAUGE_SETTINGS_MAX_SIZE);
  if (ret < 0) {
    return ret;
  }

  ret = pbl_settings_delete(&file, &FUEL_GAUGE_STATE_KEY, sizeof(FUEL_GAUGE_STATE_KEY));
  pbl_settings_close(&file);
  if (ret < 0) {
    return ret;
  }

  PBL_LOG_DBG("Fuel gauge state erased");

  return 0;
}

int battery_state_store_load(void *state, size_t size) {
  struct pbl_settings_file file;
  int ret;

  ret = pbl_settings_open(&file, FUEL_GAUGE_SETTINGS_FILE_NAME, FUEL_GAUGE_SETTINGS_MAX_SIZE);
  if (ret < 0) {
    return ret;
  }

  if (pbl_settings_get_len(&file, &FUEL_GAUGE_STATE_KEY, sizeof(FUEL_GAUGE_STATE_KEY)) != size) {
    pbl_settings_close(&file);
    return -ENOENT;
  }

  ret = pbl_settings_get(&file, &FUEL_GAUGE_STATE_KEY, sizeof(FUEL_GAUGE_STATE_KEY), state, size);
  pbl_settings_close(&file);
  if (ret < 0) {
    return ret;
  }

  PBL_LOG_DBG("Fuel gauge state loaded");

  return 0;
}

int battery_state_store_save(const void *state, size_t size) {
  struct pbl_settings_file file;
  int ret;

  ret = pbl_settings_open(&file, FUEL_GAUGE_SETTINGS_FILE_NAME, FUEL_GAUGE_SETTINGS_MAX_SIZE);
  if (ret < 0) {
    return ret;
  }

  ret = pbl_settings_set(&file, &FUEL_GAUGE_STATE_KEY, sizeof(FUEL_GAUGE_STATE_KEY), state, size);
  pbl_settings_close(&file);
  if (ret < 0) {
    return ret;
  }

  PBL_LOG_DBG("Fuel gauge state saved");

  return 0;
}
#endif // CONFIG_MFG
