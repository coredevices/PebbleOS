/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_clk Clock
 * @ingroup drivers
 * @brief Clock controller class.
 *
 * A clock controller is a struct pbl_clk_controller device providing a set of clocks, each
 * identified by a provider-specific number defined in the controller header.
 * A consumer holds a struct pbl_clk naming the controller and clock, lists the controller among
 * its device dependencies, and enables the clock while it uses the peripheral. Enables are
 * counted per clock, so drivers sharing a clock never gate it under each other, and are safe to
 * call from any context:
 *
 * @code{.c}
 * static const struct pbl_clk s_clk = PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_TRNG);
 *
 * pbl_clk_enable(&s_clk);
 * // use the peripheral
 * pbl_clk_disable(&s_clk);
 * @endcode
 * @{
 */

struct pbl_clk_controller;

/** @brief Clock controller driver operations, called with interrupts locked. */
struct pbl_clk_controller_ops {
  /** Ungate a clock. Returns 0 or a negative errno. */
  int (*enable)(const struct pbl_clk_controller *ctrl, uint32_t id);
  /** Gate a clock. Returns 0 or a negative errno. */
  int (*disable)(const struct pbl_clk_controller *ctrl, uint32_t id);
};

/** @brief A clock controller. */
struct pbl_clk_controller {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_clk_controller_ops *ops;
  /** Enable count of each clock. */
  uint8_t *use_counts;
  /** Number of clocks. */
  uint16_t num_clks;
};

/**
 * @brief Define the class state of clock controller @p sym, for the driver's define macro.
 *
 * @param sym Symbol of the controller instance.
 * @param _num_clks Number of clocks.
 */
#define PBL_CLK_CONTROLLER_STATE_DEFINE(sym, _num_clks) \
  PBL_DEVICE_STATE_DEFINE(sym);                         \
  static uint8_t sym##_clk_use_counts[_num_clks]

/**
 * @brief Initializer for the struct pbl_clk_controller of controller @p sym.
 *
 * @param sym Symbol of the controller instance, with its state defined by
 * PBL_CLK_CONTROLLER_STATE_DEFINE().
 * @param _name Name.
 * @param _init Device init function, or NULL.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 */
#define PBL_CLK_CONTROLLER_INIT(sym, _name, _init, _parent, _ops) \
  {                                                               \
    .dev = PBL_DEVICE_INIT(sym, _name, _init, _parent, nullptr),  \
    .ops = (_ops),                                                \
    .use_counts = sym##_clk_use_counts,                           \
    .num_clks = sizeof(sym##_clk_use_counts),                     \
  }

/** @brief A clock, as used by a consumer. */
struct pbl_clk {
  /** Controller. */
  const struct pbl_clk_controller *ctrl;
  /** Clock number on the controller. */
  uint32_t id;
};

/**
 * @brief Initializer for a struct pbl_clk.
 *
 * @param _ctrl Clock controller.
 * @param _id Clock number, from the controller's binding header.
 */
#define PBL_CLK(_ctrl, _id) {.ctrl = (_ctrl), .id = (_id)}

/**
 * @brief Take a reference on a clock, ungating it for the first user.
 *
 * @param clk Clock.
 * @return 0 or a negative errno.
 */
int pbl_clk_enable(const struct pbl_clk *clk);

/**
 * @brief Drop a reference on a clock, gating it after the last user.
 *
 * Must balance a successful pbl_clk_enable().
 *
 * @param clk Clock.
 * @return 0 or a negative errno.
 */
int pbl_clk_disable(const struct pbl_clk *clk);

/**
 * @brief Check whether a clock has users.
 *
 * @param clk Clock.
 * @return True if the clock is enabled.
 */
bool pbl_clk_is_enabled(const struct pbl_clk *clk);

/** @} */
