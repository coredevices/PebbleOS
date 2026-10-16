/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/als.h>
#include <pbl/drivers/ambient/qemu.h>

static int prv_init(const struct pbl_als *als) {
  return 0;
}

static int prv_read(const struct pbl_als *als, uint32_t *level) {
  *level = 0U;

  return 0;
}

static const struct pbl_als_ops s_ops = {
  .init = prv_init,
  .read = prv_read,
};

PBL_ALS_STATE_DEFINE(pbl_als_qemu);
const struct pbl_als pbl_als_qemu = PBL_ALS_INIT(pbl_als_qemu, "als", nullptr, &s_ops, nullptr);
PBL_DEVICE_REGISTER(pbl_als_qemu, &pbl_als_qemu.dev);
