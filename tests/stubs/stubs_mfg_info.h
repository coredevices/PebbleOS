/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "mfg/mfg_serials.h"
#include "applib/app_watch_info.h"
#include "applib/graphics/gtypes.h"

static const char *expected_serial_number = "2DQ0135B3424";

const char* mfg_get_serial_number(void) {
  return expected_serial_number;
}

void mfg_info_get_serialnumber(char *serial_number, size_t serial_number_size) {
  strncpy(serial_number, expected_serial_number, serial_number_size);
  if (serial_number_size > MFG_SERIAL_NUMBER_SIZE) {
    serial_number[MFG_SERIAL_NUMBER_SIZE] = '\0';
  }
}

static const char *expected_hw_version = "V2R2";
const char* mfg_get_hw_version(void) {
  return expected_hw_version;
}

WatchInfoColor mfg_info_get_watch_color(void) {
  return WATCH_INFO_COLOR_PINK;
}

GPoint mfg_info_get_disp_offsets(void) {
  return GPointZero;
}
