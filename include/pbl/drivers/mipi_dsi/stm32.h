/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/display_bridge.h>
#include <pbl/drivers/mipi_dsi.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_mipi_dsi_stm32 STM32 DSI host
 * @ingroup drivers_mipi_dsi
 * @brief @ref drivers_mipi_dsi host of the STM32H7, in adapted command mode.
 *
 * Attaching a device starts the regulator and the D-PHY PLL, from the reference clock, for 500
 * Mbit/s per lane, the fastest the D-PHY runs. Each refresh then sends a frame of the display
 * controller to the frame memory of the panel, as the @ref drivers_display_bridge of the
 * controller, @c sym_bridge. The panel reports its tearing effect on the TE pin, when the board
 * gives one in the default pin state of the host, else over the link. Commands go through the
 * generic packet FIFOs, polled. ST's boards drive their panels the same way.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_mipi_dsi_stm32_state {
  pbl_display_bridge_event_cb_t cb;
  void *arg;
};
/** @endcond */

/** @brief Host. */
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
  /** TE pin, in the default state, if the board wires it. */
  struct pbl_pinctrl pinctrl;
};

/** @brief The host as the bridge of its display controller. */
struct pbl_mipi_dsi_stm32_bridge {
  /** Bridge. */
  struct pbl_display_bridge bridge;
  /** Host. */
  const struct pbl_mipi_dsi_stm32 *dsi;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_mipi_dsi_host_ops pbl_mipi_dsi_stm32_ops;
extern const struct pbl_display_bridge_ops pbl_mipi_dsi_stm32_bridge_ops;
int pbl_mipi_dsi_stm32_init(const struct pbl_device *dev);
void pbl_mipi_dsi_stm32_irq_handler(const struct pbl_mipi_dsi_stm32 *dsi);
/** @endcond */

/**
 * @brief Define the host and its bridge.
 *
 * @param sym Symbol of the host, its bridge being @c sym_bridge.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Wrapper interrupt.
 * @param _pclk Bus clock, from PBL_CLK().
 * @param _ref Reference clock, from PBL_CLK().
 * @param _pinctrl Default pin state, with the TE pin, or NULL.
 */
#define PBL_MIPI_DSI_STM32_DEFINE(sym, _name, _base, _irq, _pclk, _ref, _pinctrl)                \
  extern const struct pbl_display_bridge sym##_bridge;                                           \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                  \
  static struct pbl_mipi_dsi_stm32_state sym##_dsi_state;                                        \
  PBL_DEVICE_INSTANCE(sym, struct pbl_mipi_dsi_stm32, struct pbl_mipi_dsi_host, host){           \
    .host = PBL_MIPI_DSI_HOST_INIT(sym, _name, pbl_mipi_dsi_stm32_init, nullptr,                 \
                                   &pbl_mipi_dsi_stm32_ops),                                     \
    .state = &sym##_dsi_state,                                                                   \
    .irq = (_irq),                                                                               \
    .base = (_base),                                                                             \
    .pclk = _pclk,                                                                               \
    .ref = _ref,                                                                                 \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                                \
  };                                                                                             \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_MIPI_DSI_STM32_IRQ_PRIO, pbl_mipi_dsi_stm32_irq_handler,      \
                      &sym##_inst, 0);                                                           \
  PBL_DEVICE_STATE_DEFINE(sym##_bridge);                                                         \
  PBL_DEVICE_INSTANCE(sym##_bridge, struct pbl_mipi_dsi_stm32_bridge, struct pbl_display_bridge, \
                      bridge) {                                                                  \
    .bridge = PBL_DISPLAY_BRIDGE_INIT(sym##_bridge, _name "_bridge", &sym##_inst.host.dev,       \
                                      &pbl_mipi_dsi_stm32_bridge_ops),                           \
    .dsi = &sym##_inst,                                                                          \
  }

/** @} */
