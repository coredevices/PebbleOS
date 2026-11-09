/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/util/math.h>

#include <popups/mic_banner.h>
#include <popups/timeline/peek.h>

//! System overlays that reserve part of the app's screen through the unobstructed area service.
//! The app sees the union of all of them; a new overlay is added here.
typedef enum SystemOverlay {
  SystemOverlayNone = 0,
  SystemOverlayTimelinePeek,
  SystemOverlayMicBanner,
} SystemOverlay;

//! `overlay_y`, a framebuffer origin y for `overlay` (e.g. mid-slide), combined with the current
//! obstruction of every other overlay.
static inline int16_t system_obstruction_compose(SystemOverlay overlay, int16_t overlay_y) {
  int16_t y = overlay_y;
  if (overlay != SystemOverlayTimelinePeek) {
    y = MIN(y, timeline_peek_get_obstruction_origin_y());
  }
  if (overlay != SystemOverlayMicBanner) {
    y = MIN(y, mic_banner_get_obstruction_origin_y());
  }
  return y;
}

//! Framebuffer y where the system overlays start obstructing the app.
static inline int16_t system_obstruction_get_origin_y(void) {
  return system_obstruction_compose(SystemOverlayNone, INT16_MAX);
}
