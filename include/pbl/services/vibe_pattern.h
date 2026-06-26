/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

//! Source that started a vibe pattern, so cancels can be scoped to their owner
//! (e.g. a notification dismiss must not stop an alarm vibe).
typedef enum VibePatternOwner {
  VibePatternOwner_Other = 0,   // default for untagged kernel vibes
  VibePatternOwner_App,         // any app/worker vibe; cleared on app cleanup
  VibePatternOwner_Notification,
  VibePatternOwner_PhoneCall,
  VibePatternOwner_Alarm,
} VibePatternOwner;

void vibes_init();

int32_t vibes_get_vibe_strength(void);
int32_t vibes_get_default_vibe_strength(void);
void vibes_set_default_vibe_strength(int32_t vibe_strength_default);

void vibe_service_set_enabled(bool enable);

//! Tag the next triggered pattern's owner (consumed when it starts). Kernel/modal
//! callers set this before vibing; app vibes default to VibePatternOwner_App.
void vibe_pattern_set_owner(VibePatternOwner owner);

//! Clear the active pattern only if it is owned by `owner`; no-op otherwise.
void vibe_pattern_clear_for_owner(VibePatternOwner owner);
