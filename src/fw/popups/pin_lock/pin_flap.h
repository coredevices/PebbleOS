/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "applib/graphics/gcontext.h"
#include "applib/graphics/gtypes.h"
#include "applib/ui/animation.h"
#include "applib/ui/layer.h"
#include "popups/pin_lock/unlock_window.h"  // PinEntry

typedef struct {
  const PinEntry *entry;   // caller owns
  const char *title;       // i18n key drawn bold above the panels
  bool mask_confirmed;     // confirmed positions show '*' when true
  bool haptic;             // light vibe when a flap finishes rolling
} PinFlapConfig;

typedef struct PinFlap {
  PinFlapConfig config;
  // Animation state — valid only while animating is true.
  Animation *anim;
  Layer *layer;          // marked dirty each frame
  AnimationProgress progress;  // 0..ANIMATION_NORMALIZED_MAX, current roll
  bool animating;
  uint8_t from_digit;    // digit rolling out
  int8_t direction;      // +1 up (next), -1 down (prev)
} PinFlap;

void pin_flap_init(PinFlap *flap, const PinFlapConfig *config);

//! Draw the bold title + the row of split-flap panels into `bounds`.
//! Call from a Layer update_proc. Frees i18n strings owned by `flap`.
void pin_flap_draw(PinFlap *flap, GContext *ctx, GRect bounds);

//! Vertical pixel offset of the rolling digit for normalized animation
//! progress (0..ANIMATION_NORMALIZED_MAX) over a panel of height `panel_h`.
//! Pure — no firmware dependencies.
int16_t pin_flap_roll_offset(int32_t progress, int16_t panel_h);

//! Start a roll on the active panel toward the entry's current digit.
//! `from_digit` is the previous value; `direction` +1 (up) / -1 (down).
void pin_flap_animate_step(PinFlap *flap, Layer *layer,
                           uint8_t from_digit, int8_t direction);

//! Unschedule any running animation and reset animation state.
void pin_flap_reset(PinFlap *flap);
