/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/touch.h>
#include <pbl/drivers/touch/qemu.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/work.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#define TOUCH_STATE   0x00
#define TOUCH_X       0x04
#define TOUCH_Y       0x08
#define TOUCH_INTCTRL 0x0C
#define TOUCH_INTSTAT 0x10

#define TOUCH_STATE_DOWN PBL_BIT(0)
#define TOUCH_INT_EVENT  PBL_BIT(0)

static const struct pbl_touch_qemu *prv_qemu(const struct pbl_touch *touch) {
  return container_of(touch, const struct pbl_touch_qemu, touch);
}

static void prv_work(void *arg) {
  const struct pbl_touch_qemu *qemu = arg;
  uint32_t state;
  uint16_t x;
  uint16_t y;

  qemu->state->work_pending = false;

  state = pbl_readl(qemu->base + TOUCH_STATE);
  x = (uint16_t)pbl_readl(qemu->base + TOUCH_X);
  y = (uint16_t)pbl_readl(qemu->base + TOUCH_Y);

  pbl_touch_report(&qemu->touch, (state & TOUCH_STATE_DOWN) != 0U, x, y);
}

void pbl_touch_qemu_irq_handler(const struct pbl_touch_qemu *qemu) {
  pbl_writel(TOUCH_INT_EVENT, qemu->base + TOUCH_INTSTAT);

  if (qemu->state->work_pending) {
    return;
  }

  qemu->state->work_pending = pbl_sys_work_call_from_isr(prv_work, (void *)qemu);
}

static int prv_init(const struct pbl_touch *touch) {
  const struct pbl_touch_qemu *qemu = prv_qemu(touch);

  pbl_writel(0U, qemu->base + TOUCH_INTCTRL);
  pbl_writel(TOUCH_INT_EVENT, qemu->base + TOUCH_INTSTAT);

  pbl_irq_enable(qemu->irq);

  return 0;
}

static int prv_enable(const struct pbl_touch *touch) {
  pbl_writel(TOUCH_INT_EVENT, prv_qemu(touch)->base + TOUCH_INTCTRL);

  return 0;
}

static int prv_disable(const struct pbl_touch *touch) {
  pbl_writel(0U, prv_qemu(touch)->base + TOUCH_INTCTRL);

  return 0;
}

const struct pbl_touch_ops pbl_touch_qemu_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
};

#include PBL_DT_INSTANCES(qemu_pebble_touch)
