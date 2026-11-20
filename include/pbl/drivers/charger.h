/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_charger Charger
 * @ingroup drivers
 * @brief Battery charger class.
 *
 * A charger is a struct pbl_charger device: it reports whether external power is present, the
 * charge status and the battery voltage, current and temperature, and lets the battery be
 * charged or not. Every call into the driver runs with the device lock held, a recursive mutex.
 *
 * Drivers report external power changes with pbl_charger_report_online_changed(), from thread
 * context, to the listener set with pbl_charger_set_listener().
 *
 * @code{.c}
 * struct pbl_charger_measurements meas;
 *
 * if (pbl_charger_get_measurements(CHARGER, &meas) == 0) {
 *   // meas.voltage_mv, meas.current_ua, meas.temp_mc
 * }
 * @endcode
 * @{
 */

struct pbl_charger;

/** @brief Charge status. */
enum pbl_charger_status {
  /** Not charging, or not reported. */
  PBL_CHARGER_STATUS_UNKNOWN,
  /** Charge complete, battery full. */
  PBL_CHARGER_STATUS_FULL,
  /** Trickle charging. */
  PBL_CHARGER_STATUS_TRICKLE,
  /** Constant current charging. */
  PBL_CHARGER_STATUS_CONSTANT_CURRENT,
  /** Constant voltage charging. */
  PBL_CHARGER_STATUS_CONSTANT_VOLTAGE,
};

/** @brief Battery measurements. */
struct pbl_charger_measurements {
  /** Voltage in mV. */
  int32_t voltage_mv;
  /** Current in uA, positive while discharging. */
  int32_t current_ua;
  /** Temperature in millidegrees Celsius. */
  int32_t temp_mc;
};

/** @brief Charger listener. */
struct pbl_charger_listener {
  /** External power was connected or removed. Called with the device lock held. */
  void (*online_changed)(const struct pbl_charger *charger);
};

/** @brief Charger driver operations, called with the device lock held. */
struct pbl_charger_ops {
  /** Optional. Bring the hardware up. */
  int (*init)(const struct pbl_charger *charger);
  /** Check whether external power is present. */
  int (*is_online)(const struct pbl_charger *charger, bool *online);
  /** Get the charge status. */
  int (*get_status)(const struct pbl_charger *charger, enum pbl_charger_status *status);
  /** Measure the battery. */
  int (*get_measurements)(const struct pbl_charger *charger, struct pbl_charger_measurements *meas);
  /** Allow charging or not. */
  int (*enable)(const struct pbl_charger *charger, bool enable);
  /** Optional. Let the battery supply a high current or not. */
  int (*set_high_load)(const struct pbl_charger *charger, bool high_load);
};

/** @brief Charger runtime state. */
struct pbl_charger_state {
  /** Device lock. */
  struct pbl_mutex lock;
  /** Listener, or NULL. */
  const struct pbl_charger_listener *listener;
  /** Number of high load requests held. */
  uint8_t high_load_count;
};

/** @brief Charger. */
struct pbl_charger {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_charger_ops *ops;
  /** Runtime state. */
  struct pbl_charger_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_charger_class;
int pbl_charger_dev_init(const struct pbl_device *dev);
/** @endcond */

/** @brief The board's charger. */
extern const struct pbl_charger *const CHARGER;

/**
 * @brief Define the class state of a charger.
 *
 * @param sym Symbol of the instance.
 */
#define PBL_CHARGER_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);       \
  static struct pbl_charger_state sym##_charger_state = {.lock = PBL_MUTEX_INITIALIZER}

/**
 * @brief Initialize a struct pbl_charger.
 *
 * @param sym Symbol of the instance, as passed to PBL_CHARGER_STATE_DEFINE().
 * @param _name Device name.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 */
#define PBL_CHARGER_INIT(sym, _name, _parent, _ops)                                              \
  {                                                                                              \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_charger_dev_init, _parent, &pbl_charger_class), \
    .ops = (_ops),                                                                               \
    .state = &sym##_charger_state,                                                               \
  }

/**
 * @brief Check whether external power is present.
 *
 * @param charger Charger.
 * @param[out] online True if external power is present.
 * @return 0 or a negative errno.
 */
int pbl_charger_is_online(const struct pbl_charger *charger, bool *online);

/**
 * @brief Get the charge status.
 *
 * @param charger Charger.
 * @param[out] status Charge status.
 * @return 0 or a negative errno.
 */
int pbl_charger_get_status(const struct pbl_charger *charger, enum pbl_charger_status *status);

/**
 * @brief Check whether the battery is charging: trickle, constant current or constant voltage.
 *
 * @param charger Charger.
 * @param[out] charging True while charging.
 * @return 0 or a negative errno.
 */
int pbl_charger_is_charging(const struct pbl_charger *charger, bool *charging);

/**
 * @brief Measure the battery voltage, current and temperature.
 *
 * @param charger Charger.
 * @param[out] meas Measurements.
 * @return 0 or a negative errno.
 */
int pbl_charger_get_measurements(const struct pbl_charger *charger,
                                 struct pbl_charger_measurements *meas);

/**
 * @brief Allow charging or not.
 *
 * @param charger Charger.
 * @param enable True to allow charging.
 * @return 0 or a negative errno.
 */
int pbl_charger_enable(const struct pbl_charger *charger, bool enable);

/**
 * @brief Request a high discharge current, e.g. for the speaker.
 *
 * Requests nest: the battery may supply a high current until each one is released.
 *
 * @param charger Charger.
 * @return 0, @c -ENOTSUP if the charger has no such limit, or a negative errno.
 */
int pbl_charger_high_load_request(const struct pbl_charger *charger);

/**
 * @brief Release a request made with pbl_charger_high_load_request().
 *
 * @param charger Charger.
 * @return 0, @c -EALREADY if no request is held, or a negative errno.
 */
int pbl_charger_high_load_release(const struct pbl_charger *charger);

/**
 * @brief Set the listener of a charger.
 *
 * @param charger Charger.
 * @param listener Listener, or NULL for none.
 */
void pbl_charger_set_listener(const struct pbl_charger *charger,
                              const struct pbl_charger_listener *listener);

/**
 * @brief Report that external power was connected or removed, for drivers.
 *
 * From thread context.
 *
 * @param charger Charger.
 */
void pbl_charger_report_online_changed(const struct pbl_charger *charger);

/** @} */
