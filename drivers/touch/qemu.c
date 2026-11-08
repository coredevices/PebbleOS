/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/touch.h>
#include <pbl/drivers/touch/qemu.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/work.h>
#include <pbl/util/bits.h>

#include <hardware/qemu_memorymap.h>

#define TOUCH_STATE   0x00
#define TOUCH_X       0x04
#define TOUCH_Y       0x08
#define TOUCH_INTCTRL 0x0C
#define TOUCH_INTSTAT 0x10

#define TOUCH_STATE_DOWN PBL_BIT(0)
#define TOUCH_INT_EVENT  PBL_BIT(0)

static bool s_work_pending;

static void prv_work(void *arg) {
  uint32_t state;
  uint16_t x;
  uint16_t y;

  s_work_pending = false;

  state = pbl_readl(QEMU_TOUCH_BASE + TOUCH_STATE);
  x = (uint16_t)pbl_readl(QEMU_TOUCH_BASE + TOUCH_X);
  y = (uint16_t)pbl_readl(QEMU_TOUCH_BASE + TOUCH_Y);

  pbl_touch_report(QEMU_TOUCH, (state & TOUCH_STATE_DOWN) != 0U, x, y);
}

static void prv_irq_handler(void) {
  pbl_writel(TOUCH_INT_EVENT, QEMU_TOUCH_BASE + TOUCH_INTSTAT);

  if (s_work_pending) {
    return;
  }

  s_work_pending = pbl_sys_work_call_from_isr(prv_work, nullptr);
}

PBL_IRQ_CONNECT(TOUCH, 6, prv_irq_handler, , 0);

static int prv_init(const struct pbl_touch *touch) {
  pbl_writel(0U, QEMU_TOUCH_BASE + TOUCH_INTCTRL);
  pbl_writel(TOUCH_INT_EVENT, QEMU_TOUCH_BASE + TOUCH_INTSTAT);

  pbl_irq_enable(PBL_IRQN(TOUCH));

  return 0;
}

static int prv_enable(const struct pbl_touch *touch) {
  pbl_writel(TOUCH_INT_EVENT, QEMU_TOUCH_BASE + TOUCH_INTCTRL);

  return 0;
}

static int prv_disable(const struct pbl_touch *touch) {
  pbl_writel(0U, QEMU_TOUCH_BASE + TOUCH_INTCTRL);

  return 0;
}

static const struct pbl_touch_ops s_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
};

PBL_TOUCH_STATE_DEFINE(pbl_touch_qemu);
const struct pbl_touch pbl_touch_qemu =
    PBL_TOUCH_INIT(pbl_touch_qemu, "touch", nullptr, &s_ops, .size_x = CONFIG_TOUCH_QEMU_SIZE_X,
                   .size_y = CONFIG_TOUCH_QEMU_SIZE_Y);
PBL_DEVICE_REGISTER(pbl_touch_qemu, &pbl_touch_qemu.dev);
