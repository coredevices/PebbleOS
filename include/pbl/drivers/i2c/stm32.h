/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_i2c_stm32 STM32 I2C
 * @ingroup drivers_i2c
 * @brief @ref drivers_i2c driver for the I2C controllers of the STM32H7, in master mode.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_i2c_stm32_state {
  struct pbl_i2c_msg *msgs;
  uint8_t num_msgs;
  // Message and byte the transfer is at, and the end of the segment it is in
  uint8_t idx;
  uint8_t seg_end;
  uint32_t pos;
  // Bytes left in the segment, and moved since it started
  uint32_t left;
  uint32_t moved;
  uint16_t addr;
  bool read;
  int error;
  uint32_t timingr;
};
/** @endcond */

/** @brief STM32 I2C bus. */
struct pbl_i2c_stm32 {
  /** Bus. */
  struct pbl_i2c_bus bus;
  /** Driver runtime state. */
  struct pbl_i2c_stm32_state *state;
  /** SCL and SDA pins. */
  struct pbl_pinctrl pinctrl;
  /** Kernel clock. */
  struct pbl_clk clk;
  /** Registers. */
  uintptr_t base;
  /** Bus clock frequency, in Hz. */
  uint32_t clock_hz;
  /** Event and error interrupts. */
  pbl_irq_t irq_ev;
  pbl_irq_t irq_er;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_i2c_bus_ops pbl_i2c_stm32_ops;
/** @endcond */

/**
 * @brief Controller interrupt handler, connected by PBL_I2C_STM32_DEFINE().
 *
 * @param i2c Bus.
 */
void pbl_i2c_stm32_irq_handler(const struct pbl_i2c_stm32 *i2c);

/**
 * @brief Define an STM32 I2C bus.
 *
 * @param sym Symbol.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq_ev Event interrupt.
 * @param _irq_er Error interrupt.
 * @param _clk Kernel clock, from PBL_CLK().
 * @param _clock_hz Bus clock frequency, in Hz.
 * @param _pinctrl Pin states.
 */
#define PBL_I2C_STM32_DEFINE(sym, _name, _base, _irq_ev, _irq_er, _clk, _clock_hz, _pinctrl)      \
  PBL_I2C_BUS_STATE_DEFINE(sym);                                                                  \
  static struct pbl_i2c_stm32_state sym##_stm32_state;                                            \
  PBL_DEVICE_INSTANCE(sym, struct pbl_i2c_stm32, struct pbl_i2c_bus, bus){                        \
    .bus = PBL_I2C_BUS_INIT(sym, _name, nullptr, &pbl_i2c_stm32_ops),                             \
    .state = &sym##_stm32_state,                                                                  \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                                 \
    .clk = _clk,                                                                                  \
    .base = (_base),                                                                              \
    .clock_hz = (_clock_hz),                                                                      \
    .irq_ev = (_irq_ev),                                                                          \
    .irq_er = (_irq_er),                                                                          \
  };                                                                                              \
  PBL_IRQ_CONNECT_NUM(_irq_ev, CONFIG_I2C_STM32_IRQ_PRIO, pbl_i2c_stm32_irq_handler, &sym##_inst, \
                      0);                                                                         \
  PBL_IRQ_CONNECT_NUM(_irq_er, CONFIG_I2C_STM32_IRQ_PRIO, pbl_i2c_stm32_irq_handler, &sym##_inst, 0)

/** @} */
