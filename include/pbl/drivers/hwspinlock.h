/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_hwspinlock Hardware spinlock
 * @ingroup drivers
 * @brief Hardware spinlock class.
 *
 * A hardware spinlock serializes processors that share registers or memory, where interrupt locks
 * only serialize the code of one. A hardware spinlock controller is a struct
 * pbl_hwspinlock_controller device holding a set of locks, each identified by a number. A
 * consumer holds a struct pbl_hwspinlock, from its devicetree `hwlocks`, and takes it around
 * the accesses it shares, in place of pbl_irq_lock(). Without one, PBL_HWSPINLOCK_NONE, taking it
 * only locks interrupts:
 *
 * @code{.c}
 * int ret = pbl_hwspinlock_lock(&ctrl->hwlock);
 * if (ret != 0) {
 *   return ret;
 * }
 * pbl_modifyl(mask, value, reg);
 * pbl_hwspinlock_unlock(&ctrl->hwlock);
 * @endcode
 *
 * Locks are usable before their controller is initialized, as for the pins of an early console.
 * @{
 */

struct pbl_hwspinlock_controller;

/** @brief Hardware spinlock controller driver operations, called with interrupts locked. */
struct pbl_hwspinlock_controller_ops {
  /** Try to take a lock: 0 when taken, -EBUSY when another processor holds it. */
  int (*trylock)(const struct pbl_hwspinlock_controller *ctrl, uint32_t id);
  /** Release a lock taken with trylock. */
  void (*unlock)(const struct pbl_hwspinlock_controller *ctrl, uint32_t id);
};

/** @brief A hardware spinlock controller. */
struct pbl_hwspinlock_controller {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_hwspinlock_controller_ops *ops;
  /** Number of locks. */
  uint16_t num_locks;
};

/**
 * @brief Define the class state of hardware spinlock controller @p sym, for the driver's define
 * macro.
 *
 * @param sym Symbol of the controller instance.
 */
#define PBL_HWSPINLOCK_CONTROLLER_STATE_DEFINE(sym) PBL_DEVICE_STATE_DEFINE(sym)

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_hwspinlock_controller_class;
/** @endcond */

/**
 * @brief Initializer for the struct pbl_hwspinlock_controller of controller @p sym.
 *
 * @param sym Symbol of the controller instance, with its state defined by
 * PBL_HWSPINLOCK_CONTROLLER_STATE_DEFINE().
 * @param _name Name.
 * @param _init Device init function, or NULL.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 * @param _num_locks Number of locks.
 */
#define PBL_HWSPINLOCK_CONTROLLER_INIT(sym, _name, _init, _parent, _ops, _num_locks)            \
  {                                                                                             \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, _parent, &pbl_hwspinlock_controller_class), \
    .ops = (_ops),                                                                              \
    .num_locks = (_num_locks),                                                                  \
  }

/** @brief A hardware spinlock, as used by a consumer. */
struct pbl_hwspinlock {
  /** Controller, NULL without a lock. */
  const struct pbl_hwspinlock_controller *ctrl;
  /** Lock number on the controller. */
  uint32_t id;
};

/**
 * @brief Initializer for a struct pbl_hwspinlock.
 *
 * @param _ctrl Hardware spinlock controller.
 * @param _id Lock number.
 */
#define PBL_HWSPINLOCK(_ctrl, _id) {.ctrl = (_ctrl), .id = (_id)}

/** @brief Initializer for a struct pbl_hwspinlock without a lock. */
#define PBL_HWSPINLOCK_NONE {.ctrl = nullptr, .id = 0U}

/**
 * @brief Lock interrupts, then take a lock, spinning while another processor holds it.
 *
 * Holders keep a lock for a few register accesses: a lock held for longer than
 * CONFIG_HWSPINLOCK_TIMEOUT_US belongs to a processor that stopped. On success, release it with
 * pbl_hwspinlock_unlock().
 *
 * @param lock Lock, or one without a lock, which only locks interrupts.
 *
 * @retval 0 On success, with interrupts locked.
 * @retval -EINVAL The lock is not one of its controller.
 * @retval -ETIMEDOUT Another processor held the lock for too long; interrupts are as they were.
 */
int pbl_hwspinlock_lock(const struct pbl_hwspinlock *lock);

/**
 * @brief Release a lock taken with pbl_hwspinlock_lock(), then unlock interrupts.
 *
 * @param lock Lock.
 */
void pbl_hwspinlock_unlock(const struct pbl_hwspinlock *lock);

/** @} */
