/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <time.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_rtc RTC
 * @ingroup drivers
 * @brief Real-time clock class: hardware that keeps the wall clock across resets.
 *
 * An RTC is a struct pbl_rtc device that counts UTC time in seconds on its own, through resets
 * and, with a backup supply, power loss. The kernel keeps the running wall clock
 * (pbl_realtime()); the clock service sets it from the board's RTC at boot and writes it back
 * when the time changes and periodically.
 *
 * @code{.c}
 * time_t t;
 *
 * if (pbl_rtc_get_time(RTC, &t) == 0) {
 *   pbl_realtime_set(t);
 * }
 * @endcode
 * @{
 */

struct pbl_rtc;

/** @brief RTC driver operations. */
struct pbl_rtc_ops {
  /** Read the time, UTC seconds since the epoch. */
  int (*get_time)(const struct pbl_rtc *rtc, time_t *t);
  /** Set the time, UTC seconds since the epoch. */
  int (*set_time)(const struct pbl_rtc *rtc, time_t t);
};

/** @brief RTC. */
struct pbl_rtc {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_rtc_ops *ops;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_rtc_class;
/** @endcond */

/** @brief The board's RTC. */
extern const struct pbl_rtc *const RTC;

/**
 * @brief Initialize a struct pbl_rtc.
 *
 * @param sym Symbol of the instance, with its state defined by PBL_DEVICE_STATE_DEFINE().
 * @param _name Device name.
 * @param _init Init function, or NULL.
 * @param _ops Driver operations.
 */
#define PBL_RTC_INIT(sym, _name, _init, _ops)                                          \
  {                                                                                    \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, nullptr, nullptr, &pbl_rtc_class), \
    .ops = (_ops),                                                                     \
  }

/**
 * @brief Read the time.
 *
 * @param rtc RTC.
 * @param[out] t UTC time in seconds since the epoch.
 * @return 0, -ENODEV if the RTC is not ready, or a negative errno.
 */
int pbl_rtc_get_time(const struct pbl_rtc *rtc, time_t *t);

/**
 * @brief Set the time.
 *
 * @param rtc RTC.
 * @param t UTC time in seconds since the epoch.
 * @return 0, -ENODEV if the RTC is not ready, -EINVAL if the RTC cannot hold @p t, or a negative
 *         errno.
 */
int pbl_rtc_set_time(const struct pbl_rtc *rtc, time_t t);

/** @} */
