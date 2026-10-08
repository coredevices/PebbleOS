/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>

#include <pbl/drivers/accel.h>
#include <pbl/drivers/mag.h>
#include <pbl/shell/shell.h>

#include <kernel/util/sleep.h>

static int prv_accel_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  AccelDriverSample sample;
  int ret;

  ret = accel_peek(&sample);
  if (ret != 0) {
    pbl_shell_error(sh, "read failed (%d)", ret);
    return -EIO;
  }

  pbl_shell_print(sh, "x=%" PRId16 " y=%" PRId16 " z=%" PRId16 " mg", sample.x, sample.y, sample.z);
  return 0;
}

static int prv_accel_info(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_shell_print(sh, "sampling interval: %" PRIu32 " us", accel_get_sampling_interval());
  pbl_shell_print(sh, "max FIFO samples: %" PRIu32, accel_get_max_num_samples());
  pbl_shell_print(sh, "shake detection: %s", accel_get_shake_detection_enabled() ? "on" : "off");
  pbl_shell_print(sh, "double tap detection: %s",
                  accel_get_double_tap_detection_enabled() ? "on" : "off");
  return 0;
}

static const struct pbl_shell_cmd sub_accel[] = {
  PBL_SHELL_CMD(read, NULL, "Read one sample", prv_accel_read),
  PBL_SHELL_CMD(info, NULL, "Show the driver state", prv_accel_info),
  PBL_SHELL_SUBCMD_SET_END,
};

#ifdef CONFIG_MAG
static int prv_mag_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  MagData data;
  MagReadStatus status;

  mag_start_sampling();
  psleep(250);
  status = mag_read_data(&data);
  mag_release();

  if (status != MagReadSuccess) {
    pbl_shell_error(sh, "read failed (%d)", status);
    return -EIO;
  }

  pbl_shell_print(sh, "x=%d y=%d z=%d", data.x, data.y, data.z);
  return 0;
}

static const struct pbl_shell_cmd sub_mag[] = {
  PBL_SHELL_CMD(read, NULL, "Read one sample", prv_mag_read),
  PBL_SHELL_SUBCMD_SET_END,
};
#endif

static const struct pbl_shell_cmd sub_imu[] = {
  PBL_SHELL_CMD(accel, sub_accel, "Accelerometer", NULL),
#ifdef CONFIG_MAG
  PBL_SHELL_CMD(mag, sub_mag, "Magnetometer", NULL),
#endif
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(imu, sub_imu, "IMU drivers", NULL);
