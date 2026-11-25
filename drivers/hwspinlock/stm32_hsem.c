/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/hwspinlock.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/stm32h7_hsem.h>

#define NUM_SEMAPHORES 32U

// The ID this core takes the semaphores with
#ifdef CONFIG_CPU_CORTEX_M4
#define COREID HSEM_COREID_CM4
#else
#define COREID HSEM_COREID_CM7
#endif

struct pbl_hwspinlock_stm32_hsem {
  struct pbl_hwspinlock_controller ctrl;
  uintptr_t base;
  struct pbl_clk clk;
};

static const struct pbl_hwspinlock_stm32_hsem *prv_hsem(
    const struct pbl_hwspinlock_controller *ctrl) {
  return PBL_CONTAINER_OF(ctrl, const struct pbl_hwspinlock_stm32_hsem, ctrl);
}

// Reading the lock register takes a free semaphore, for this core: the read tells who holds it.
// The clock stays on while this core holds it, as the semaphores may also be used before init.
static int prv_trylock(const struct pbl_hwspinlock_controller *ctrl, uint32_t id) {
  const struct pbl_hwspinlock_stm32_hsem *hsem = prv_hsem(ctrl);
  const uint32_t taken = HSEM_R_LOCK | PBL_FIELD_PREP(HSEM_R_COREID_MASK, COREID);
  int ret;

  ret = pbl_clk_enable(&hsem->clk);
  if (ret != 0) {
    return ret;
  }

  if ((pbl_readl(hsem->base + STM32H7_HSEM_RLR_OFFSET(id)) & ~HSEM_R_PROCID_MASK) != taken) {
    (void)pbl_clk_disable(&hsem->clk);
    return -EBUSY;
  }

  return 0;
}

static void prv_unlock(const struct pbl_hwspinlock_controller *ctrl, uint32_t id) {
  const struct pbl_hwspinlock_stm32_hsem *hsem = prv_hsem(ctrl);

  pbl_writel(PBL_FIELD_PREP(HSEM_R_COREID_MASK, COREID), hsem->base + STM32H7_HSEM_R_OFFSET(id));
  (void)pbl_clk_disable(&hsem->clk);
}

static const struct pbl_hwspinlock_controller_ops s_ops = {
  .trylock = prv_trylock,
  .unlock = prv_unlock,
};

// A core stopped or reset while holding a lock left it taken: free those this core held
static int prv_init(const struct pbl_device *dev) {
  const struct pbl_hwspinlock_stm32_hsem *hsem =
      PBL_CONTAINER_OF(dev, const struct pbl_hwspinlock_stm32_hsem, ctrl.dev);
  uint32_t key;
  int ret;

  ret = pbl_clk_enable(&hsem->clk);
  if (ret != 0) {
    return ret;
  }

  key = PBL_FIELD_GET(HSEM_CR_KEY_MASK, pbl_readl(hsem->base + STM32H7_HSEM_KEYR_OFFSET));
  pbl_writel(PBL_FIELD_PREP(HSEM_CR_KEY_MASK, key) | PBL_FIELD_PREP(HSEM_CR_COREID_MASK, COREID),
             hsem->base + STM32H7_HSEM_CR_OFFSET);

  return pbl_clk_disable(&hsem->clk);
}

#define PBL_HWSPINLOCK_STM32_HSEM_DEFINE(sym, _name, _base, _clk)                                  \
  PBL_HWSPINLOCK_CONTROLLER_STATE_DEFINE(sym);                                                     \
  PBL_DEVICE_INSTANCE(sym, struct pbl_hwspinlock_stm32_hsem, struct pbl_hwspinlock_controller,     \
                      ctrl){                                                                       \
    .ctrl = PBL_HWSPINLOCK_CONTROLLER_INIT(sym, _name, prv_init, nullptr, &s_ops, NUM_SEMAPHORES), \
    .base = (_base),                                                                               \
    .clk = _clk,                                                                                   \
  };

#include PBL_DT_INSTANCES(st_stm32_hwspinlock_hwspinlock)
