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
 * doorbell.
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

/** @cond INTERNAL_HIDDEN */
#define PBL_MBOX_SF32LB52_FIRST_(_first, ...) _first
/** @endcond */

/**
 * @brief Define a mailbox block.
 *
 * @param sym Symbol.
 * @param _name Device name.
 * @param _base Base address.
 * @param _clk Clock, or PBL_CLK(nullptr, 0) if not controlled by this CPU.
 * @param ... Interrupt, if it reaches this CPU.
 */
#define PBL_MBOX_SF32LB52_DEFINE(sym, _name, _base, _clk, ...)                             \
  PBL_MBOX_CONTROLLER_STATE_DEFINE(sym, PBL_MBOX_SF32LB52_NUM_DOORBELLS);                  \
  PBL_DEVICE_INSTANCE(sym, struct pbl_mbox_sf32lb52, struct pbl_mbox_controller, ctrl){    \
    .ctrl = PBL_MBOX_CONTROLLER_INIT(sym, _name, pbl_mbox_sf32lb52_init, nullptr,          \
                                     &pbl_mbox_sf32lb52_ops, 1U),                          \
    .base = (_base),                                                                       \
    .irqn = PBL_MBOX_SF32LB52_FIRST_(__VA_ARGS__ __VA_OPT__(, ) PBL_MBOX_SF32LB52_NO_IRQ), \
    .clk = _clk,                                                                           \
  };                                                                                       \
  __VA_OPT__(PBL_IRQ_CONNECT_NUM(__VA_ARGS__, CONFIG_MBOX_SF32LB52_IRQ_PRIO,               \
                                 pbl_mbox_sf32lb52_irq_handler, &sym##_inst, 0))

/**
 * @brief Interrupt handler of a block rung by the remote, connected by PBL_MBOX_SF32LB52_DEFINE().
 *
 * @param mbox Mailbox.
 */
void pbl_mbox_sf32lb52_irq_handler(const struct pbl_mbox_sf32lb52 *mbox);

/** @} */
