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
 *
 * Clocks form a tree, across controllers: a clock may have a parent, as the gate of a peripheral
 * has the clock it gates, and its rate follows from the parent's. Enabling a clock enables its
 * parents first.
 *
 * A consumer that depends on a clock's rate reads it with pbl_clk_get_rate() and, if the rate
 * can change at run time, registers a struct pbl_clk_notifier to follow it.
 * @{
 */

struct pbl_clk_controller;
struct pbl_clk;

/** @brief Clock controller driver operations, called with interrupts locked. */
struct pbl_clk_controller_ops {
  /** Ungate a clock, or NULL if the clocks always run. Returns 0 or a negative errno. */
  int (*enable)(const struct pbl_clk_controller *ctrl, uint32_t id);
  /** Gate a clock, or NULL if the clocks always run. Returns 0 or a negative errno. */
  int (*disable)(const struct pbl_clk_controller *ctrl, uint32_t id);
  /**
   * Get a clock's rate in mHz from its parent's, 0 for a clock without one, or NULL if each
   * clock runs at its parent's rate. Returns 0 or a negative errno.
   */
  int (*get_rate)(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t parent_mhz,
                  uint64_t *rate_mhz);
  /**
   * Get a clock's parent, or NULL if the clocks have none. Returns 0, @c -ENOENT if the clock
   * has none, or another negative errno.
   */
  int (*get_parent)(const struct pbl_clk_controller *ctrl, uint32_t id, struct pbl_clk *parent);
  /**
   * Switch a clock to another parent, or NULL if no clock can. Returns 0, @c -EINVAL if it
   * cannot take that parent, or another negative errno.
   */
  int (*set_parent)(const struct pbl_clk_controller *ctrl, uint32_t id,
                    const struct pbl_clk *parent);
  /**
   * Set a clock's rate in mHz from its parent's, 0 for a clock without one, or NULL if no clock
   * can. Returns 0, @c -EINVAL if the clock cannot run at that rate exactly, or another negative
   * errno.
   */
  int (*set_rate)(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t rate_mhz,
                  uint64_t parent_mhz);
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

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_clk_controller_class;
/** @endcond */

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
#define PBL_CLK_CONTROLLER_INIT(sym, _name, _init, _parent, _ops)                                 \
  {                                                                                               \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, _parent, nullptr, &pbl_clk_controller_class), \
    .ops = (_ops),                                                                                \
    .use_counts = sym##_clk_use_counts,                                                           \
    .num_clks = sizeof(sym##_clk_use_counts),                                                     \
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

/** @brief Parent and rate a clock is set to when its controller comes up. */
struct pbl_clk_assigned {
  /** Clock. */
  struct pbl_clk clk;
  /** Parent, without a controller to keep the current one. */
  struct pbl_clk parent;
  /** Rate in Hz, or 0 to keep the current one. */
  uint32_t rate_hz;
};

/**
 * @brief Initializer for a struct pbl_clk_assigned.
 *
 * @param _clk Clock, a PBL_CLK() initializer.
 * @param _parent Parent, a PBL_CLK() initializer, or {} to keep the current one.
 * @param _rate_hz Rate in Hz, or 0 to keep the current one.
 */
#define PBL_CLK_ASSIGNED(_clk, _parent, _rate_hz) \
  {.clk = _clk, .parent = _parent, .rate_hz = (_rate_hz)}

/**
 * @brief Define the list of parents and rates a clock controller assigns, for the driver's define
 * macro.
 *
 * @param sym Symbol of the list.
 * @param ... PBL_CLK_ASSIGNED() initializers, from the @c assigned-clocks,
 * @c assigned-clock-parents and @c assigned-clock-rates properties of the controller's node.
 */
#define PBL_CLK_ASSIGNED_DEFINE(sym, ...) \
  static const struct pbl_clk_assigned sym[] = {__VA_ARGS__ __VA_OPT__(, ){}}

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

/**
 * @brief Get the rate of a clock.
 *
 * Callable from any context.
 *
 * @param clk Clock.
 * @param[out] rate_mhz Rate in mHz.
 * @return 0, @c -ENOTSUP if the rate is not known, or another negative errno.
 */
int pbl_clk_get_rate(const struct pbl_clk *clk, uint64_t *rate_mhz);

/**
 * @brief Get the parent of a clock.
 *
 * @param clk Clock.
 * @param[out] parent Parent.
 * @return 0, @c -ENOENT if the clock has no parent, or another negative errno.
 */
int pbl_clk_get_parent(const struct pbl_clk *clk, struct pbl_clk *parent);

/**
 * @brief Switch a clock to another parent.
 *
 * The references an enabled clock holds move to the new parent. The notifiers of the clock and
 * of the clocks below it are told the new rates.
 *
 * @param clk Clock.
 * @param parent New parent.
 * @return 0, @c -ENOTSUP if the clock cannot switch, @c -EINVAL if it cannot take that parent,
 * or another negative errno.
 */
int pbl_clk_set_parent(const struct pbl_clk *clk, const struct pbl_clk *parent);

/**
 * @brief Set the rate of a clock.
 *
 * The notifiers of the clock and of the clocks below it are told the new rates.
 *
 * @param clk Clock.
 * @param rate_mhz Rate in mHz.
 * @return 0, @c -ENOTSUP if the clock cannot change its rate, @c -EINVAL if it cannot run at that
 * rate exactly, or another negative errno.
 */
int pbl_clk_set_rate(const struct pbl_clk *clk, uint64_t rate_mhz);

/**
 * @brief Switch clocks to their assigned parents, then set their assigned rates, as Linux does,
 * for clock controller drivers.
 *
 * @param assigned List from PBL_CLK_ASSIGNED_DEFINE().
 * @return 0 or the first error of pbl_clk_set_parent() or pbl_clk_set_rate().
 */
int pbl_clk_assign(const struct pbl_clk_assigned *assigned);

struct pbl_clk_notifier;

/**
 * @brief Rate change callback.
 *
 * Called from the context the provider changed the rate in, possibly an interrupt: it must not
 * block.
 *
 * @param nb Notifier.
 * @param rate_mhz New rate in mHz.
 */
typedef void (*pbl_clk_notifier_fn_t)(struct pbl_clk_notifier *nb, uint64_t rate_mhz);

/** @brief A rate change subscription. */
struct pbl_clk_notifier {
  /** Clock. */
  struct pbl_clk clk;
  /** Callback. */
  pbl_clk_notifier_fn_t fn;
  /** @cond INTERNAL_HIDDEN */
  struct pbl_clk_notifier *next;
  /** @endcond */
};

/**
 * @brief Call a function whenever the rate of a clock changes.
 *
 * @param nb Notifier, with its clock and callback set; must stay valid.
 */
void pbl_clk_notifier_register(struct pbl_clk_notifier *nb);

/**
 * @brief Report a new rate to the notifiers of a clock and of the clocks below it, for clock
 * providers.
 *
 * @param clk Clock whose rate changed.
 */
void pbl_clk_rate_changed(const struct pbl_clk *clk);

/** @} */
