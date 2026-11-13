/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_ram RAM
 * @ingroup drivers
 * @brief Memory-mapped RAM outside the SoC's own, only powered while it has users.
 *
 * A device maps its memory at an address its driver finds out at init, see
 * pbl_ram_get_region(). Its contents are only there between pbl_ram_get() and
 * the matching pbl_ram_put(): the first user powers the memory on, and once the last one is
 * gone, the next deep sleep powers it off. While it has users, deep sleep keeps its contents.
 * What goes where in the memory is up to its user, the kernel scratch heap (see
 * @ref kernel_scratch).
 * @{
 */

struct pbl_ram;

/** @brief Driver operations. */
struct pbl_ram_ops {
  /**
   * Bring the memory up, powered off, and set where it is mapped and its size in the state.
   * Returns 0 or a negative errno.
   */
  int (*init)(const struct pbl_ram *ram);
  /** Power the memory on. Returns 0 or a negative errno. */
  int (*power_on)(const struct pbl_ram *ram);
  /** Power the memory off, losing its contents. Runs with interrupts disabled. */
  void (*power_off)(const struct pbl_ram *ram);
  /**
   * Optional. Put the memory in a state that keeps its contents with less power. Runs in deep
   * sleep entry, with interrupts disabled.
   */
  void (*retain)(const struct pbl_ram *ram);
  /** With @c retain. Leave that state. Runs in deep sleep exit, with interrupts disabled. */
  void (*resume)(const struct pbl_ram *ram);
  /**
   * Optional. Wait until the memory has taken everything written to it. Runs before the CPU
   * stops its clocks, with interrupts disabled.
   */
  void (*quiesce)(const struct pbl_ram *ram);
};

/** @cond INTERNAL_HIDDEN */
struct pbl_ram_state {
  struct pbl_mutex lock;
  uintptr_t base;
  size_t size;
  uint32_t users;
  uint32_t power_cycles;
  volatile bool on;
  volatile bool busy;
  volatile bool retained;
};
/** @endcond */

/** @brief RAM device. */
struct pbl_ram {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_ram_ops *ops;
  /** Runtime state. */
  struct pbl_ram_state *state;
};

extern const struct pbl_device_class pbl_ram_class;
int pbl_ram_dev_init(const struct pbl_device *dev);

/** @brief External RAM, behind the kernel scratch heap. */
extern const struct pbl_ram *const EXTRAM;

#define PBL_RAM_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);   \
  static struct pbl_ram_state sym##_ram_state = {.lock = PBL_MUTEX_INITIALIZER}

#define PBL_RAM_INIT(sym, _name, _parent, _deps, _ops)                                          \
  {                                                                                             \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_ram_dev_init, _parent, _deps, &pbl_ram_class), \
    .ops = (_ops),                                                                              \
    .state = &sym##_ram_state,                                                                  \
  }

/**
 * @brief Get where the memory is mapped, and its size.
 *
 * @param ram Device.
 * @param[out] base CPU address of the memory.
 * @param[out] size Size of the memory, in bytes; 0 if the device is not ready.
 */
void pbl_ram_get_region(const struct pbl_ram *ram, uintptr_t *base, size_t *size);

/**
 * @brief Start using the memory, powering it on if it is off.
 *
 * Its contents are undefined if it had no user. Not callable from interrupts.
 *
 * @param ram Device.
 * @return 0, @c -ENODEV if the device is not ready, or the error powering it on.
 */
int pbl_ram_get(const struct pbl_ram *ram);

/**
 * @brief Stop using the memory.
 *
 * Without users, the next deep sleep powers it off.
 *
 * @param ram Device.
 */
void pbl_ram_put(const struct pbl_ram *ram);

/**
 * @brief Prepare the memory for deep sleep.
 *
 * Called by the SoC on deep sleep entry, with interrupts disabled: the memory is retained if it
 * has users, and powered off otherwise.
 *
 * @param ram Device.
 */
void pbl_ram_suspend(const struct pbl_ram *ram);

/**
 * @brief Bring the memory back after deep sleep.
 *
 * Called by the SoC on deep sleep exit, with interrupts disabled.
 *
 * @param ram Device.
 */
void pbl_ram_resume(const struct pbl_ram *ram);

/**
 * @brief Let the memory settle before the CPU stops its clocks.
 *
 * Called by the SoC before a clock-stopping idle state other than deep sleep, with interrupts
 * disabled.
 *
 * @param ram Device.
 */
void pbl_ram_quiesce(const struct pbl_ram *ram);

/** @} */
