/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "pbl/services/touch/touch_event.h"

#include <stdbool.h>
#include <stdint.h>

//! Touch-to-click synthesis bridge.
//!
//! Synthesizes button clicks from touch gestures so apps that use their own
//! click handlers (rather than ScrollLayer/MenuLayer) respond to touch:
//!   - a tap        -> SELECT (or UP/SELECT/DOWN over an action bar)
//!   - a vertical swipe -> UP / DOWN
//! Runs on KernelMain, active while a watchapp is the focused foreground
//! process or a focusing modal (notification popup, action menu, ...) is up,
//! and suppressed while the focused app consumes touch directly.

//! Icon-presence bits for ActionBarSynthDescriptor.icon_mask.
#define ACTION_BAR_SYNTH_ICON_UP     (1 << 0)
#define ACTION_BAR_SYNTH_ICON_SELECT (1 << 1)
#define ACTION_BAR_SYNTH_ICON_DOWN   (1 << 2)

//! Kernel-visible snapshot of the focused window's action bar, published by the
//! applib ActionBarLayer (via syscall) so the KernelMain bridge can route a tap
//! landing on the bar to UP/SELECT/DOWN by its vertical zone.
typedef struct ActionBarSynthDescriptor {
  bool present;       //!< an action bar is currently shown
  GRect frame;        //!< its rectangle in global screen coordinates
  uint8_t icon_mask;  //!< ACTION_BAR_SYNTH_ICON_* bits for the icons that exist
} ActionBarSynthDescriptor;

//! Initialize the bridge. Subscribes to app focus changes for foreground
//! gating. Call once from services init on KernelMain.
void touch_click_synth_init(void);

//! Feed a raw touch event to the bridge's tap detector. Called inline from the
//! kernel event loop's PEBBLE_TOUCH_EVENT handling.
void touch_click_synth_handle_touch(const TouchEvent *event);

//! Publish (or clear) the focused window's action bar descriptor. Called on
//! KernelMain from the sys_touch_click_synth_set_action_bar syscall handler.
void touch_click_synth_set_action_bar(const ActionBarSynthDescriptor *descriptor);
