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
 *
 * An RTC with an alarm calls back at a given time, from its interrupt, and an armed alarm wakes the
 * system from power-off, which then boots as from any other wakeup:
 *
 * @code{.c}
 * ret = pbl_rtc_set_alarm(RTC, t + 60, prv_alarm, nullptr);
 * @endcode
 * @{
 */

struct pbl_rtc;

/**
 * @brief Alarm callback, from interrupt context.
 *
 * @param rtc RTC.
 * @param user_data Data given to pbl_rtc_set_alarm().
 */
typedef void (*pbl_rtc_alarm_callback_t)(const struct pbl_rtc *rtc, void *user_data);

/** @brief RTC driver operations. */
struct pbl_rtc_ops {
  /** Read the time, UTC seconds since the epoch. */
  int (*get_time)(const struct pbl_rtc *rtc, time_t *t);
  /** Set the time, UTC seconds since the epoch. */
  int (*set_time)(const struct pbl_rtc *rtc, time_t t);
  /** Optional. Arm the alarm, replacing an armed one. */
  int (*set_alarm)(const struct pbl_rtc *rtc, time_t t, pbl_rtc_alarm_callback_t cb,
                   void *user_data);
  /** Optional. Disarm the alarm. */
  int (*cancel_alarm)(const struct pbl_rtc *rtc);
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
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 */
#define PBL_RTC_INIT(sym, _name, _init, _parent, _ops)                        \
  {                                                                           \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, _parent, &pbl_rtc_class), \
    .ops = (_ops),                                                            \
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

/**
 * @brief Arm the alarm, replacing an armed one.
 *
 * The alarm fires once, calling @p cb, and wakes the system if it is powered off by then. Setting
 * the time leaves an armed alarm at the time it was armed for.
 *
 * @param rtc RTC.
 * @param t UTC time in seconds since the epoch, after the current time.
 * @param cb Callback, or @c nullptr to only wake the system.
 * @param user_data Data passed to @p cb.
 * @return 0, -ENODEV if the RTC is not ready, -ENOTSUP if it has no alarm, -EINVAL if @p t is
 *         not after the current time, -ERANGE if it is too far for the alarm, or a negative
 *         errno.
 */
int pbl_rtc_set_alarm(const struct pbl_rtc *rtc, time_t t, pbl_rtc_alarm_callback_t cb,
                      void *user_data);

/**
 * @brief Disarm the alarm.
 *
 * @param rtc RTC.
 * @return 0, -ENODEV if the RTC is not ready, -ENOTSUP if it has no alarm, or a negative errno.
 */
int pbl_rtc_cancel_alarm(const struct pbl_rtc *rtc);

/** @} */
