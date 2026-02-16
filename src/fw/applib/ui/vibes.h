/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! @file action_bar_layer.h
//! @addtogroup UI
//! @{
//!   @addtogroup Vibes
//! \brief Controlling the vibration motor
//!
//! The Vibes API provides calls that let you control Pebble’s vibration motor.
//!
//! The vibration motor can be used as a visceral mechanism for giving immediate feedback to the user.
//! You can use it to highlight important moments in games, or to draw the attention of the user.
//! However, you should use the vibration feature sparingly, because sustained use will rapidly deplete Pebble’s battery,
//! and vibrating Pebble too much and too often can become annoying for users.
//! @note When using these calls, if there is an ongoing vibration,
//! calling any of the functions to emit (another) vibration will have no effect.
//!   @{

/** Data structure describing a vibration pattern.
 A pattern consists of at least 1 vibe-on duration, optionally followed by
 alternating vibe-off + vibe-on durations. Each segment may have a different duration.

 Example code:
 \code{.c}
 // Vibe pattern: ON for 200ms, OFF for 100ms, ON for 400ms:
static const uint32_t const segments[] = { 200, 100, 400 };
VibePattern pat = {
  .durations = segments,
  .num_segments = ARRAY_LENGTH(segments),
};
vibes_enqueue_custom_pattern(pat);
\endcode
 @see vibes_enqueue_custom_pattern
 */
typedef struct {
  /**
   Pointer to an array of segment durations, measured in milli-seconds.
   The maximum allowed duration is 10000ms.
   */
  const uint32_t *durations;
  /**
   The length of the array of durations.
   */
  uint32_t num_segments;
} VibePattern;

/** Data structure describing a vibration pattern with per-segment amplitude control.
 Each segment has a duration and an amplitude. Amplitude 0 means no vibration
 (off), and 100 means maximum strength. Values above 100 are clamped.

 Example code:
 \code{.c}
 // Ramp-down pattern: 100% for 200ms, 50% for 200ms, 25% for 200ms:
 static const uint32_t const segments[] = { 200, 200, 200 };
 static const uint32_t const amplitudes[] = { 100, 50, 25 };
 VibePatternWithAmplitudes pat = {
   .durations = segments,
   .amplitudes = amplitudes,
   .num_segments = ARRAY_LENGTH(segments),
 };
 vibes_enqueue_custom_pattern_with_amplitudes(pat);
 \endcode
 @see vibes_enqueue_custom_pattern_with_amplitudes
 */
typedef struct {
  /**
   Pointer to an array of segment durations, measured in milli-seconds.
   The maximum allowed duration is 10000ms.
   */
  const uint32_t *durations;
  /**
   Pointer to an array of per-segment amplitudes (0-100).
   Must have the same length as the durations array.
   0 means no vibration; 100 means maximum strength.
   */
  const uint32_t *amplitudes;
  /**
   The length of the durations and amplitudes arrays.
   */
  uint32_t num_segments;
} VibePatternWithAmplitudes;

//! Cancel any in-flight vibe patterns; this is a no-op if there is no
//! on-going vibe.
void vibes_cancel(void);

//! Makes the watch emit one short vibration.
void vibes_short_pulse(void);

//! Makes the watch emit one long vibration.
void vibes_long_pulse(void);

//! Makes the watch emit two brief vibrations.
//!
void vibes_double_pulse(void);

//! Makes the watch emit a 'custom' vibration pattern.
//! @param pattern An arbitrary vibration pattern
//! @see VibePattern
void vibes_enqueue_custom_pattern(VibePattern pattern);

//! Makes the watch emit a 'custom' vibration pattern with per-segment amplitude control.
//! Unlike vibes_enqueue_custom_pattern, this function bypasses the system vibration
//! intensity preference and uses the provided amplitude values directly.
//! @param pattern An arbitrary vibration pattern with amplitudes
//! @see VibePatternWithAmplitudes
void vibes_enqueue_custom_pattern_with_amplitudes(VibePatternWithAmplitudes pattern);

//!   @} // end addtogroup Vibes
//! @} // end addtogroup UI
