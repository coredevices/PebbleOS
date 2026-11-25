/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/hwspinlock.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>

PBL_DEVICE_CLASS_DEFINE(pbl_hwspinlock_controller_class, "hwspinlock");

int pbl_hwspinlock_lock(const struct pbl_hwspinlock *lock) {
  const struct pbl_hwspinlock_controller *ctrl = lock->ctrl;

  pbl_irq_lock();

  if (ctrl == nullptr) {
    return 0;
  }

  if (lock->id >= ctrl->num_locks) {
    pbl_irq_unlock();
    return -EINVAL;
  }

  for (uint32_t waited = 0U;; waited++) {
    int ret = ctrl->ops->trylock(ctrl, lock->id);

    if (ret == 0) {
      break;
    }

    if (ret != -EBUSY || waited == CONFIG_HWSPINLOCK_TIMEOUT_US) {
      pbl_irq_unlock();
      return ret == -EBUSY ? -ETIMEDOUT : ret;
    }

    pbl_busy_wait_us(1U);
  }

  return 0;
}

void pbl_hwspinlock_unlock(const struct pbl_hwspinlock *lock) {
  if (lock->ctrl != nullptr) {
    lock->ctrl->ops->unlock(lock->ctrl, lock->id);
  }

  pbl_irq_unlock();
}
