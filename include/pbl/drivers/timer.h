/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_timer System timer
 * @ingroup drivers
 * @brief Clock source and clock event devices the kernel keeps time with.
 *
 * A clock source is a free-running counter: the kernel reads it to know how much time passed,
 * extending it to 64 bits. A clock event device raises an interrupt after a given number of its
 * cycles, once: the kernel programs it for the next tick while threads run, and for the next
 * timeout when the CPU idles. Drivers know nothing about ticks.
 *
 * The system clock source and clock event device are @ref pbl_system_clocksource and
 * @ref pbl_system_clockevent, defined by the timer drivers of the SoC; one device may provide
 * both. The kernel initializes them when the scheduler starts, or earlier through
 * pbl_device_init(). Both must keep running in every idle state the SoC enters.
 *
 * @code{.c}
 * static uint32_t prv_read(const struct pbl_clocksource *cs) {
 *   return pbl_readl(FOO_CNT);
 * }
 *
 * static void prv_set_next(const struct pbl_clockevent *ce, uint32_t delta) {
 *   pbl_writel(pbl_readl(FOO_CNT) + delta, FOO_CMP);
 * }
 *
 * static void prv_isr(void) {
 *   pbl_writel(FOO_EVENT_CMP, FOO_EVENT_CLR);
 *   pbl_clockevent_handler(&s_foo.ce);
 * }
 * @endcode
 * @{
 */

struct pbl_clocksource;
struct pbl_clockevent;

/** @brief Clock source operations. */
struct pbl_clocksource_ops {
  /** Read the counter. Called with interrupts masked; must not block. */
  uint32_t (*read)(const struct pbl_clocksource *cs);
};

/** @brief Clock source: a free-running counter. */
struct pbl_clocksource {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_clocksource_ops *ops;
  /** Counter mask, 2^bits - 1. */
  uint32_t mask;
};

/** @brief Clock event device operations. */
struct pbl_clockevent_ops {
  /**
   * Raise the event after @p delta cycles, replacing any event pending. Called with interrupts
   * masked; @p delta is within the device limits.
   */
  void (*set_next)(const struct pbl_clockevent *ce, uint32_t delta);
  /** Cancel any event pending. Called with interrupts masked. */
  void (*shutdown)(const struct pbl_clockevent *ce);
};

/** @brief Clock event device: a one-shot timer interrupt. */
struct pbl_clockevent {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_clockevent_ops *ops;
  /** Shortest delay the device can generate, in its cycles. */
  uint32_t min_delta;
  /** Longest delay the device can generate, in its cycles. */
  uint32_t max_delta;
};

/**
 * @brief Clock source initializer.
 *
 * @param sym Symbol of the device instance, with its state defined by PBL_DEVICE_STATE_DEFINE().
 * @param _name Name.
 * @param _init Init function, which must set the rate with pbl_clocksource_set_hz() or
 *              pbl_clocksource_set_mhz().
 * @param _ops Driver operations.
 * @param _bits Counter width.
 */
#define PBL_CLOCKSOURCE_INIT(sym, _name, _init, _ops, _bits)                                   \
  {                                                                                            \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, nullptr, nullptr, &pbl_clocksource_class), \
    .ops = (_ops),                                                                             \
    .mask = (uint32_t)((1ULL << (_bits)) - 1U),                                                \
  }

/**
 * @brief Clock event device initializer.
 *
 * @param sym Symbol of the device instance, with its state defined by PBL_DEVICE_STATE_DEFINE().
 * @param _name Name.
 * @param _init Init function, which must set the rate with pbl_clockevent_set_hz().
 * @param _ops Driver operations.
 * @param _min Shortest delay, in cycles.
 * @param _max Longest delay, in cycles.
 */
#define PBL_CLOCKEVENT_INIT(sym, _name, _init, _ops, _min, _max)                              \
  {                                                                                           \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, nullptr, nullptr, &pbl_clockevent_class), \
    .ops = (_ops),                                                                            \
    .min_delta = (_min),                                                                      \
    .max_delta = (_max),                                                                      \
  }

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_clocksource_class;
extern const struct pbl_device_class pbl_clockevent_class;
/** @endcond */

/** @brief The system clock source, defined by the SoC's timer driver. */
extern const struct pbl_clocksource *const pbl_system_clocksource;

/** @brief The system clock event device, defined by the SoC's timer driver. */
extern const struct pbl_clockevent *const pbl_system_clockevent;

/**
 * @brief Set the rate of a clock source.
 *
 * Called by the driver from its init, and again whenever the rate changes, for instance after a
 * low-power oscillator calibration. Time stays continuous across changes.
 *
 * @param cs Clock source.
 * @param hz Counter rate.
 */
void pbl_clocksource_set_hz(const struct pbl_clocksource *cs, uint32_t hz);

/**
 * @brief Set the rate of a clock source, with millihertz resolution.
 *
 * As pbl_clocksource_set_hz(), for counters whose rate is not a whole number of hertz, such as a
 * calibrated RC oscillator.
 *
 * @param cs Clock source.
 * @param mhz Counter rate, in millihertz.
 */
void pbl_clocksource_set_mhz(const struct pbl_clocksource *cs, uint64_t mhz);

/**
 * @brief Set the rate of a clock event device.
 *
 * Called by the driver from its init, and again whenever the rate changes.
 *
 * @param ce Clock event device.
 * @param hz Rate of the cycles the device counts delays in.
 */
void pbl_clockevent_set_hz(const struct pbl_clockevent *ce, uint32_t hz);

/**
 * @brief Report a clock event.
 *
 * Called by the driver from its interrupt handler.
 *
 * @param ce Clock event device.
 */
void pbl_clockevent_handler(const struct pbl_clockevent *ce);

/** @} */
