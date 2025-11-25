/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! @file interval_timer.h
//!
//! Times the average number of milliseconds between samples. Taking a sample is ISR-safe. Uses
//! the RTC time domain which should be fairly accurate for real clock time. Note that because
//! we use the RTC we may occasionally have to discard intervals because the wall clock time was
//! changed. This is not ideal, but it's still a better source of time than our SysTick time source
//! as that is not necessarily synced to real time.
//!
//! The resolution of the recorded timer will be same as the configured resolution of our RTC
//! peripheral, which at the time of writing is 1/256 of a second.
//!
//! The average is calculated as an exponential moving average.
//! https://en.wikipedia.org/wiki/Moving_average#Exponential_moving_average
//! The weighting factor (how responsive to recent changes) is configurable.

typedef struct {
  uint64_t last_sample_timestamp_ms;

  // The minimum and maximum values for an interval for it to be included into the average.
  uint32_t min_expected_ms;
  uint32_t max_expected_ms;

  uint32_t weighting_factor_inverted;

  //! The moving average we've calculated based on the samples we have so far.
  uint32_t average_ms;
  //! The number of samples we've taken.
  uint32_t num_samples;
} IntervalTimer;

//! Initialize an interval timer.
//!
//! Allows the specification of an acceptable range of intervals. This is used to discard invalid
//! intervals.
//!
//! Intervals are averaged together in a moving average that includes the last n intervals. The
//! number of intervals to average over is configurable.
//!
//! @param min_expected_ms The minimum number of milliseconds between samples
//! @param min_expected_ms The maximum number of milliseconds between samples
//! @param weighting_factor_inverted
//!     1 / alpha. Specified as a inverted number to avoid dealing with floats. The higher the
//!     number the less responsive to recent changes our average is.
void interval_timer_init(IntervalTimer *timer, uint32_t min_expected_ms, uint32_t max_expected_ms,
                         uint32_t weighting_factory_inverted);

//! Record a sample that marks the start/end of an interval.
//! Safe to call from an ISR.
void interval_timer_take_sample(IntervalTimer *timer);

//! @param[out] average_ms_out The average ms for the interval.
//! @return The number of valid intervals that are in our moving average. Note that this value
//!         will never be larger than num_intervals_in_average.
uint32_t interval_timer_get(IntervalTimer *timer, uint32_t *average_ms_out);
