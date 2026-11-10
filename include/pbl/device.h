/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
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
 * PBL_DEVICE_STATE_DEFINE(), PBL_DEVICE_INIT() and PBL_DEVICE_REGISTER(). The kernel brings
 * every registered device up before main(), dependencies first, whatever their order in the
 * table: a device's parent and deps are initialized before it, as are the devices its devicetree
 * node refers to, listed by PBL_DEVICE_DEPENDS(). A driver's init calls pbl_device_init() on
 * any other device it uses.
 *
 * An init returning @c -ENODEV means the device is not present: that is not logged as an error.
 *
 * A device class (GPIO port, clock controller, ...) defines a struct pbl_device_class and tags
 * its devices with it through its init macro, so tooling can list them or find one by name with
 * PBL_DEVICE_CLASS_FOREACH() and pbl_device_find().
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

/** @brief A device class. */
struct pbl_device_class {
  /** Name. */
  const char *name;
};

/**
 * @brief Define a device class.
 *
 * @param sym Symbol of the class.
 * @param _name Name.
 */
#define PBL_DEVICE_CLASS_DEFINE(sym, _name) const struct pbl_device_class sym = {.name = (_name)}

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
  /** Optional. Class of the device. */
  const struct pbl_device_class *cls;
  /** Runtime state. */
  struct pbl_device_state *state;
};

/**
 * @brief Memory region reserved for a device, what a devicetree @c memory-region entry points to
 * (a @c reserved-memory node).
 */
struct pbl_mem_region {
  /** Base address. */
  uintptr_t base;
  /** Size in bytes. */
  size_t size;
};

/**
 * @brief Initializer of a struct pbl_mem_region.
 *
 * @param _base Base address.
 * @param _size Size in bytes.
 */
#define PBL_MEM_REGION(_base, _size) {.base = (_base), .size = (_size)}

/** @brief Static dependency list for struct pbl_device::deps. */
#define PBL_DEVICE_DEPS(...) ((const struct pbl_device *const[]){__VA_ARGS__, nullptr})

/**
 * @brief Define the runtime state of device instance @p sym.
 *
 * @param sym Symbol of the device instance.
 */
#define PBL_DEVICE_STATE_DEFINE(sym) static struct pbl_device_state sym##_device_state

/**
 * @brief Initializer for the struct pbl_device of device instance @p sym, in class @p _cls.
 *
 * For the init macros of device classes.
 *
 * @param sym Symbol of the device instance, with its state defined by PBL_DEVICE_STATE_DEFINE().
 * @param _name Name.
 * @param _init Init function, or NULL.
 * @param _parent Parent device, or NULL.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 * @param _cls Class.
 */
#define PBL_DEVICE_INIT_CLASS(sym, _name, _init, _parent, _deps, _cls) \
  {                                                                    \
    .name = (_name),                                                   \
    .init = (_init),                                                   \
    .parent = (_parent),                                               \
    .deps = (_deps),                                                   \
    .cls = (_cls),                                                     \
    .state = &sym##_device_state,                                      \
  }

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
  PBL_DEVICE_INIT_CLASS(sym, _name, _init, _parent, _deps, nullptr)

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

/** @brief Devices that device @c dev needs, on top of its parent and deps. */
struct pbl_device_depends {
  /** Device. */
  const struct pbl_device *dev;
  /** NULL-terminated. Devices initialized first. */
  const struct pbl_device *const *deps;
};

#ifdef PBL_NO_LINKER_SCRIPT
#define PBL_DEVICE_DEPENDS_SECTION PBL_UNSORTED_SECTION(pbl_dev_deps)
#else
#define PBL_DEVICE_DEPENDS_SECTION PBL_SECTION(".pbl_dev_deps")
#endif

/**
 * @brief List devices a device needs, without touching its definition.
 *
 * pbl_device_init() initializes them first, as struct pbl_device::deps. The list is constant
 * and takes no RAM. The instances of a devicetree get theirs from what their node refers to.
 *
 * @param sym Symbol of the device instance.
 * @param _dev Pointer to its struct pbl_device.
 * @param ... Pointers to the struct pbl_device of each device it needs.
 */
#define PBL_DEVICE_DEPENDS(sym, _dev, ...)                             \
  static const struct pbl_device_depends sym##_device_depends PBL_USED \
      PBL_DEVICE_DEPENDS_SECTION = {.dev = (_dev), .deps = PBL_DEVICE_DEPS(__VA_ARGS__)}

/**
 * @brief Give an instance a second name with the type of its class, for code that does not know
 * the driver. It is the same object, and usable in constant initializers.
 *
 * The name may only be passed to the API of its class: code that needs the type of the driver
 * uses the instance, or an accessor of the driver.
 *
 * @param sym Name to give.
 * @param inst Symbol of the instance.
 * @param type Type of the instance.
 * @param class_type Type of the class object, which has to be its first member.
 * @param member Name of the class object.
 */
#ifdef __APPLE__
// Mach-O has no alias attribute: a second global symbol at the instance does the same
#define PBL_DEVICE_ALIAS(sym, inst, type, class_type, member)               \
  static_assert(offsetof(type, member) == 0, #member " has to come first"); \
  __asm__(".globl _" #sym "\n.set _" #sym ", _" #inst);                     \
  extern const class_type sym
#else
#define PBL_DEVICE_ALIAS(sym, inst, type, class_type, member)               \
  static_assert(offsetof(type, member) == 0, #member " has to come first"); \
  extern const class_type sym __attribute__((alias(#inst)))
#endif

/**
 * @brief Start the definition of a registered instance, followed by its initializer.
 *
 * The instance is @c sym of the type of its class, for code that does not know the driver, and
 * <tt>sym ## _inst</tt> of its own type, for the driver.
 *
 * @param sym Symbol of the instance.
 * @param type Type of the instance.
 * @param class_type Type of the class object, which has to be its first member.
 * @param member Name of the class object, holding the struct pbl_device as @c dev.
 */
#define PBL_DEVICE_INSTANCE(sym, type, class_type, member)     \
  extern const type sym##_inst;                                \
  PBL_DEVICE_REGISTER(sym##_inst, &sym##_inst.member.dev);     \
  PBL_DEVICE_ALIAS(sym, sym##_inst, type, class_type, member); \
  const type sym##_inst =

/**
 * @brief Initialize a device, its parent and deps first, with those of PBL_DEVICE_DEPENDS().
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
 * Called by the kernel on the main thread, before main().
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

/**
 * @brief Get the next registered device of a class.
 *
 * @param cls Class.
 * @param prev Previous device, or NULL for the first one.
 * @return Device, or NULL after the last one.
 */
const struct pbl_device *pbl_device_class_next(const struct pbl_device_class *cls,
                                               const struct pbl_device *prev);

/**
 * @brief Iterate over the registered devices of a class.
 *
 * @param cls Class.
 * @param dev Loop variable, a const struct pbl_device pointer.
 */
#define PBL_DEVICE_CLASS_FOREACH(cls, dev)                              \
  for ((dev) = pbl_device_class_next((cls), nullptr); (dev) != nullptr; \
       (dev) = pbl_device_class_next((cls), (dev)))

/**
 * @brief Find a registered device of a class by name.
 *
 * @param cls Class.
 * @param name Name.
 * @return Device, or NULL if there is none.
 */
const struct pbl_device *pbl_device_find(const struct pbl_device_class *cls, const char *name);

/** @} */
