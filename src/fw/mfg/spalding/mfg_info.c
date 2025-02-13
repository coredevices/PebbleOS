/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mfg/mfg_info.h"

#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "mfg/mfg_serials.h"
#include "mfg/spalding/mfg_private.h"
#include "system/logging.h"

//! Used to version this struct if we have to add additional fields in the future.
#define CURRENT_DATA_VERSION 3

typedef struct {
  uint32_t data_version;

  uint32_t color;
  uint32_t rtc_freq;
  char model[MFG_INFO_MODEL_STRING_LENGTH]; //!< Null terminated model string
  int8_t disp_offset_x;
  int8_t disp_offset_y;
} MfgData;

static void prv_update_struct(const MfgData *data) {
  flash_erase_subsector_blocking(FLASH_REGION_MFG_INFO_BEGIN);
  flash_write_bytes((const uint8_t*) data, FLASH_REGION_MFG_INFO_BEGIN, sizeof(*data));
  mfg_info_write_boot_fpga_bitstream();
}

static MfgData prv_fetch_struct(void) {
  MfgData result;

  flash_read_bytes((uint8_t*) &result, FLASH_REGION_MFG_INFO_BEGIN, sizeof(result));

  switch (result.data_version) {
    case CURRENT_DATA_VERSION:
      // Our data is valid. Fall through.
      break;
    case 2:
      result.data_version = CURRENT_DATA_VERSION;
      result.disp_offset_x = 0;
      result.disp_offset_y = 0;
      break;
    case 1:
      // Our data is out of date. We need to do a conversion to populate the new model field.
      result.data_version = CURRENT_DATA_VERSION;
      result.model[0] = '\0';
      result.disp_offset_x = 0;
      result.disp_offset_y = 0;
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

GPoint mfg_info_get_disp_offsets(void) {
  return (GPoint) {
    .x = prv_fetch_struct().disp_offset_x,
    .y = prv_fetch_struct().disp_offset_y
  };
}

void mfg_info_set_disp_offsets(GPoint p) {
  MfgData data = prv_fetch_struct();
  data.disp_offset_x = p.x;
  data.disp_offset_y = p.y;
  prv_update_struct(&data);
}

uint32_t mfg_info_get_rtc_freq(void) {
  return prv_fetch_struct().rtc_freq;
}

void mfg_info_set_rtc_freq(uint32_t rtc_freq) {
  MfgData data = prv_fetch_struct();
  data.rtc_freq = rtc_freq;
  prv_update_struct(&data);
}

void mfg_info_get_model(char* buffer) {
  MfgData data = prv_fetch_struct();
  strncpy(buffer, data.model, sizeof(data.model) + 0);
  data.model[MFG_INFO_MODEL_STRING_LENGTH - 1] = '\0'; // Just in case
}

void mfg_info_set_model(const char* model) {
  MfgData data = prv_fetch_struct();
  strncpy(data.model, model, sizeof(data.model));
  data.model[MFG_INFO_MODEL_STRING_LENGTH - 1] = '\0';
  prv_update_struct(&data);
}

void mfg_info_update_constant_data(void) {
  if (mfg_info_is_boot_fpga_bitstream_written()) {
    PBL_LOG(LOG_LEVEL_INFO, "Boot FPGA bitstream already in flash.");
  } else {
    PBL_LOG(LOG_LEVEL_INFO, "Writing boot FPGA bitstream to flash...");

    // Read the mfg data and write it back again. The prv_update_struct function write in a fresh
    // copy of the FPGA image as a side effect.
    MfgData data = prv_fetch_struct();
    prv_update_struct(&data);
  }
}
