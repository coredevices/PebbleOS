/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_reset Reset
 * @ingroup drivers
 * @brief Reset controller class.
 *
 * A reset controller is a struct pbl_reset_controller device driving a set of reset lines, each
 * identified by a provider-specific number defined in the controller header.
 * A consumer holds a struct pbl_reset naming the controller and line, and lists the controller
 * among its device dependencies:
 *
 * @code{.c}
 * static const struct pbl_reset s_rst = PBL_RESET(SF32LB_HPSYS_RCC_RESET, SF32LB52_HPSYS_RST_TRNG);
 *
 * pbl_reset_reset(&s_rst);
 * @endcode
 * @{
 */

struct pbl_reset_controller;

/** @brief Reset controller driver operations, called with interrupts locked. */
struct pbl_reset_controller_ops {
  /** Put a line in reset. Returns 0 or a negative errno. */
  int (*assert)(const struct pbl_reset_controller *ctrl, uint32_t id);
  /** Take a line out of reset. Returns 0 or a negative errno. */
  int (*deassert)(const struct pbl_reset_controller *ctrl, uint32_t id);
};

/** @brief A reset controller. */
struct pbl_reset_controller {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_reset_controller_ops *ops;
  /** Number of reset lines. */
  uint16_t num_resets;
};

/**
 * @brief Define the class state of reset controller @p sym, for the driver's define macro.
 *
 * @param sym Symbol of the controller instance.
 */
#define PBL_RESET_CONTROLLER_STATE_DEFINE(sym) PBL_DEVICE_STATE_DEFINE(sym)

/**
 * @brief Initializer for the struct pbl_reset_controller of controller @p sym.
 *
 * @param sym Symbol of the controller instance, with its state defined by
 * PBL_RESET_CONTROLLER_STATE_DEFINE().
 * @param _name Name.
 * @param _init Device init function, or NULL.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 * @param _num_resets Number of reset lines.
 */
#define PBL_RESET_CONTROLLER_INIT(sym, _name, _init, _parent, _ops, _num_resets) \
  {                                                                              \
    .dev = PBL_DEVICE_INIT(sym, _name, _init, _parent, nullptr),                 \
    .ops = (_ops),                                                               \
    .num_resets = (_num_resets),                                                 \
  }

/** @brief A reset line, as used by a consumer. */
struct pbl_reset {
  /** Controller. */
  const struct pbl_reset_controller *ctrl;
  /** Line number on the controller. */
  uint32_t id;
};

/**
 * @brief Initializer for a struct pbl_reset.
 *
 * @param _ctrl Reset controller.
 * @param _id Line number, from the controller's binding header.
 */
#define PBL_RESET(_ctrl, _id) {.ctrl = (_ctrl), .id = (_id)}

/**
 * @brief Put a line in reset.
 *
 * @param rst Reset line.
 * @return 0 or a negative errno.
 */
int pbl_reset_assert(const struct pbl_reset *rst);

/**
 * @brief Take a line out of reset.
 *
 * @param rst Reset line.
 * @return 0 or a negative errno.
 */
int pbl_reset_deassert(const struct pbl_reset *rst);

/**
 * @brief Pulse a line: assert then deassert it.
 *
 * @param rst Reset line.
 * @return 0 or a negative errno.
 */
int pbl_reset_reset(const struct pbl_reset *rst);

/** @} */
