/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/i2c.h>
#include <pbl/drivers/pinctrl.h>

#include <bf0_hal.h>

/**
 * @defgroup drivers_i2c_sf32lb SF32LB I2C
 * @ingroup drivers_i2c
 * @brief @ref drivers_i2c bus driver for the SF32LB I2C controller.
 *
 * Deep sleep is blocked while a transfer is in flight. A bus given a receive DMA channel serves
 * pbl_i2c_read_register_block_dma() reads of @ref I2C_SF32LB_DMA_MIN_BYTES or more through DMA,
 * so the CPU can sleep through the transfer instead of taking an interrupt per byte.
 *
 * The controller interrupt, and the DMA one if any, are connected by the board:
 *
 * @code{.c}
 * PBL_PINCTRL_STATE_DEFINE(s_i2c2_default, SF32LB52_PINCTRL,
 *                          SF32LB52_PA32_I2C2_SCL | PBL_PINCTRL_SF32LB52_INPUT,
 *                          SF32LB52_PA33_I2C2_SDA | PBL_PINCTRL_SF32LB52_INPUT);
 * PBL_I2C_SF32LB_DMA_DEFINE(s_i2c2, "i2c2", I2C2, 400000, &s_i2c2_default, DMA1_Channel6,
 *                           DMA_REQUEST_23, DMAC1_CH6, 5, NULL);
 * PBL_IRQ_CONNECT(I2C2, 5, pbl_i2c_sf32lb_irq_handler, &s_i2c2, 0);
 * PBL_IRQ_CONNECT(DMAC1_CH6, 5, pbl_i2c_sf32lb_dma_irq_handler, &s_i2c2, 0);
 * @endcode
 * @{
 */

/** @brief Shortest read that goes through DMA. */
#define I2C_SF32LB_DMA_MIN_BYTES 32

/** @cond INTERNAL_HIDDEN */
struct pbl_i2c_sf32lb_state {
  I2C_HandleTypeDef hdl;
  bool deepsleep_blocked;
  DMA_HandleTypeDef hdma_rx;
  uint8_t *dma_data;
  uint32_t dma_size;
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
  /** Controller clock module. */
  RCC_MODULE_TYPE module;
  /** Controller interrupt. */
  IRQn_Type irqn;
  /** Receive DMA interrupt, when the bus has a receive DMA channel. */
  IRQn_Type dma_irqn;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_i2c_bus_ops pbl_i2c_sf32lb_ops;

#define PBL_I2C_SF32LB_DEFINE_IMPL(sym, _name, _inst, _module, _irqn, _clock_hz, _pinctrl, \
                                   _dma_ch, _dma_req, _dma_irqn, _dma_prio, _deps)         \
  PBL_I2C_BUS_STATE_DEFINE(sym);                                                           \
  static struct pbl_i2c_sf32lb_state sym##_sf32lb_state = {                                \
    .hdl =                                                                                 \
        {                                                                                  \
          .Instance = _inst,                                                               \
          .Init =                                                                          \
              {                                                                            \
                .AddressingMode = I2C_ADDRESSINGMODE_7BIT,                                 \
                .ClockSpeed = (_clock_hz),                                                 \
                .GeneralCallMode = I2C_GENERALCALL_DISABLE,                                \
              },                                                                           \
          .Mode = HAL_I2C_MODE_MASTER,                                                     \
          .core = CORE_ID_HCPU,                                                            \
        },                                                                                 \
    .hdma_rx = {                                                                           \
      .Instance = _dma_ch,                                                                 \
      .Init = {                                                                            \
        .Request = (_dma_req),                                                             \
        .IrqPrio = (_dma_prio),                                                            \
      },                                                                                   \
    },                                                                                     \
  };                                                                                       \
  const struct pbl_i2c_sf32lb sym = {                                                      \
    .bus = PBL_I2C_BUS_INIT(sym, _name, &pbl_i2c_sf32lb_ops, _deps),                       \
    .state = &sym##_sf32lb_state,                                                          \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                          \
    .module = (_module),                                                                   \
    .irqn = (_irqn),                                                                       \
    .dma_irqn = (_dma_irqn),                                                               \
  };                                                                                       \
  PBL_DEVICE_REGISTER(sym, &sym.bus.dev)
/** @endcond */

/**
 * @brief Define a bus on an SF32LB I2C controller, in 7-bit master mode.
 *
 * @param sym Symbol of the bus.
 * @param _name Name.
 * @param _inst Controller, @c I2C1 to @c I2C4.
 * @param _clock_hz Bus clock frequency.
 * @param _pinctrl Pin state with SCL and SDA, from PBL_PINCTRL_STATE_DEFINE().
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_I2C_SF32LB_DEFINE(sym, _name, _inst, _clock_hz, _pinctrl, _deps)              \
  PBL_I2C_SF32LB_DEFINE_IMPL(sym, _name, _inst, RCC_MOD_##_inst, _inst##_IRQn, _clock_hz, \
                             _pinctrl, nullptr, 0, (IRQn_Type)0, 0, _deps)

/**
 * @brief Define a bus on an SF32LB I2C controller, with a receive DMA channel.
 *
 * @param sym Symbol of the bus.
 * @param _name Name.
 * @param _inst Controller, @c I2C1 to @c I2C4.
 * @param _clock_hz Bus clock frequency.
 * @param _pinctrl Pin state with SCL and SDA, from PBL_PINCTRL_STATE_DEFINE().
 * @param _dma_ch DMA channel, e.g. @c DMA1_Channel6.
 * @param _dma_req DMA request, e.g. @c DMA_REQUEST_23.
 * @param _dma_irq DMA channel interrupt line, e.g. @c DMAC1_CH6.
 * @param _dma_prio DMA interrupt priority, as connected.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_I2C_SF32LB_DMA_DEFINE(sym, _name, _inst, _clock_hz, _pinctrl, _dma_ch, _dma_req, \
                                  _dma_irq, _dma_prio, _deps)                                \
  PBL_I2C_SF32LB_DEFINE_IMPL(sym, _name, _inst, RCC_MOD_##_inst, _inst##_IRQn, _clock_hz,    \
                             _pinctrl, _dma_ch, _dma_req, _dma_irq##_IRQn, _dma_prio, _deps)

/**
 * @brief Controller interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param i2c Bus.
 */
void pbl_i2c_sf32lb_irq_handler(const struct pbl_i2c_sf32lb *i2c);

/**
 * @brief Receive DMA interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param i2c Bus.
 */
void pbl_i2c_sf32lb_dma_irq_handler(const struct pbl_i2c_sf32lb *i2c);

/** @} */
