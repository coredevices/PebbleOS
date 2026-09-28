/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include "mfg_serials.h"

#include "pbl/util/size.h"

static const uint8_t OTP_SERIAL_SLOT_INDICES[] = {
  OTP_SERIAL,
};
static const uint8_t OTP_PCBA_SLOT_INDICES[] = {OTP_PCBA_SERIAL};
static const uint8_t OTP_HWVER_SLOT_INDICES[] = {OTP_HWVER};

static const char DUMMY_SERIAL[MFG_SERIAL_NUMBER_SIZE + 1] = "XXXXXXXXXXXX";
// FIXME: shouldn't the dummy HWVER be 9 X's?
static const char DUMMY_HWVER[MFG_HW_VERSION_SIZE + 1] = "XXXXXXXX";
static const char DUMMY_PCBA_SERIAL[MFG_PCBA_SERIAL_NUMBER_SIZE + 1] = "XXXXXXXXXXXX";

const char *mfg_get_serial_number(void) {
  // Trying from "most recent" slot to "least recent":
  for (int i = ARRAY_LENGTH(OTP_SERIAL_SLOT_INDICES) - 1; i >= 0; --i) {
    const uint8_t index = OTP_SERIAL_SLOT_INDICES[i];
    if (otp_is_locked(index)) {
      return otp_get_slot(index);
    }
  }
  return DUMMY_SERIAL;
}

const char *mfg_get_hw_version(void) {
  // Trying from "most recent" slot to "least recent":
  for (int i = ARRAY_LENGTH(OTP_HWVER_SLOT_INDICES) - 1; i >= 0; --i) {
    const uint8_t index = OTP_HWVER_SLOT_INDICES[i];
    if (otp_is_locked(index)) {
      return otp_get_slot(index);
    }
  }
  return DUMMY_HWVER;
}

const char *mfg_get_pcba_serial_number(void) {
  // Trying from "most recent" slot to "least recent":
  for (int i = ARRAY_LENGTH(OTP_PCBA_SLOT_INDICES) - 1; i >= 0; --i) {
    const uint8_t index = OTP_PCBA_SLOT_INDICES[i];
    if (otp_is_locked(index)) {
      return otp_get_slot(index);
    }
  }
  return DUMMY_PCBA_SERIAL;
}

static MfgSerialsResult prv_mfg_write_data_to_slot(const uint8_t *slot_indices, size_t num_slots,
                                                   const char *data, size_t data_size,
                                                   uint8_t *out_index) {
  for (unsigned int i = 0; i < num_slots; ++i) {
    const uint8_t index = slot_indices[i];
    const OtpWriteResult result = otp_write_slot(index, data);
    if (result == OtpWriteSuccess) {
      if (out_index) {
        *out_index = index;
      }
      return MfgSerialsResultSuccess;
    }
    // if OtpWriteFailCorrupt or OtpWriteFailAlreadyWritten, continue to next slot.
  }
  return MfgSerialsResultFailNoMoreSpace;
}

MfgSerialsResult mfg_write_serial_number(const char *serial, size_t serial_size,
                                         uint8_t *out_index) {
  if ((serial_size != (MFG_SERIAL_NUMBER_SIZE)) || (serial[serial_size] != '\0')) {
    return MfgSerialsResultFailIncorrectLength;
  }

  return prv_mfg_write_data_to_slot(OTP_SERIAL_SLOT_INDICES, ARRAY_LENGTH(OTP_SERIAL_SLOT_INDICES),
                                    serial, serial_size, out_index);
}

MfgSerialsResult mfg_write_pcba_serial_number(const char *serial, size_t serial_size,
                                              uint8_t *out_index) {
  if ((serial_size > MFG_PCBA_SERIAL_NUMBER_SIZE) || (serial[serial_size] != '\0')) {
    return MfgSerialsResultFailIncorrectLength;
  }

  return prv_mfg_write_data_to_slot(OTP_PCBA_SLOT_INDICES, ARRAY_LENGTH(OTP_PCBA_SLOT_INDICES),
                                    serial, serial_size, out_index);
}

MfgSerialsResult mfg_write_hw_version(const char *hwver, size_t hwver_size, uint8_t *out_index) {
  if ((hwver_size > MFG_HW_VERSION_SIZE) || hwver[hwver_size] != '\0') {
    return MfgSerialsResultFailIncorrectLength;
  }
  return prv_mfg_write_data_to_slot(OTP_HWVER_SLOT_INDICES, ARRAY_LENGTH(OTP_HWVER_SLOT_INDICES),
                                    hwver, hwver_size, out_index);
}

#if defined(CONFIG_IS_BIGBOARD)

#include <stdio.h>
#include <pbl/drivers/rtc.h>
#include <pbl/logging/logging.h>

#ifndef CONFIG_SOC_NRF52
static void prv_get_not_so_unique_serial(char *serial_number) {
  // Contains 96 bits (12 bytes) that uniquely identify the STM32F2/F4 MCUs:
  const uint8_t *DEVICE_ID_REGISTER = (const uint8_t *)0x1FFF7A10;
  // BBs used the first bytes of the ID registers, which happened to be not very unique...
  for (int i = 2, r = 7; i < MFG_SERIAL_NUMBER_SIZE; i += 2, ++r) {
    sniprintf(&serial_number[i], 3 /* 2 hex digits + zero terminator */, "%02X",
              DEVICE_ID_REGISTER[r]);
  }
  serial_number[MFG_SERIAL_NUMBER_SIZE] = 0;
}
#endif

static bool prv_get_more_unique_serial(char *serial_number) {
  for (int i = 2; i < MFG_SERIAL_NUMBER_SIZE; i += 2) {
    sniprintf(&serial_number[i], 3 /* 2 hex digits + zero terminator */, "%02X", rand());
  }
  serial_number[MFG_SERIAL_NUMBER_SIZE] = 0;
  return true;
}

void mfg_write_bigboard_serial_number(void) {
  char serial_number[MFG_SERIAL_NUMBER_SIZE + 1];
  // Start with underscore, so it's easy to filter out from analytics:
  serial_number[0] = '_';
  serial_number[1] = 'B';
  serial_number[2] = 0;

  // Check whether the previous not-so-unique SN or the no SN ("XXXXXXXXXXXX") has been written:
#ifndef CONFIG_SOC_NRF52
  prv_get_not_so_unique_serial(serial_number);
#endif
  const char *current_serial_number = mfg_get_serial_number();

  if (strcmp(current_serial_number, serial_number) && strcmp(current_serial_number, DUMMY_SERIAL)) {
    return;
  }

  // Create a "more unique" serial number using rand():
  if (prv_get_more_unique_serial(serial_number)) {
    mfg_write_serial_number(serial_number, MFG_SERIAL_NUMBER_SIZE, NULL);
  }
}
#endif

#if defined(CONFIG_SHELL) && defined(CONFIG_RECOVERY_FW)
#include <errno.h>
#include <pbl/shell/shell.h>

static int prv_print_feedback(const struct pbl_shell *sh, const MfgSerialsResult result,
                              const uint8_t index, const char *name) {
  switch (result) {
    case MfgSerialsResultAlreadyWritten:
      pbl_shell_error(sh, "%s already present! %s", name, otp_get_slot(index));
      return -EEXIST;
    case MfgSerialsResultCorrupt:
      pbl_shell_error(sh, "Writing failed; %s may be corrupt!", name);
      return -EIO;
    case MfgSerialsResultFailIncorrectLength:
      pbl_shell_error(sh, "Incorrect length");
      return -EINVAL;
    case MfgSerialsResultFailNoMoreSpace:
      pbl_shell_error(sh, "No more space!");
      return -ENOSPC;
    case MfgSerialsResultSuccess:
      pbl_shell_print(sh, "OK");
      return 0;
    default:
      return -EIO;
  }
}

static int prv_cmd_serial(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (argc == 1) {
    pbl_shell_print(sh, "%s", mfg_get_serial_number());
    return 0;
  }

  MfgSerialsResult result;
  uint8_t index = 0;
  size_t len = strlen(argv[1]);
  if ((len >= 11) && (len <= MFG_SERIAL_NUMBER_SIZE)) {
    result = mfg_write_serial_number(argv[1], len, &index);
  } else {
    result = MfgSerialsResultFailIncorrectLength;
  }

  return prv_print_feedback(sh, result, index, "Serial");
}

static int prv_cmd_hwver(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (argc == 1) {
    pbl_shell_print(sh, "%s", mfg_get_hw_version());
    return 0;
  }

  MfgSerialsResult result;
  uint8_t index = 0;
  size_t len = strlen(argv[1]);
  if (len > 0) {
    result = mfg_write_hw_version(argv[1], len, &index);
  } else {
    result = MfgSerialsResultFailIncorrectLength;
  }

  return prv_print_feedback(sh, result, index, "HW version");
}

static int prv_cmd_pcbaserial(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (argc == 1) {
    pbl_shell_print(sh, "%s", mfg_get_pcba_serial_number());
    return 0;
  }

  MfgSerialsResult result;
  uint8_t index = 0;
  size_t len = strlen(argv[1]);
  if ((len > 0) && (len <= MFG_PCBA_SERIAL_NUMBER_SIZE)) {
    result = mfg_write_pcba_serial_number(argv[1], len, &index);
  } else {
    result = MfgSerialsResultFailIncorrectLength;
  }

  return prv_print_feedback(sh, result, index, "PCBA Serial");
}

PBL_SHELL_SUBCMD_ADD(sub_mfg, serial, NULL, "Read or write the serial number [serial]",
                     prv_cmd_serial, 1, 1);
PBL_SHELL_SUBCMD_ADD(sub_mfg, hwver, NULL, "Read or write the HW version [hwver]", prv_cmd_hwver, 1,
                     1);
PBL_SHELL_SUBCMD_ADD(sub_mfg, pcbaserial, NULL, "Read or write the PCBA serial number [serial]",
                     prv_cmd_pcbaserial, 1, 1);
#endif
