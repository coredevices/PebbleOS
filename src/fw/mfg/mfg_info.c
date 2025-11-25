/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg/mfg_serials.h"

//! @file mfg_info.c
//!
//! This file implements mfg_info functions where the storage is the same for both tintin and
//! snowy, mostly for things that are stored in OTP. See the tintin/mfg_info.c and
//! snowy/mfg_info.c for the board specific implementations.

void mfg_info_get_serialnumber(char *serial_number, size_t serial_number_size) {
  strncpy(serial_number, mfg_get_serial_number(), serial_number_size);
  if (serial_number_size > MFG_SERIAL_NUMBER_SIZE) {
    // FIXME: manually adding a null-terminator if there is space
    // strncpy should pad the end of strings with nulls if there is space, but
    // everywhere that seems to use OTP or registry strings seems to pad the end
    // with a null-term.
    // Note: making an assumption here that the serial number is always going to the MFG_SERIAL_NUMBER_SIZE characters
    serial_number[MFG_SERIAL_NUMBER_SIZE] = '\0';
  }
}

void mfg_info_get_pcba_serialnumber(char *pcba_serial_number, size_t pcba_serial_number_size) {
  strncpy(pcba_serial_number, mfg_get_pcba_serial_number(), pcba_serial_number_size);
  if (pcba_serial_number_size > MFG_PCBA_SERIAL_NUMBER_SIZE) {
    // same assumption as in mfg_info_get_pcba_serialnumber
    pcba_serial_number[MFG_PCBA_SERIAL_NUMBER_SIZE] = '\0';
  }
}

void mfg_info_get_hw_version(char *hw_version, size_t hw_version_size) {
  strncpy(hw_version, mfg_get_hw_version(), hw_version_size);
  if (hw_version_size > MFG_HW_VERSION_SIZE) {
    // same assumption as in mfg_info_get_pcba_serialnumber
    hw_version[MFG_HW_VERSION_SIZE] = '\0';
  }
}

