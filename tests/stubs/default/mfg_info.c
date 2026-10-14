/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/compiler.h>

#include <applib/app_watch_info.h>
#include <applib/graphics/gtypes.h>
#include <mfg/mfg_serials.h>

static const char *expected_serial_number = "2DQ0135B3424";

PBL_WEAK const char *mfg_get_serial_number(void) {
  return expected_serial_number;
}

PBL_WEAK void mfg_info_get_serialnumber(char *serial_number, size_t serial_number_size) {
  strncpy(serial_number, expected_serial_number, serial_number_size);
  if (serial_number_size > MFG_SERIAL_NUMBER_SIZE) {
    serial_number[MFG_SERIAL_NUMBER_SIZE] = '\0';
  }
}

static const char *expected_hw_version = "V2R2";

PBL_WEAK const char *mfg_get_hw_version(void) {
  return expected_hw_version;
}

PBL_WEAK WatchInfoColor mfg_info_get_watch_color(void) {
  return WATCH_INFO_COLOR_PINK;
}
