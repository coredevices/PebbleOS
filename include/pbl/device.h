/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/section.h>

/**
 * @defgroup drivers_device Device model
 * @ingroup drivers
 * @brief Common core of every hardware device: controllers, buses and the peripherals on them.
 *
 * A device class embeds struct pbl_device, a driver embeds the class struct, and container_of()
 * walks back out. Devices are const and live in flash; the only RAM is the state they point to.
 *
 * Instances are defined with the @c PBL_<DRIVER>_DEFINE() macro of their driver, built from
 * PBL_DEVICE_STATE_DEFINE(), PBL_DEVICE_INIT() and PBL_DEVICE_REGISTER(). pbl_device_init_all()
 * brings every registered device up, dependencies first: a device's parent and deps are
 * initialized before it, and a driver's init calls pbl_device_init() on the devices it uses.
 *
 * An init returning @c -ENODEV means the device is not present: that is not logged as an error.
 *
 * @code{.c}
 * struct foo {
 *   struct pbl_device dev;
 *   uint8_t addr;
 * };
 *
 * static int prv_foo_init(const struct pbl_device *dev) {
 *   const struct foo *foo = container_of(dev, const struct foo, dev);
 *   return prv_probe(foo->addr) ? 0 : -ENODEV;
 * }
 *
 * PBL_DEVICE_STATE_DEFINE(s_foo);
 * static const struct foo s_foo = {
 *   .dev = PBL_DEVICE_INIT(s_foo, "foo", prv_foo_init, &s_bus.dev, PBL_DEVICE_DEPS(&s_ldo.dev)),
 *   .addr = 0x18,
 * };
 * PBL_DEVICE_REGISTER(s_foo, &s_foo.dev);
 * @endcode
 * @{
 */

/** @brief Device status. */
enum pbl_device_status {
  /** Not initialized yet. */
  PBL_DEVICE_UNINIT,
  /** Initialization in progress. */
  PBL_DEVICE_INITIALIZING,
  /** Initialized. */
  PBL_DEVICE_READY,
  /** Initialization failed, or the device is not present. */
  PBL_DEVICE_FAILED,
};

/** @brief Runtime state of a device. */
struct pbl_device_state {
  /** One of @ref pbl_device_status. */
  uint8_t status;
  /** Result of the initialization. */
  int res;
};

/** @brief A hardware device. */
struct pbl_device {
  /** Name, for logs. */
  const char *name;
  /** Optional. Returns 0, @c -ENODEV if the device is not present, or another negative errno. */
  int (*init)(const struct pbl_device *dev);
  /** Optional. Bus or multi-function device the device sits on, initialized first. */
  const struct pbl_device *parent;
  /** Optional, NULL-terminated. Further devices initialized first. */
  const struct pbl_device *const *deps;
  /** Runtime state. */
  struct pbl_device_state *state;
};

/** @brief Static dependency list for struct pbl_device::deps. */
#define PBL_DEVICE_DEPS(...) ((const struct pbl_device *const[]){__VA_ARGS__, NULL})

/**
 * @brief Define the runtime state of device instance @p sym.
 *
 * @param sym Symbol of the device instance.
 */
#define PBL_DEVICE_STATE_DEFINE(sym) static struct pbl_device_state sym##_device_state

/**
 * @brief Initializer for the struct pbl_device of device instance @p sym.
 *
 * @param sym Symbol of the device instance, with its state defined by PBL_DEVICE_STATE_DEFINE().
 * @param _name Name.
 * @param _init Init function, or NULL.
 * @param _parent Parent device, or NULL.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_DEVICE_INIT(sym, _name, _init, _parent, _deps) \
  {                                                        \
    .name = (_name),                                       \
    .init = (_init),                                       \
    .parent = (_parent),                                   \
    .deps = (_deps),                                       \
    .state = &sym##_device_state,                          \
  }

#ifdef PBL_NO_LINKER_SCRIPT
#define PBL_DEVICE_TABLE_SECTION PBL_UNSORTED_SECTION(pbl_devices)
#else
#define PBL_DEVICE_TABLE_SECTION PBL_SECTION(".pbl_devices")
#endif

/**
 * @brief Add a device to the table pbl_device_init_all() walks.
 *
 * @param sym Symbol of the device instance.
 * @param dev Pointer to its struct pbl_device.
 */
#define PBL_DEVICE_REGISTER(sym, dev) \
  static const struct pbl_device *const sym##_device_entry PBL_USED PBL_DEVICE_TABLE_SECTION = (dev)

/**
 * @brief Initialize a device, its parent and deps first.
 *
 * A no-op once the device is ready or has failed. A dependency cycle asserts.
 *
 * @param dev Device.
 * @return 0, the error its init returned, or @c -ENODEV if its parent or a dep failed.
 */
int pbl_device_init(const struct pbl_device *dev);

/**
 * @brief Check whether a device is initialized.
 *
 * @param dev Device.
 * @return True if the device is ready.
 */
bool pbl_device_is_ready(const struct pbl_device *dev);

/**
 * @brief Initialize every registered device.
 *
 * @return Number of devices that failed or are not present.
 */
int pbl_device_init_all(void);

/**
 * @brief Initialize every registered device whose parent is @p parent.
 *
 * For a multi-function device to bring its functions up from its own init.
 *
 * @param parent Parent device.
 * @return Number of devices that failed or are not present.
 */
int pbl_device_init_children(const struct pbl_device *parent);

/** @} */
