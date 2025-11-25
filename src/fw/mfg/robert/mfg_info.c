/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg/mfg_info.h"

#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "mfg/mfg_serials.h"
#include "system/logging.h"

//! Used to version this struct if we have to add additional fields in the future.
#define CURRENT_DATA_VERSION 2

typedef struct {
  uint32_t data_version;

  uint32_t color;
  uint32_t rtc_freq;
  char model[MFG_INFO_MODEL_STRING_LENGTH]; //!< Null terminated model string
} MfgData;

static void prv_update_struct(const MfgData *data) {
  flash_erase_subsector_blocking(FLASH_REGION_MFG_INFO_BEGIN);
  flash_write_bytes((const uint8_t*) data, FLASH_REGION_MFG_INFO_BEGIN, sizeof(*data));
}

static MfgData prv_fetch_struct(void) {
  MfgData result;

  flash_read_bytes((uint8_t*) &result, FLASH_REGION_MFG_INFO_BEGIN, sizeof(result));

  switch (result.data_version) {
    case CURRENT_DATA_VERSION:
      // Our data is valid. Fall through.
      break;
    case 1:
      // Our data is out of date. We need to do a conversion to populate the new model field.
      result.data_version = CURRENT_DATA_VERSION;
      result.model[0] = '\0';
      break;
    default:
      // No data present, just return an initialized struct with default values.
      return (MfgData) { .data_version = CURRENT_DATA_VERSION };
  }

  return result;
}

WatchInfoColor mfg_info_get_watch_color(void) {
  return prv_fetch_struct().color;
}

void mfg_info_set_watch_color(WatchInfoColor color) {
  MfgData data = prv_fetch_struct();
  data.color = color;
  prv_update_struct(&data);
}

uint32_t mfg_info_get_rtc_freq(void) {
  return prv_fetch_struct().rtc_freq;
}

#if MANUFACTURING_FW
void mfg_info_set_rtc_freq(uint32_t rtc_freq) {
  MfgData data = prv_fetch_struct();
  data.rtc_freq = rtc_freq;
  prv_update_struct(&data);
}
#endif

void mfg_info_get_model(char* buffer) {
  MfgData data = prv_fetch_struct();
  strncpy(buffer, data.model, sizeof(data.model));
  data.model[MFG_INFO_MODEL_STRING_LENGTH - 1] = '\0'; // Just in case
}

void mfg_info_set_model(const char* model) {
  MfgData data = prv_fetch_struct();
  strncpy(data.model, model, sizeof(data.model));
  data.model[MFG_INFO_MODEL_STRING_LENGTH - 1] = '\0';
  prv_update_struct(&data);
}

GPoint mfg_info_get_disp_offsets(void) {
  // Not implemented. Can just assume no offset
  return (GPoint) {};
}

void mfg_info_set_disp_offsets(GPoint p) {
  // Not implemented.
}

void mfg_info_update_constant_data(void) {
  // No constant data required for Robert.
}

bool mfg_info_is_hrm_present(void) {
  return true;
}
