/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/touch/touch_event.h"

//! Touch-to-click synthesis bridge.
//!
//! Synthesizes button clicks from touch gestures so apps that use their own
//! click handlers (rather than ScrollLayer/MenuLayer) respond to touch:
//!   - a tap        -> SELECT
//!   - a vertical swipe -> UP / DOWN
//! Runs on KernelMain, active while a watchapp is the focused foreground
//! process or a focusing modal (notification popup, action menu, ...) is up,
//! and suppressed while the focused app consumes touch directly.

//! Initialize the bridge. Subscribes to app focus changes for foreground
//! gating. Call once from services init on KernelMain.
void touch_click_synth_init(void);

//! Feed a raw touch event to the bridge's tap detector. Called inline from the
//! kernel event loop's PEBBLE_TOUCH_EVENT handling.
void touch_click_synth_handle_touch(const TouchEvent *event);
