/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/temperature.h>
#include <pbl/drivers/temperature/qemu.h>

static int prv_read(const struct pbl_temp *temp, int32_t *millicelsius) {
  *millicelsius = 25000;

  return 0;
}

static const struct pbl_temp_ops s_ops = {
  .read = prv_read,
};

PBL_TEMP_STATE_DEFINE(pbl_temp_qemu);
const struct pbl_temp pbl_temp_qemu =
    PBL_TEMP_INIT(pbl_temp_qemu, "temp", nullptr, nullptr, &s_ops);
PBL_DEVICE_REGISTER(pbl_temp_qemu, &pbl_temp_qemu.dev);
