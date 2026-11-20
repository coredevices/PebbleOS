/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/kernel/timer.h>
#include <pbl/util/freq_track.h>

/**
 * @defgroup drivers_clk_cal Calibrated clocks
 * @ingroup drivers_clk
 * @brief Track the rate of an imprecise oscillator by measuring it against a reference.
 *
 * For clock providers whose rate is only known by measurement, such as an RC oscillator. The
 * provider counts a gate of @c gate_cycles oscillator cycles against a reference clock; this
 * module runs a gate every @c period_ms from a kernel timer, feeds the results to a
 * @ref util_freq_track tracker with the oscillator's noise model, and reports the new rate to the
 * clock's notifiers. Gates and notifiers run in the tick interrupt, so the CPU sleeps through
 * each gate. Until the first gate succeeds, the rate is the nominal one and gates are retried
 * every second.
 *
 * To characterize an oscillator, pbl_clk_cal_sample() takes raw gates of any length and
 * pbl_clk_cal_hold() fixes the published rate; the @c clk shell command exposes both.
 *
 * @code{.c}
 * PBL_CLK_CAL_DEFINE(s_cal, &s_cal_ops, &s_params, PBL_CLK(&s_ctrl, 0), 2000, 60000);
 *
 * static int prv_init(const struct pbl_device *dev) {
 *   pbl_clk_cal_start(&s_cal, 32768.0);
 *   return 0;
 * }
 *
 * static int prv_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t *mhz) {
 *   *mhz = pbl_clk_cal_get_rate(&s_cal);
 *   return 0;
 * }
 * @endcode
 * @{
 */

struct pbl_clk_cal;

/** @brief Gate operations, called from the tick interrupt or a thread, one gate at a time. */
struct pbl_clk_cal_ops {
  /**
   * Start a gate of @p cycles oscillator cycles. Returns 0, @c -EBUSY to be retried a second
   * later, or another negative errno to skip this period.
   */
  int (*start)(const struct pbl_clk_cal *cal, uint32_t cycles);
  /**
   * Read the gate started last, of @p cycles cycles, once its time has passed, and release the
   * hardware. Returns 0 with the measured frequency in Hz, @c -EINPROGRESS if the gate has not
   * ended yet, to be read again shortly, or another negative errno once it cannot end.
   */
  int (*finish)(const struct pbl_clk_cal *cal, uint32_t cycles, double *hz);
};

/** @brief Runtime state of a calibrated clock. */
struct pbl_clk_cal_state {
  /** @cond INTERNAL_HIDDEN */
  const struct pbl_clk_cal *cal;
  const struct pbl_clk_cal *next;
  struct pbl_freq_track ft;
  struct pbl_timer timer;
  uint64_t rate_mhz;
  uint64_t held_mhz;
  uint32_t accepted;
  uint32_t rejected;
  uint32_t failed;
  bool gate_running;
  bool paused;
  uint8_t retries;
  /** @endcond */
};

/** @brief A calibrated clock. */
struct pbl_clk_cal {
  /** Gate operations. */
  const struct pbl_clk_cal_ops *ops;
  /** Noise model of the oscillator. */
  const struct pbl_freq_track_params *params;
  /** Clock whose notifiers follow the rate. */
  struct pbl_clk clk;
  /** Gate length, in oscillator cycles. */
  uint32_t gate_cycles;
  /** Time between gates, in milliseconds. */
  uint32_t period_ms;
  /** Runtime state. */
  struct pbl_clk_cal_state *state;
};

/** @brief Calibration state, for diagnostics. */
struct pbl_clk_cal_status {
  /** Tracked frequency, in mHz. */
  uint64_t tracked_mhz;
  /** Published rate, in mHz: the tracked one unless held. */
  uint64_t rate_mhz;
  /** Standard deviation of the tracked frequency, in parts per billion. */
  uint32_t sigma_ppb;
  /** Gates used. */
  uint32_t accepted;
  /** Gates rejected as outliers. */
  uint32_t rejected;
  /** Gates skipped or failed. */
  uint32_t failed;
  /** Whether the rate is held. */
  bool held;
};

/**
 * @brief Define a calibrated clock and its state.
 *
 * @param sym Symbol of the calibrated clock.
 * @param _ops Gate operations.
 * @param _params Noise model, a struct pbl_freq_track_params.
 * @param _clk Clock whose notifiers follow the rate, from PBL_CLK().
 * @param _gate_cycles Gate length, in oscillator cycles.
 * @param _period_ms Time between gates, in milliseconds.
 */
#define PBL_CLK_CAL_DEFINE(sym, _ops, _params, _clk, _gate_cycles, _period_ms) \
  static struct pbl_clk_cal_state sym##_state;                                 \
  static const struct pbl_clk_cal sym = {                                      \
    .ops = (_ops),                                                             \
    .params = (_params),                                                       \
    .clk = _clk,                                                               \
    .gate_cycles = (_gate_cycles),                                             \
    .period_ms = (_period_ms),                                                 \
    .state = &sym##_state,                                                     \
  }

/**
 * @brief Start the periodic gates, the first one now.
 *
 * @param cal Calibrated clock.
 * @param nominal_hz Rate until the first gate succeeds.
 */
void pbl_clk_cal_start(const struct pbl_clk_cal *cal, double nominal_hz);

#ifdef CONFIG_DEVICE_NAMES
/**
 * @brief Find a calibrated clock by the name of its controller.
 *
 * @param name Controller device name.
 * @return Calibrated clock, or NULL if none is started under that name.
 */
const struct pbl_clk_cal *pbl_clk_cal_find(const char *name);
#endif

/**
 * @brief Take one raw gate, outside the periodic calibration, which waits meanwhile.
 *
 * Blocks for the gate; thread context only. The result does not reach the tracker.
 *
 * @param cal Calibrated clock.
 * @param cycles Gate length, in oscillator cycles.
 * @param[out] hz Measured frequency.
 * @return 0 or a negative errno.
 */
int pbl_clk_cal_sample(const struct pbl_clk_cal *cal, uint32_t cycles, double *hz);

/**
 * @brief Publish a fixed rate, or the tracked one again.
 *
 * The tracker keeps running. With a fixed rate, comparing the time kept by the clock against a
 * reference gives the true mean rate of the oscillator.
 *
 * @param cal Calibrated clock.
 * @param rate_mhz Rate to publish in mHz, or 0 to follow the tracker again.
 */
void pbl_clk_cal_hold(const struct pbl_clk_cal *cal, uint64_t rate_mhz);

/**
 * @brief Get the calibration state.
 *
 * @param cal Calibrated clock.
 * @param[out] status State.
 */
void pbl_clk_cal_get_status(const struct pbl_clk_cal *cal, struct pbl_clk_cal_status *status);

/**
 * @brief Get the current rate.
 *
 * @param cal Calibrated clock.
 * @return Rate in mHz, 0 before any estimate.
 */
uint64_t pbl_clk_cal_get_rate(const struct pbl_clk_cal *cal);

/** @} */
