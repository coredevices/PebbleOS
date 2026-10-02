/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/**
 * @defgroup services_vibe_pattern Vibe patterns
 * @ingroup services
 * @brief Vibration motor sequencing and strength control.
 *
 * Plays queued patterns of (duration, strength) steps on the vibration motor. Strengths range
 * from -100 to 100, 0 being off. Each pattern has an owner, so cancellation can be scoped: a
 * notification dismiss must not stop an alarm vibe.
 *
 * Kernel code tags the next pattern before starting it, and later clears only its own:
 *
 * @code{.c}
 * vibe_pattern_set_owner(VibePatternOwner_Alarm);
 * vibes_long_pulse();
 * ...
 * vibe_pattern_clear_for_owner(VibePatternOwner_Alarm);
 * @endcode
 * @{
 */

/** @brief Source that started a vibe pattern. */
typedef enum VibePatternOwner {
  /** Untagged kernel vibes, the default. */
  VibePatternOwner_Other = 0,
  /** Vibes started by the app task; cleared on app cleanup. */
  VibePatternOwner_App,
  /** Notification vibes. */
  VibePatternOwner_Notification,
  /** Phone call vibes. */
  VibePatternOwner_PhoneCall,
  /** Alarm vibes. */
  VibePatternOwner_Alarm,
} VibePatternOwner;

/** @brief Initialize the service. */
void vibes_init();

/**
 * @brief Get the strength the motor is currently driven at.
 *
 * @return Strength, -100 to 100, 0 when off.
 */
int32_t vibes_get_vibe_strength(void);

/**
 * @brief Get the time since the motor was last active.
 *
 * Measured from when it was last turned on, or last turned off. Used to suppress false shake and
 * tap detections caused by vibration.
 *
 * @return Milliseconds, or @c UINT32_MAX if the motor has not run since boot.
 */
uint32_t vibes_get_time_since_last_vibe_ms(void);

/**
 * @brief Get the default strength, used by on/off pattern steps.
 *
 * @return Strength, -100 to 100.
 */
int32_t vibes_get_default_vibe_strength(void);

/**
 * @brief Set the default strength, from the vibration strength setting.
 *
 * @param vibe_strength_default Strength, -100 to 100.
 */
void vibes_set_default_vibe_strength(int32_t vibe_strength_default);

/**
 * @brief Runlevel hook.
 *
 * Turns the motor off before disabling. Patterns do not drive the motor while disabled.
 *
 * @param enable True to enable vibrations.
 */
void vibe_service_set_enabled(bool enable);

/**
 * @brief Set the owner of the next pattern.
 *
 * Consumed when the first step of the next pattern is queued. Without it, patterns from the app
 * task are owned by #VibePatternOwner_App and others by #VibePatternOwner_Other.
 *
 * @param owner Owner of the next pattern.
 */
void vibe_pattern_set_owner(VibePatternOwner owner);

/**
 * @brief Stop the current pattern if owned by @p owner.
 *
 * Does nothing otherwise.
 *
 * @param owner Owner whose pattern to stop.
 */
void vibe_pattern_clear_for_owner(VibePatternOwner owner);

/** @} */
