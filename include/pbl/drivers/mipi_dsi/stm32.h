/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/mipi_dsi.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_mipi_dsi_stm32 STM32 DSI host
 * @ingroup drivers_mipi_dsi
 * @brief @ref drivers_mipi_dsi host of the STM32H7, in video mode.
 *
 * Attaching a device starts the regulator and the D-PHY PLL, from the reference clock, for 500
 * Mbit/s per lane, the fastest the D-PHY runs, and sets video mode up from the timing of the
 * device at the rate of the pixel clock: the display controller must be streaming by then. The
 * sync signals are active high. Commands go through the generic packet FIFOs, polled.
 * @{
 */

/** @brief Host. */
/** @cond INTERNAL_HIDDEN */
struct pbl_mipi_dsi_stm32_state {
  pbl_mipi_dsi_event_cb_t cb;
  void *arg;
};
/** @endcond */

struct pbl_mipi_dsi_stm32 {
  /** Host. */
  struct pbl_mipi_dsi_host host;
  /** Driver runtime state. */
  struct pbl_mipi_dsi_stm32_state *state;
  /** Wrapper interrupt. */
  pbl_irq_t irq;
  /** Registers. */
  uintptr_t base;
  /** Bus clock. */
  struct pbl_clk pclk;
  /** Reference clock of the D-PHY PLL. */
  struct pbl_clk ref;
  /** Pixel clock of the display controller feeding it. */
  struct pbl_clk px_clk;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_mipi_dsi_host_ops pbl_mipi_dsi_stm32_ops;
int pbl_mipi_dsi_stm32_init(const struct pbl_device *dev);
void pbl_mipi_dsi_stm32_irq_handler(const struct pbl_mipi_dsi_stm32 *dsi);
/** @endcond */

/**
 * @brief Define the host.
 *
 * @param sym Symbol of the host.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Wrapper interrupt.
 * @param _pclk Bus clock, from PBL_CLK().
 * @param _ref Reference clock, from PBL_CLK().
 * @param _px_clk Pixel clock, from PBL_CLK().
 */
#define PBL_MIPI_DSI_STM32_DEFINE(sym, _name, _base, _irq, _pclk, _ref, _px_clk)            \
  PBL_DEVICE_STATE_DEFINE(sym);                                                             \
  static struct pbl_mipi_dsi_stm32_state sym##_dsi_state;                                   \
  PBL_DEVICE_INSTANCE(sym, struct pbl_mipi_dsi_stm32, struct pbl_mipi_dsi_host, host){      \
    .host = PBL_MIPI_DSI_HOST_INIT(sym, _name, pbl_mipi_dsi_stm32_init, nullptr,            \
                                   &pbl_mipi_dsi_stm32_ops),                                \
    .state = &sym##_dsi_state,                                                              \
    .irq = (_irq),                                                                          \
    .base = (_base),                                                                        \
    .pclk = _pclk,                                                                          \
    .ref = _ref,                                                                            \
    .px_clk = _px_clk,                                                                      \
  };                                                                                        \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_MIPI_DSI_STM32_IRQ_PRIO, pbl_mipi_dsi_stm32_irq_handler, \
                      &sym##_inst, 0)

/** @} */
