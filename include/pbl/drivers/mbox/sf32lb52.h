/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/mbox.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_mbox_sf32lb52 SF32LB52 mailbox
 * @ingroup drivers_mbox
 * @brief SiFli SF32LB52 mailbox.
 *
 * One mailbox block, with 16 doorbells on channel 1. Writing a doorbell interrupts the other
 * subsystem's CPU, so a block either rings the remote (MAILBOX1, from the HCPU) or, when its
 * interrupt reaches this CPU, is rung by it (MAILBOX2, LCPU2HCPU). Specifiers have one cell, the
 * doorbell:
 *
 * @code{.c}
 * PBL_MBOX_SF32LB52_DEFINE(s_mailbox1, "mailbox1", SF32LB52_MAILBOX1_BASE,
 *                          PBL_MBOX_SF32LB52_NO_IRQ,
 *                          PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_MAILBOX1), nullptr);
 * PBL_MBOX_SF32LB52_DEFINE(s_mailbox2, "mailbox2", SF32LB52_MAILBOX2_BASE, PBL_IRQN(LCPU2HCPU),
 *                          PBL_CLK(nullptr, 0), nullptr);
 * PBL_IRQ_CONNECT(LCPU2HCPU, 5, pbl_mbox_sf32lb52_irq_handler, &s_mailbox2, 0);
 *
 * static const struct pbl_mbox_chan s_tx = PBL_MBOX_CHAN(&s_mailbox1.ctrl, 0);
 * @endcode
 * @{
 */

/** @brief Interrupt of a block whose interrupt does not reach this CPU. */
#define PBL_MBOX_SF32LB52_NO_IRQ UINT16_MAX

/** @brief Doorbells per block. */
#define PBL_MBOX_SF32LB52_NUM_DOORBELLS 16U

/** @brief SF32LB52 mailbox. */
struct pbl_mbox_sf32lb52 {
  /** Controller. */
  struct pbl_mbox_controller ctrl;
  /** Base address. */
  uintptr_t base;
  /** Interrupt, or PBL_MBOX_SF32LB52_NO_IRQ. */
  pbl_irq_t irqn;
  /** Clock, if controlled by this CPU. */
  struct pbl_clk clk;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_mbox_controller_ops pbl_mbox_sf32lb52_ops;
int pbl_mbox_sf32lb52_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define and register a mailbox block.
 *
 * @param sym Symbol.
 * @param _name Device name.
 * @param _base Base address.
 * @param _irqn Interrupt, or PBL_MBOX_SF32LB52_NO_IRQ if it does not reach this CPU.
 * @param _clk Clock, or PBL_CLK(nullptr, 0) if not controlled by this CPU.
 * @param _deps Device dependencies.
 */
#define PBL_MBOX_SF32LB52_DEFINE(sym, _name, _base, _irqn, _clk, _deps)         \
  PBL_MBOX_CONTROLLER_STATE_DEFINE(sym, PBL_MBOX_SF32LB52_NUM_DOORBELLS);       \
  const struct pbl_mbox_sf32lb52 sym = {                                        \
    .ctrl = PBL_MBOX_CONTROLLER_INIT(sym, _name, pbl_mbox_sf32lb52_init, _deps, \
                                     &pbl_mbox_sf32lb52_ops, 1U),               \
    .base = (_base),                                                            \
    .irqn = (_irqn),                                                            \
    .clk = _clk,                                                                \
  };                                                                            \
  PBL_DEVICE_REGISTER(sym, &sym.ctrl.dev)

/**
 * @brief Interrupt handler of a block rung by the remote, to connect with PBL_IRQ_CONNECT().
 *
 * @param mbox Mailbox.
 */
void pbl_mbox_sf32lb52_irq_handler(const struct pbl_mbox_sf32lb52 *mbox);

/** @} */
