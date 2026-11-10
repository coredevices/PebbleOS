/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/dma.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_i2c_sf32lb SF32LB I2C
 * @ingroup drivers_i2c
 * @brief @ref drivers_i2c bus driver for the SF32LB I2C controller.
 *
 * Transfers are interrupt driven and issue any message sequence. Deep sleep is blocked while a
 * transfer is in flight. A bus given a DMA channel receives @ref PBL_I2C_MSG_DMA_SAFE reads of
 * @ref I2C_SF32LB_DMA_MIN_BYTES or more through DMA, so the CPU can sleep through them instead
 * of taking an interrupt per byte; the DMA channel needs no interrupt.
 *
 * An instance connects its controller interrupt itself, with priority
 * @c CONFIG_I2C_SF32LB_IRQ_PRIO:
 *
 * @code{.c}
 * PBL_PINCTRL_STATE_DEFINE(s_i2c2_default, SF32LB52_PINCTRL,
 *                          SF32LB52_PA32_I2C2_SCL | PBL_PINCTRL_SF32LB52_INPUT,
 *                          SF32LB52_PA33_I2C2_SDA | PBL_PINCTRL_SF32LB52_INPUT);
 * static const struct pbl_dma_chan s_i2c2_dma =
 *     PBL_DMA_CHAN(&dt_dmac1, 5, SF32LB52_DMA_REQ_I2C2, SF32LB52_DMA_PL_HIGH);
 * PBL_I2C_SF32LB_DEFINE(s_i2c2, "i2c2", SF32LB52_I2C2_BASE, PBL_SOC_IRQN_I2C2,
 *                       PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_I2C2), 400000,
 *                       &s_i2c2_default, &s_i2c2_dma);
 * @endcode
 * @{
 */

/** @brief Shortest read that goes through DMA. */
#define I2C_SF32LB_DMA_MIN_BYTES 32

/** @cond INTERNAL_HIDDEN */
struct pbl_i2c_sf32lb_state {
  struct pbl_i2c_msg *msgs;
  uint8_t num_msgs;
  uint8_t idx;
  uint32_t pos;
  uint32_t chunk;
  uint16_t addr;
  bool addr_phase;
  bool stop_sent;
  bool dma;
  bool sleep_blocked;
  int error;
  uint16_t flv;
};
/** @endcond */

/** @brief SF32LB I2C bus. */
struct pbl_i2c_sf32lb {
  /** I2C bus. */
  struct pbl_i2c_bus bus;
  /** Driver runtime state. */
  struct pbl_i2c_sf32lb_state *state;
  /** SCL and SDA pins. */
  struct pbl_pinctrl pinctrl;
  /** Receive DMA channel, or NULL. */
  const struct pbl_dma_chan *dma;
  /** Controller clock. */
  struct pbl_clk clk;
  /** Registers. */
  uintptr_t base;
  /** Bus clock frequency, in Hz. */
  uint32_t clock_hz;
  /** Interrupt line. */
  pbl_irq_t irq;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_i2c_bus_ops pbl_i2c_sf32lb_ops;
/** @endcond */

/**
 * @brief Define a bus on an SF32LB I2C controller.
 *
 * @param sym Symbol of the bus, a struct pbl_i2c_bus.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Interrupt line, a plain number such as @c PBL_SOC_IRQN_I2C1.
 * @param _clk Controller clock, from PBL_CLK().
 * @param _clock_hz Bus clock frequency.
 * @param _pinctrl Pin state with SCL and SDA.
 * @param _dma Receive DMA channel, or NULL.
 */
#define PBL_I2C_SF32LB_DEFINE(sym, _name, _base, _irq, _clk, _clock_hz, _pinctrl, _dma) \
  PBL_I2C_BUS_STATE_DEFINE(sym);                                                        \
  static struct pbl_i2c_sf32lb_state sym##_sf32lb_state;                                \
  PBL_DEVICE_INSTANCE(sym, struct pbl_i2c_sf32lb, struct pbl_i2c_bus, bus){             \
    .bus = PBL_I2C_BUS_INIT(sym, _name, &pbl_i2c_sf32lb_ops, nullptr),                  \
    .state = &sym##_sf32lb_state,                                                       \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                       \
    .dma = (_dma),                                                                      \
    .clk = _clk,                                                                        \
    .base = (_base),                                                                    \
    .clock_hz = (_clock_hz),                                                            \
    .irq = (_irq),                                                                      \
  };                                                                                    \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_I2C_SF32LB_IRQ_PRIO, pbl_i2c_sf32lb_irq_handler, &sym##_inst, 0)

/**
 * @brief Controller interrupt handler, connected by PBL_I2C_SF32LB_DEFINE().
 *
 * @param i2c Bus.
 */
void pbl_i2c_sf32lb_irq_handler(const struct pbl_i2c_sf32lb *i2c);

/** @} */
