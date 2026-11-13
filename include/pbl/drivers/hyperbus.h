/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_hyperbus HyperBus
 * @ingroup drivers
 * @brief HyperBus controllers.
 *
 * A controller drives one HyperBus memory, which it maps for reads and writes. The driver of
 * the memory, which knows its registers and what it needs of the bus, uses the controller
 * through this interface without knowing which one it is.
 *
 * Register addresses are HyperBus word addresses in the register space, and values are as the
 * memory documents them. Times are given as the memory specifies them, for the controller to
 * turn into what its clock makes of them.
 * @{
 */

struct pbl_hyperbus;

/** @brief What a HyperBus memory needs of the bus. */
struct pbl_hyperbus_config {
  /** Latency, in clocks. */
  uint8_t latency;
  /** Longest time CS may stay low, in nanoseconds. */
  uint32_t cs_low_max_ns;
  /** Shortest time CS stays high between accesses, in nanoseconds. */
  uint16_t cs_high_min_ns;
  /** Shortest time between a read and the next access, in nanoseconds. */
  uint16_t read_recovery_ns;
};

/** @brief Driver operations. */
struct pbl_hyperbus_ops {
  /** Ungate the controller. Returns 0 or a negative errno. */
  int (*enable)(const struct pbl_hyperbus *bus);
  /** Gate the controller. Callable with interrupts disabled. */
  void (*disable)(const struct pbl_hyperbus *bus);
  /** Get the bus clock rate, in Hz. Returns 0 or a negative errno. */
  int (*get_rate)(const struct pbl_hyperbus *bus, uint32_t *rate_hz);
  /**
   * Set the bus up for the memory, calibrating it if the controller needs to, and map the
   * memory. Returns its CPU address in @p base, or a negative errno.
   */
  int (*configure)(const struct pbl_hyperbus *bus, const struct pbl_hyperbus_config *cfg,
                   uintptr_t *base);
  /** Read a register. Callable with interrupts disabled. Returns 0 or a negative errno. */
  int (*read_reg)(const struct pbl_hyperbus *bus, uint32_t addr, uint16_t *val);
  /** Write a register. Callable with interrupts disabled. Returns 0 or a negative errno. */
  int (*write_reg)(const struct pbl_hyperbus *bus, uint32_t addr, uint16_t val);
  /**
   * Pulse CS, which wakes a sleeping memory up. Callable with interrupts disabled. Returns 0 or
   * a negative errno.
   */
  int (*wake)(const struct pbl_hyperbus *bus);
};

/** @brief HyperBus controller. */
struct pbl_hyperbus {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_hyperbus_ops *ops;
};

extern const struct pbl_device_class pbl_hyperbus_class;

/**
 * @brief Initializer for the struct pbl_hyperbus of controller @p sym.
 *
 * @param sym Symbol of the controller instance, with its state defined by
 * PBL_DEVICE_STATE_DEFINE().
 * @param _name Name.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 */
#define PBL_HYPERBUS_INIT(sym, _name, _parent, _ops)                                          \
  {                                                                                           \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, nullptr, _parent, nullptr, &pbl_hyperbus_class), \
    .ops = (_ops),                                                                            \
  }

/**
 * @brief Ungate the controller.
 *
 * @param bus Controller.
 * @return 0 or a negative errno.
 */
static inline int pbl_hyperbus_enable(const struct pbl_hyperbus *bus) {
  return bus->ops->enable(bus);
}

/**
 * @brief Gate the controller.
 *
 * Callable with interrupts disabled.
 *
 * @param bus Controller.
 */
static inline void pbl_hyperbus_disable(const struct pbl_hyperbus *bus) {
  bus->ops->disable(bus);
}

/**
 * @brief Get the bus clock rate.
 *
 * @param bus Controller.
 * @param[out] rate_hz Rate, in Hz.
 * @return 0 or a negative errno.
 */
static inline int pbl_hyperbus_get_rate(const struct pbl_hyperbus *bus, uint32_t *rate_hz) {
  return bus->ops->get_rate(bus, rate_hz);
}

/**
 * @brief Set the bus up for the memory and map the memory.
 *
 * @param bus Controller, enabled.
 * @param cfg What the memory needs of the bus.
 * @param[out] base CPU address of the memory.
 * @return 0 or a negative errno.
 */
static inline int pbl_hyperbus_configure(const struct pbl_hyperbus *bus,
                                         const struct pbl_hyperbus_config *cfg, uintptr_t *base) {
  return bus->ops->configure(bus, cfg, base);
}

/**
 * @brief Read a register of the memory.
 *
 * Callable with interrupts disabled.
 *
 * @param bus Controller, configured.
 * @param addr Word address in the register space.
 * @param[out] val Value.
 * @return 0 or a negative errno.
 */
static inline int pbl_hyperbus_read_reg(const struct pbl_hyperbus *bus, uint32_t addr,
                                        uint16_t *val) {
  return bus->ops->read_reg(bus, addr, val);
}

/**
 * @brief Write a register of the memory.
 *
 * Callable with interrupts disabled.
 *
 * @param bus Controller, enabled.
 * @param addr Word address in the register space.
 * @param val Value.
 * @return 0 or a negative errno.
 */
static inline int pbl_hyperbus_write_reg(const struct pbl_hyperbus *bus, uint32_t addr,
                                         uint16_t val) {
  return bus->ops->write_reg(bus, addr, val);
}

/**
 * @brief Pulse CS, which wakes a sleeping memory up.
 *
 * Callable with interrupts disabled.
 *
 * @param bus Controller, enabled.
 * @return 0 or a negative errno.
 */
static inline int pbl_hyperbus_wake(const struct pbl_hyperbus *bus) {
  return bus->ops->wake(bus);
}

/** @} */
