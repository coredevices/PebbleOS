/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/section.h>

/**
 * @defgroup kernel_reboot Reboot and power-off
 * @ingroup kernel
 * @brief Restart or power off the system, with handlers to shut down in order.
 *
 * pbl_reboot(), pbl_power_off() and pbl_power_cut() run the handlers of the matching prepare mode,
 * then the handlers that restart, power off or cut the power of the system, highest priority
 * first. Prepare handlers shut
 * down what must not be cut off midway, e.g. a radio link or a flash erase. Restart and power-off
 * handlers end the system; one that returns has declined or failed, and the next one runs. If none
 * ends the system, the kernel powers the SoC off with pbl_soc_power_off() when the SoC selects
 * @c CONFIG_SOC_HAS_POWER_OFF, or resets it with pbl_soc_reset() when it selects
 * @c CONFIG_SOC_HAS_RESET, else with the architecture's reset.
 *
 * Prepare handlers only run on the first request made from a thread with interrupts and the
 * scheduler unlocked; otherwise, or when a handler requests a reboot itself, they are skipped.
 * Restart and power-off handlers run in the context of the request, so they must check it before
 * doing anything that blocks.
 *
 * @code{.c}
 * static void prv_radio_off(enum pbl_sys_off_mode mode, void *user_data) {
 *   radio_disable();
 * }
 *
 * PBL_SYS_OFF_HANDLER_DEFINE(radio_restart, PBL_SYS_OFF_RESTART_PREPARE, PBL_SYS_OFF_PRIO_DEFAULT,
 *                            prv_radio_off, nullptr);
 * PBL_SYS_OFF_HANDLER_DEFINE(radio_power_off, PBL_SYS_OFF_POWER_OFF_PREPARE,
 *                            PBL_SYS_OFF_PRIO_DEFAULT, prv_radio_off, nullptr);
 * @endcode
 * @{
 */

/** @brief Stage of a restart or power-off a handler runs in. */
enum pbl_sys_off_mode {
  /** Before a restart, from a thread; may block. */
  PBL_SYS_OFF_RESTART_PREPARE,
  /** Before a power-off, from a thread; may block. */
  PBL_SYS_OFF_POWER_OFF_PREPARE,
  /** Restart the system. */
  PBL_SYS_OFF_RESTART,
  /** Power off the system. */
  PBL_SYS_OFF_POWER_OFF,
  /** Cut the power of the system. */
  PBL_SYS_OFF_POWER_CUT,
};

/** @brief Priority of handlers that must run after the others of their mode. */
#define PBL_SYS_OFF_PRIO_LOW -128
/** @brief Default handler priority. */
#define PBL_SYS_OFF_PRIO_DEFAULT 0
/** @brief Priority of handlers that must run before the others of their mode. */
#define PBL_SYS_OFF_PRIO_HIGH 128

/** @brief Restart or power-off handler. */
struct pbl_sys_off_handler {
  /** Called in @ref mode. */
  void (*handler)(enum pbl_sys_off_mode mode, void *user_data);
  /** For @ref PBL_SYS_OFF_POWER_CUT handlers, whether they can cut the power now, or NULL. */
  int (*check)(void *user_data);
  /** Data passed to @ref handler and @ref check. */
  void *user_data;
  /** Mode the handler runs in. */
  enum pbl_sys_off_mode mode;
  /** Order among the handlers of @ref mode, highest first. */
  int priority;
};

/** @cond INTERNAL_HIDDEN */
#define PBL_SYS_OFF_HANDLER_DEFINE_IMPL(_name, _mode, _priority, _check, _handler, _user_data) \
  static const struct pbl_sys_off_handler pbl_sys_off_handler_##_name PBL_ITERABLE_SECTION(    \
      const struct pbl_sys_off_handler, sys_off, #_name) = {                                   \
    .handler = (_handler),                                                                     \
    .check = (_check),                                                                         \
    .user_data = (_user_data),                                                                 \
    .mode = (_mode),                                                                           \
    .priority = (_priority),                                                                   \
  }
/** @endcond */

/**
 * @brief Define a restart or power-off handler.
 *
 * @param _name Name of the handler, unique across the firmware.
 * @param _mode Mode the handler runs in, an @ref pbl_sys_off_mode.
 * @param _priority Order among the handlers of @p _mode, highest first.
 * @param _handler Function called in @p _mode.
 * @param _user_data Data passed to @p _handler.
 */
#define PBL_SYS_OFF_HANDLER_DEFINE(_name, _mode, _priority, _handler, _user_data) \
  PBL_SYS_OFF_HANDLER_DEFINE_IMPL(_name, _mode, _priority, nullptr, _handler, _user_data)

/**
 * @brief Define a power-cut handler.
 *
 * @param _name Name of the handler, unique across the firmware.
 * @param _priority Order among the power-cut handlers, highest first.
 * @param _check Function telling whether the handler can cut the power now, called from the
 * thread requesting it before anything is torn down: 0, @c -ENOTSUP if the handler never cuts the
 * power, or another negative errno for why it can't now. NULL if it always can.
 * @param _handler Function called in @ref PBL_SYS_OFF_POWER_CUT.
 * @param _user_data Data passed to @p _check and @p _handler.
 */
#define PBL_SYS_OFF_POWER_CUT_HANDLER_DEFINE(_name, _priority, _check, _handler, _user_data) \
  PBL_SYS_OFF_HANDLER_DEFINE_IMPL(_name, PBL_SYS_OFF_POWER_CUT, _priority, _check, _handler, \
                                  _user_data)

/**
 * @brief Restart the system in order.
 *
 * Runs the @ref PBL_SYS_OFF_RESTART_PREPARE handlers, then the @ref PBL_SYS_OFF_RESTART ones.
 * Callable from any context.
 */
[[noreturn]] void pbl_reboot(void);

/**
 * @brief Power off the system in order.
 *
 * Runs the @ref PBL_SYS_OFF_POWER_OFF_PREPARE handlers, then the @ref PBL_SYS_OFF_POWER_OFF ones.
 * If none of them powers off, powers the SoC off, or resets it if the SoC cannot power off.
 * Callable from any context.
 */
[[noreturn]] void pbl_power_off(void);

/**
 * @brief Cut the power of the system in order.
 *
 * First asks the @ref PBL_SYS_OFF_POWER_CUT handlers whether they can cut the power now, and
 * returns if none can, with nothing torn down. Otherwise runs the
 * @ref PBL_SYS_OFF_POWER_OFF_PREPARE handlers, then the power-cut ones, which cut the power of the
 * whole system, e.g. a PMIC entering ship mode. Unlike a power-off, it may lose what the SoC keeps
 * when off, depending on what the board keeps powered, so it is only made on request. If none of
 * the handlers cuts the power after all, resets the SoC: it never falls back to a power-off.
 * Callable from a thread.
 *
 * @return Only on failure: @c -ENOTSUP if no handler cuts the power, or the error of the handler
 * that can't cut it now, e.g. @c -EBUSY.
 */
int pbl_power_cut(void);

/**
 * @brief Reset the SoC at once, without running any handler.
 *
 * For contexts where nothing else can be trusted to work. Callable from any context.
 */
[[noreturn]] void pbl_reboot_emergency(void);

/**
 * @brief Reset the SoC.
 *
 * Implemented by SoCs that select @c CONFIG_SOC_HAS_RESET, for those the architecture's reset
 * doesn't suit. Called with interrupts locked.
 */
[[noreturn]] void pbl_soc_reset(void);

/**
 * @brief Power off the SoC.
 *
 * Implemented by SoCs that select @c CONFIG_SOC_HAS_POWER_OFF. Only the wake sources drivers have
 * armed stay on; the system boots from reset when one fires. Called with interrupts locked.
 */
[[noreturn]] void pbl_soc_power_off(void);

/** @} */
