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
 * pbl_reboot() and pbl_power_off() run the handlers of the matching prepare mode, then the
 * handlers that restart or power off the system, highest priority first. Prepare handlers shut
 * down what must not be cut off midway, e.g. a radio link or a flash erase. Restart and power-off
 * handlers end the system; one that returns has declined or failed, and the next one runs. If none
 * ends the system, the kernel resets the SoC with pbl_soc_reset() when the SoC selects
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
  /** Data passed to @ref handler. */
  void *user_data;
  /** Mode the handler runs in. */
  enum pbl_sys_off_mode mode;
  /** Order among the handlers of @ref mode, highest first. */
  int priority;
};

/**
 * @brief Define a restart or power-off handler.
 *
 * @param _name Name of the handler, unique across the firmware.
 * @param _mode Mode the handler runs in, an @ref pbl_sys_off_mode.
 * @param _priority Order among the handlers of @p _mode, highest first.
 * @param _handler Function called in @p _mode.
 * @param _user_data Data passed to @p _handler.
 */
#ifdef PBL_NO_LINKER_SCRIPT
#define PBL_SYS_OFF_HANDLER_DEFINE(_name, _mode, _priority, _handler, _user_data) \
  static const struct pbl_sys_off_handler pbl_sys_off_handler_##_name PBL_USED    \
  PBL_UNSORTED_SECTION(pbl_sys_off) = {                                           \
    .handler = (_handler),                                                        \
    .user_data = (_user_data),                                                    \
    .mode = (_mode),                                                              \
    .priority = (_priority),                                                      \
  }
#else
#define PBL_SYS_OFF_HANDLER_DEFINE(_name, _mode, _priority, _handler, _user_data)             \
  static const struct pbl_sys_off_handler pbl_sys_off_handler_##_name PBL_USED PBL_ALIGNED(4) \
      PBL_SECTION(".pbl_sys_off_handlers." #_name) = {                                        \
        .handler = (_handler),                                                                \
        .user_data = (_user_data),                                                            \
        .mode = (_mode),                                                                      \
        .priority = (_priority),                                                              \
  }
#endif

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
 * Resets the SoC if none of them powers off. Callable from any context.
 */
[[noreturn]] void pbl_power_off(void);

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

/** @} */
