/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/reset.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/irq.h>

static int prv_call(const struct pbl_reset *rst,
                    int (*op)(const struct pbl_reset_controller *ctrl, uint32_t id)) {
  int ret;

  PBL_ASSERTN(pbl_device_is_ready(&rst->ctrl->dev));

  if (rst->id >= rst->ctrl->num_resets) {
    return -EINVAL;
  }

  pbl_irq_lock();
  ret = op(rst->ctrl, rst->id);
  pbl_irq_unlock();

  return ret;
}

int pbl_reset_assert(const struct pbl_reset *rst) {
  return prv_call(rst, rst->ctrl->ops->assert);
}

int pbl_reset_deassert(const struct pbl_reset *rst) {
  return prv_call(rst, rst->ctrl->ops->deassert);
}

int pbl_reset_reset(const struct pbl_reset *rst) {
  int ret;

  ret = pbl_reset_assert(rst);
  if (ret != 0) {
    return ret;
  }

  return pbl_reset_deassert(rst);
}
