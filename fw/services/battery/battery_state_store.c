/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "battery_state_store.h"

#include <pbl/logging/logging.h>

#ifdef CONFIG_MFG
#include <pbl/drivers/flash.h>

#include <flash_region/flash_region.h>
#else
#include <pbl/settings/settings.h>

#include <system/status_errno.h>
#endif

PBL_LOG_MODULE_DECLARE(service_battery, CONFIG_SERVICE_BATTERY_LOG_LEVEL);

#ifdef CONFIG_MFG
// In manufacturing firmware, use dedicated MFG_BATTERY_STATE flash region
void battery_state_store_erase(void) {
  pbl_flash_erase(FLASH, FLASH_REGION_MFG_BATTERY_STATE_BEGIN, SUBSECTOR_SIZE_BYTES);
  PBL_LOG_DBG("Fuel gauge state erased");
}

bool battery_state_store_load(void *state, size_t size) {
  if (size > (FLASH_REGION_MFG_BATTERY_STATE_END - FLASH_REGION_MFG_BATTERY_STATE_BEGIN)) {
    return false;
  }

  pbl_flash_read(FLASH, FLASH_REGION_MFG_BATTERY_STATE_BEGIN, state, size);

  // Check if the flash region contains valid data (not all 0xFF)
  uint8_t *bytes = (uint8_t *)state;
  bool all_erased = true;
  for (size_t i = 0; i < size; i++) {
    if (bytes[i] != 0xFF) {
      all_erased = false;
      break;
    }
  }

  if (all_erased) {
    return false;
  }

  PBL_LOG_DBG("Fuel gauge state loaded");

  return true;
}

void battery_state_store_save(const void *state, size_t size) {
  if (size > (FLASH_REGION_MFG_BATTERY_STATE_END - FLASH_REGION_MFG_BATTERY_STATE_BEGIN)) {
    PBL_LOG_ERR("Fuel gauge state too large for MFG_BATTERY_STATE region");
    return;
  }

  pbl_flash_erase(FLASH, FLASH_REGION_MFG_BATTERY_STATE_BEGIN, SUBSECTOR_SIZE_BYTES);
  pbl_flash_write(FLASH, FLASH_REGION_MFG_BATTERY_STATE_BEGIN, state, size);

  PBL_LOG_DBG("Fuel gauge state saved");
}
#else
// In normal firmware, use settings file
#define FUEL_GAUGE_SETTINGS_FILE_NAME "/fgs"
#define FUEL_GAUGE_SETTINGS_MAX_SIZE  2048

static const uint32_t FUEL_GAUGE_STATE_KEY = 1;

void battery_state_store_erase(void) {
  struct pbl_settings_file file;
  status_t ret;

  ret = status_from_errno(
      pbl_settings_open(&file, FUEL_GAUGE_SETTINGS_FILE_NAME, FUEL_GAUGE_SETTINGS_MAX_SIZE));
  if (ret != S_SUCCESS) {
    return;
  }

  pbl_settings_delete(&file, &FUEL_GAUGE_STATE_KEY, sizeof(FUEL_GAUGE_STATE_KEY));
  pbl_settings_close(&file);

  PBL_LOG_DBG("Fuel gauge state erased");
}

bool battery_state_store_load(void *state, size_t size) {
  struct pbl_settings_file file;
  status_t ret;

  ret = status_from_errno(
      pbl_settings_open(&file, FUEL_GAUGE_SETTINGS_FILE_NAME, FUEL_GAUGE_SETTINGS_MAX_SIZE));
  if (ret != S_SUCCESS) {
    return false;
  }

  int len = pbl_settings_get_len(&file, &FUEL_GAUGE_STATE_KEY, sizeof(FUEL_GAUGE_STATE_KEY));
  if (len != (int)size) {
    pbl_settings_close(&file);
    return false;
  }

  ret = status_from_errno(
      pbl_settings_get(&file, &FUEL_GAUGE_STATE_KEY, sizeof(FUEL_GAUGE_STATE_KEY), state, size));
  pbl_settings_close(&file);

  if (ret != S_SUCCESS) {
    return false;
  }

  PBL_LOG_DBG("Fuel gauge state loaded");

  return true;
}

void battery_state_store_save(const void *state, size_t size) {
  struct pbl_settings_file file;
  status_t ret;

  ret = status_from_errno(
      pbl_settings_open(&file, FUEL_GAUGE_SETTINGS_FILE_NAME, FUEL_GAUGE_SETTINGS_MAX_SIZE));
  if (ret != S_SUCCESS) {
    PBL_LOG_ERR("Failed to open fuel gauge settings file");
    return;
  }

  ret = status_from_errno(
      pbl_settings_set(&file, &FUEL_GAUGE_STATE_KEY, sizeof(FUEL_GAUGE_STATE_KEY), state, size));
  pbl_settings_close(&file);

  if (ret != S_SUCCESS) {
    PBL_LOG_ERR("Failed to save fuel gauge state");
  } else {
    PBL_LOG_DBG("Fuel gauge state saved");
  }
}
#endif // CONFIG_MFG
