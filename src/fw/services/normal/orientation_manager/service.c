/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if CAPABILITY_HAS_ORIENTATION_MANAGER
#include "pbl/services/normal/orientation_manager.h"
#include "system/passert.h"
#include "shell/prefs.h"
#include "drivers/display/display.h"
#include "drivers/accel.h"
#include "drivers/button.h"
#include "drivers/imu/mmc5603nj/mmc5603nj.h"
#include "kernel/events.h"
#include "process_management/process_manager.h"


void prv_change_orientation(bool rotated) {
  display_set_rotated(rotated);
  button_set_rotated(rotated);
  accel_set_rotated(rotated);
  mag_set_rotated(rotated);
}

void orientation_handle_prefs_changed(void) {
  prv_change_orientation(display_orientation_is_left());

  // Force the running app to re-render so the display reflects the new orientation immediately.
  // Without this, phone-originated orientation changes (via settings blob DB sync) would not
  // be visible until the next natural redraw (e.g. button press or watchface tick).
  PebbleEvent event = {
    .type = PEBBLE_RENDER_REQUEST_EVENT,
  };
  process_manager_send_event_to_process(PebbleTask_App, &event);
}

void orientation_manager_enable(bool on) {
  prv_change_orientation(on ? display_orientation_is_left() : false);
}
#endif