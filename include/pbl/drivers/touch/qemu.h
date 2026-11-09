/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/touch.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_touch_qemu QEMU
 * @ingroup drivers_touch
 * @brief QEMU touchscreen, covering the display.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_touch_qemu_state {
  bool work_pending;
};
/** @endcond */

/** @brief QEMU touchscreen. */
struct pbl_touch_qemu {
  /** Touchscreen. */
  struct pbl_touch touch;
  /** Register base address. */
  uintptr_t base;
  /** Interrupt line. */
  pbl_irq_t irq;
  /** Runtime state. */
  struct pbl_touch_qemu_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_touch_ops pbl_touch_qemu_ops;
/** @endcond */

/**
 * @brief Define a QEMU touchscreen.
 *
 * @param sym Symbol of the touchscreen.
 * @param _name Name.
 * @param _base Register base address.
 * @param _irq Interrupt line.
 * @param _size_x Number of X coordinates it reports.
 * @param _size_y Number of Y coordinates it reports.
 */
#define PBL_TOUCH_QEMU_DEFINE(sym, _name, _base, _irq, _size_x, _size_y)                   \
  PBL_TOUCH_STATE_DEFINE(sym);                                                             \
  static struct pbl_touch_qemu_state sym##_qemu_state;                                     \
  PBL_DEVICE_INSTANCE(sym, struct pbl_touch_qemu, struct pbl_touch, touch){                \
    .touch = PBL_TOUCH_INIT(sym, _name, nullptr, &pbl_touch_qemu_ops, .size_x = (_size_x), \
                            .size_y = (_size_y)),                                          \
    .base = (_base),                                                                       \
    .irq = (_irq),                                                                         \
    .state = &sym##_qemu_state,                                                            \
  };                                                                                       \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_TOUCH_QEMU_IRQ_PRIO, pbl_touch_qemu_irq_handler, &sym##_inst, 0)

/**
 * @brief Interrupt handler, connected by PBL_TOUCH_QEMU_DEFINE().
 *
 * @param touch Touchscreen.
 */
void pbl_touch_qemu_irq_handler(const struct pbl_touch_qemu *touch);

/** @} */
