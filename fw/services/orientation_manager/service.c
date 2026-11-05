/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_ORIENTATION_MANAGER
#include <pbl/drivers/accel.h>
#include <pbl/drivers/display.h>
#include <pbl/drivers/mag.h>
#include <pbl/services/compositor/compositor_private.h>
#include <pbl/services/orientation_manager.h>

#include <kernel/events.h>
#include <kernel/input_buttons.h>
#include <process_management/process_manager.h>
#include <shell/prefs.h>
#ifdef CONFIG_SERVICE_TOUCH
#include <pbl/services/touch/touch.h>
#endif

void prv_change_orientation(bool rotated) {
  (void)pbl_display_set_rotated(DISPLAY, rotated);
  // The panel now shows the frame the other way up, so the next app frame must be sent whole
  compositor_app_framebuffer_mirror_invalidate();
  input_buttons_set_rotated(rotated);
  (void)pbl_accel_set_rotated(ACCEL, rotated);
#ifdef CONFIG_MAG
  (void)pbl_mag_set_rotated(MAG, rotated);
#endif
#ifdef CONFIG_SERVICE_TOUCH
  touch_set_rotated(rotated);
#endif
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