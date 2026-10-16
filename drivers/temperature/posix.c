/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/temperature.h>
#include <pbl/drivers/temperature/posix.h>

static int prv_read(const struct pbl_temp *temp, int32_t *millicelsius) {
  *millicelsius = 25000;

  return 0;
}

static const struct pbl_temp_ops s_ops = {
  .read = prv_read,
};

PBL_TEMP_STATE_DEFINE(pbl_temp_posix);
const struct pbl_temp pbl_temp_posix =
    PBL_TEMP_INIT(pbl_temp_posix, "temp", nullptr, nullptr, &s_ops);
PBL_DEVICE_REGISTER(pbl_temp_posix, &pbl_temp_posix.dev);
