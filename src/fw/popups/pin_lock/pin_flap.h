/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdbool.h>

#include "applib/graphics/gcontext.h"
#include "applib/graphics/gtypes.h"
#include "applib/ui/layer.h"
#include "popups/pin_lock/unlock_window.h"  // PinEntry

typedef struct {
  const PinEntry *entry;   // caller owns
  const char *title;       // i18n key drawn bold above the panels
  bool mask_confirmed;     // confirmed positions show '*' when true
} PinFlapConfig;

typedef struct PinFlap {
  PinFlapConfig config;
} PinFlap;

void pin_flap_init(PinFlap *flap, const PinFlapConfig *config);

//! Draw the bold title + the row of split-flap panels into `bounds`.
//! Call from a Layer update_proc. Frees i18n strings owned by `flap`.
void pin_flap_draw(PinFlap *flap, GContext *ctx, GRect bounds);
