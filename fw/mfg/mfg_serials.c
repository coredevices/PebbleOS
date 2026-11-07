/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_serials.h"

#include <pbl/util/size.h>

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

PBL_SHELL_SUBCMD_ADD(sub_mfg, serial, nullptr, "Read or write the serial number [serial]",
                     prv_cmd_serial, 1, 1);
PBL_SHELL_SUBCMD_ADD(sub_mfg, hwver, nullptr, "Read or write the HW version [hwver]", prv_cmd_hwver,
                     1, 1);
PBL_SHELL_SUBCMD_ADD(sub_mfg, pcbaserial, nullptr, "Read or write the PCBA serial number [serial]",
                     prv_cmd_pcbaserial, 1, 1);
#endif
