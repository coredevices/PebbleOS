/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/util/list.h>

#include <applib/ui/status_bar_layer.h>
#include <clar.h>
#include <resource/resource.h>
#include <resource/resource_ids.auto.h>
#include <stubs_system_theme.h>

// Fakes
////////////////////////////////////
#include <fake_fonts.h>

// Stubs
////////////////////////////////////

// Stubs
////////////////////////////////////
GContext *graphics_context_get_current_context(void) {
  return nullptr;
}

// Setup
////////////////////////////////////

ResourceCallbackHandle resource_watch(ResAppNum app_num, uint32_t resource_id,
                                      ResourceChangedCallback callback, void *data) {
  return (ResourceCallbackHandle){};
}

// Helpers
////////////////////////////////////

#define cl_assert_status_bar_height(status_bar)                           \
  do {                                                                    \
    cl_assert(status_bar.layer.frame.size.h == STATUS_BAR_LAYER_HEIGHT);  \
    cl_assert(status_bar.layer.bounds.size.h == STATUS_BAR_LAYER_HEIGHT); \
  } while (0);

// Tests
////////////////////////////////////

//! The height of the status bar should always be locked to STATUS_BAR_LAYER_HEIGHT.
//! Make sure that after marking dirty, it is always reset to STATUS_BAR_LAYER_HEIGHT.
void test_status_bar_layer__modify_height(void) {
  StatusBarLayer status_bar;
  status_bar_layer_init(&status_bar);

  cl_assert_status_bar_height(status_bar);

  GRect frame = status_bar.layer.frame;
  GRect bounds = status_bar.layer.bounds;

  frame.size.h = STATUS_BAR_LAYER_HEIGHT - 5;
  layer_set_frame(&status_bar.layer, &frame);
  cl_assert_status_bar_height(status_bar);

  bounds.size.h = STATUS_BAR_LAYER_HEIGHT + 5;
  layer_set_bounds(&status_bar.layer, &bounds);
  cl_assert_status_bar_height(status_bar);
}

//! Switching to the large-bold clock mode must grow the bar to the per-platform
//! large height, and that height must survive subsequent frame/bounds pokes
//! (the property-changed proc re-derives height from the mode).
void test_status_bar_layer__large_bold_height(void) {
  StatusBarLayer status_bar;
  status_bar_layer_init(&status_bar);
  cl_assert_status_bar_height(status_bar); // default height

  status_bar_layer_set_mode(&status_bar, StatusBarLayerModeClockLargeBold);
  cl_assert(status_bar.layer.frame.size.h == STATUS_BAR_LAYER_LARGE_BOLD_HEIGHT);
  cl_assert(status_bar.layer.bounds.size.h == STATUS_BAR_LAYER_LARGE_BOLD_HEIGHT);

  GRect frame = status_bar.layer.frame;
  frame.size.h = STATUS_BAR_LAYER_LARGE_BOLD_HEIGHT - 5;
  layer_set_frame(&status_bar.layer, &frame);
  cl_assert(status_bar.layer.frame.size.h == STATUS_BAR_LAYER_LARGE_BOLD_HEIGHT);
  cl_assert(status_bar.layer.bounds.size.h == STATUS_BAR_LAYER_LARGE_BOLD_HEIGHT);

  GRect bounds = status_bar.layer.bounds;
  bounds.size.h = STATUS_BAR_LAYER_LARGE_BOLD_HEIGHT + 5;
  layer_set_bounds(&status_bar.layer, &bounds);
  cl_assert(status_bar.layer.bounds.size.h == STATUS_BAR_LAYER_LARGE_BOLD_HEIGHT);

  status_bar_layer_set_mode(&status_bar, StatusBarLayerModeClock);
  cl_assert_status_bar_height(status_bar); // back to default
}
