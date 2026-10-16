/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/drivers/accel.h>
#include <pbl/drivers/mag.h>
#include <pbl/kernel/delay.h>
#include <pbl/shell/shell.h>

static int prv_accel_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  AccelDriverSample sample;
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

#ifdef CONFIG_MAG
static int prv_mag_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  bool sampling = pbl_mag_get_sampling_interval(MAG) != 0U;
  MagData sample;
  int ret;

  // Sample briefly unless another user already is
  if (!sampling) {
    ret = pbl_mag_set_sampling_interval(MAG, 200000U, nullptr);
    if (ret != 0) {
      pbl_shell_error(sh, "could not start sampling (%d)", ret);
      return ret;
    }

    pbl_sleep_ms(250);
  }

  ret = pbl_mag_peek(MAG, &sample);

  if (!sampling) {
    (void)pbl_mag_set_sampling_interval(MAG, 0U, nullptr);
  }

  if (ret != 0) {
    pbl_shell_error(sh, "read failed (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "x=%" PRId16 " y=%" PRId16 " z=%" PRId16 " mG", sample.x, sample.y, sample.z);
  return 0;
}

static int prv_mag_info(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_shell_print(sh, "sampling interval: %" PRIu32 " us", pbl_mag_get_sampling_interval(MAG));
  return 0;
}

static const struct pbl_shell_cmd sub_mag[] = {
  PBL_SHELL_CMD(read, nullptr, "Read one sample", prv_mag_read),
  PBL_SHELL_CMD(info, nullptr, "Show the driver state", prv_mag_info),
  PBL_SHELL_SUBCMD_SET_END,
};
#endif

static const struct pbl_shell_cmd sub_imu[] = {
  PBL_SHELL_CMD(accel, sub_accel, "Accelerometer", nullptr),
#ifdef CONFIG_MAG
  PBL_SHELL_CMD(mag, sub_mag, "Magnetometer", nullptr),
#endif
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(imu, sub_imu, "IMU drivers", nullptr);
