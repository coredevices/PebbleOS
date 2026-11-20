/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/drivers/accel.h>
#include <pbl/shell/shell.h>

static int prv_accel_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_accel_sample sample;
  int ret;

  ret = pbl_accel_peek(ACCEL, &sample);
  if (ret != 0) {
    pbl_shell_error(sh, "read failed (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "x=%" PRId16 " y=%" PRId16 " z=%" PRId16 " mg", sample.x, sample.y, sample.z);
  return 0;
}

static int prv_accel_info(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_shell_print(sh, "sampling interval: %" PRIu32 " us", pbl_accel_get_sampling_interval(ACCEL));
  pbl_shell_print(sh, "max FIFO samples: %" PRIu32, pbl_accel_get_max_num_samples(ACCEL));
  pbl_shell_print(sh, "shake detection: %s", pbl_accel_get_shake_detection(ACCEL) ? "on" : "off");
  pbl_shell_print(sh, "double tap detection: %s",
                  pbl_accel_get_double_tap_detection(ACCEL) ? "on" : "off");
  return 0;
}

static const struct pbl_shell_cmd sub_accel[] = {
  PBL_SHELL_CMD(read, nullptr, "Read one sample", prv_accel_read),
  PBL_SHELL_CMD(info, nullptr, "Show the driver state", prv_accel_info),
  PBL_SHELL_SUBCMD_SET_END,
};

static const struct pbl_shell_cmd sub_imu[] = {
  PBL_SHELL_CMD(accel, sub_accel, "Accelerometer", nullptr),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(imu, sub_imu, "IMU drivers", nullptr);
