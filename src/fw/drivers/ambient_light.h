/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Light level enum
typedef enum AmbientLightLevel {
  AmbientLightLevelUnknown = 0,
  AmbientLightLevelVeryDark,
  AmbientLightLevelDark,
  AmbientLightLevelLight,
  AmbientLightLevelVeryLight,
} AmbientLightLevel;

#define AMBIENT_LIGHT_LEVEL_ENUM_COUNT (AmbientLightLevelVeryLight + 1)

#ifndef CONFIG_AMBIENT_LIGHT_BITS
// Fallback for header parsers (e.g. SDK generator) that don't preload autoconf.h.
#define CONFIG_AMBIENT_LIGHT_BITS 12
#endif

static const uint32_t AMBIENT_LIGHT_LEVEL_MAX = (1U << CONFIG_AMBIENT_LIGHT_BITS);

/** Initialize the ambient light sensor */
void ambient_light_init(void);

/** get the ambient light level scaled between 0 and AMBIENT_LIGHT_LEVEL_MAX
 */
uint32_t ambient_light_get_light_level(void);

//! Refcounted "I will want ALS readings soon" hint; bookkeeping lives in
//! ambient_light_common.c and reaches the driver via
//! ambient_light_driver_set_state().
void ambient_light_prime(void);
void ambient_light_release(void);

//! Refcounted "stop sampling right now" gate (e.g. backlight bleed-through).
//! Physical sampling is on iff prime > 0 && suspend == 0.
void ambient_light_suspend(void);
void ambient_light_resume(void);

//! Init the refcount framework. Called from the per-chip ambient_light_init().
void ambient_light_common_init(void);

//! Driver hook. `active` = prime > 0; `sampling` = active && suspend == 0.
//! No-op for drivers without a sampling gate.
void ambient_light_driver_set_state(bool active, bool sampling);

//! get the threshold between light and dark
uint32_t ambient_light_get_dark_threshold(void);

//! set the threshold between light and dark
void ambient_light_set_dark_threshold(uint32_t new_threshold);

//! figure out whether it is light outside
bool ambient_light_is_light();

//! Convert a light level obtained from ambient_light_get_light_level() into an
//! AmbientLightLevel enum value.
//! @param[in] light_level the raw light level reading obtained from ambient_light_get_light_level
AmbientLightLevel ambient_light_level_to_enum(uint32_t light_level);
