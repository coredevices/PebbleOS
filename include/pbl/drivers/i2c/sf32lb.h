/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/dma.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/pinctrl.h>

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
 * The controller interrupt is connected by the board:
 *
 * @code{.c}
 * PBL_PINCTRL_STATE_DEFINE(s_i2c2_default, SF32LB52_PINCTRL,
 *                          SF32LB52_PA32_I2C2_SCL | PBL_PINCTRL_SF32LB52_INPUT,
 *                          SF32LB52_PA33_I2C2_SDA | PBL_PINCTRL_SF32LB52_INPUT);
 * static const struct pbl_dma_chan s_i2c2_dma =
 *     PBL_DMA_CHAN(SF32LB52_DMAC1, 5, SF32LB52_DMA_REQ_I2C2, SF32LB52_DMA_PL_HIGH);
 * PBL_I2C_SF32LB_DMA_DEFINE(s_i2c2, "i2c2", 2, 400000, &s_i2c2_default, &s_i2c2_dma, NULL);
 * PBL_IRQ_CONNECT(I2C2, 5, pbl_i2c_sf32lb_irq_handler, &s_i2c2, 0);
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
  /** Bus clock frequency, in Hz. */
  uint32_t clock_hz;
  /** Controller instance, from 1. */
  uint8_t index;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_i2c_bus_ops pbl_i2c_sf32lb_ops;
/** @endcond */

/**
 * @brief Define a bus on an SF32LB I2C controller, with an optional receive DMA channel.
 *
 * @param sym Symbol of the bus.
 * @param _name Name.
 * @param _idx Controller instance, 1 to 4.
 * @param _clock_hz Bus clock frequency.
 * @param _pinctrl Pin state with SCL and SDA, from PBL_PINCTRL_STATE_DEFINE().
 * @param _dma Receive DMA channel, or NULL.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_I2C_SF32LB_DMA_DEFINE(sym, _name, _idx, _clock_hz, _pinctrl, _dma, _deps) \
  PBL_I2C_BUS_STATE_DEFINE(sym);                                                      \
  static struct pbl_i2c_sf32lb_state sym##_sf32lb_state;                              \
  const struct pbl_i2c_sf32lb sym = {                                                 \
    .bus = PBL_I2C_BUS_INIT(sym, _name, &pbl_i2c_sf32lb_ops, _deps),                  \
    .state = &sym##_sf32lb_state,                                                     \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                     \
    .dma = (_dma),                                                                    \
    .clock_hz = (_clock_hz),                                                          \
    .index = (_idx),                                                                  \
  };                                                                                  \
  PBL_DEVICE_REGISTER(sym, &sym.bus.dev)

/**
 * @brief Define a bus on an SF32LB I2C controller.
 *
 * @param sym Symbol of the bus.
 * @param _name Name.
 * @param _idx Controller instance, 1 to 4.
 * @param _clock_hz Bus clock frequency.
 * @param _pinctrl Pin state with SCL and SDA, from PBL_PINCTRL_STATE_DEFINE().
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_I2C_SF32LB_DEFINE(sym, _name, _idx, _clock_hz, _pinctrl, _deps) \
  PBL_I2C_SF32LB_DMA_DEFINE(sym, _name, _idx, _clock_hz, _pinctrl, nullptr, _deps)

/**
 * @brief Controller interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param i2c Bus.
 */
void pbl_i2c_sf32lb_irq_handler(const struct pbl_i2c_sf32lb *i2c);

/** @} */
