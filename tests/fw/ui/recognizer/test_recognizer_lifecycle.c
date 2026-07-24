/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/ui/layer.h"
#include "applib/ui/window.h"
#include "applib/ui/window_manager.h"
#include "applib/ui/window_private.h"
#include "applib/ui/recognizer/recognizer.h"
#include "applib/ui/recognizer/recognizer_manager.h"
#include "applib/ui/recognizer/recognizer_private.h"

#include <string.h>

// Stubs
#include "stubs_app_install_manager.h"
#include "stubs_app_state.h"
#include "stubs_app_window_stack.h"
#include "stubs_click.h"
#include "stubs_gbitmap.h"
#include "stubs_graphics.h"
#include "stubs_graphics_context.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_modal_manager.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_process_manager.h"
#include "stubs_resources.h"
#include "stubs_status_bar_layer.h"
#include "stubs_syscalls.h"
#include "stubs_unobstructed_area.h"
#include "stubs_window_stack.h"

bool modal_manager_is_window_visible(Window *window) {
  return false;
}
bool modal_manager_is_window_focused(Window *window) {
  return false;
}
bool layer_is_status_bar_layer(Layer *layer) {
  return false;
}

GDrawState graphics_context_get_drawing_state(GContext *ctx) {
  GDrawState state;
  memset(&state, 0, sizeof(GDrawState));
  return state;
}

void graphics_context_set_drawing_state(GContext *ctx, GDrawState draw_state) {}

static RecognizerManager s_manager;
static RecognizerManager s_modal_manager;

RecognizerManager *app_state_get_recognizer_manager(void) {
  return &s_manager;
}

// The kernel (modal) recognizer manager twin. window_get_recognizer_manager routes non-app (modal)
// windows here.
RecognizerManager *modal_manager_get_recognizer_manager(void) {
  return &s_modal_manager;
}

void test_recognizer_lifecycle__initialize(void) {
  recognizer_manager_init(&s_manager);
}

void test_recognizer_lifecycle__cleanup(void) {}

// window_get_recognizer_manager routing
////////////////////////////////////////

void test_recognizer_lifecycle__get_manager_routing(void) {
  // NULL window -> NULL
  cl_assert_equal_p(window_get_recognizer_manager(NULL), NULL);

  Window window = {};
  layer_init(&window.layer, &GRectZero);

  // Window with no parent stack -> NULL
  window.parent_window_stack = NULL;
  cl_assert_equal_p(window_get_recognizer_manager(&window), NULL);

  // App window (parent stack is the app's window stack) -> the app manager
  window.parent_window_stack = app_state_get_window_stack();
  cl_assert_equal_p(window_get_recognizer_manager(&window), &s_manager);

  // Modal window (parent stack is not the app's window stack) -> the kernel (modal) manager, kept
  // separate from the app manager so a focused modal's gestures never reach app recognizers.
  WindowStack modal_stack = {};
  window.parent_window_stack = &modal_stack;
  cl_assert_equal_p(window_get_recognizer_manager(&window), &s_modal_manager);
}

// window_became_input_focus / window_lost_input_focus
////////////////////////////////////////

void test_recognizer_lifecycle__became_and_lost_focus(void) {
  Window window = {};
  layer_init(&window.layer, &GRectZero);
  window.parent_window_stack = app_state_get_window_stack();

  // Another app window, used to check the wrong-window guard
  Window other = {};
  layer_init(&other.layer, &GRectZero);
  other.parent_window_stack = app_state_get_window_stack();

  // Put the manager into a non-idle state pointing at a stale window
  s_manager.window = &other;
  s_manager.state = RecognizerManagerState_RecognizersActive;
  s_manager.active_layer = &window.layer;

  // Becoming input focus cancels/resets and repoints the manager at this window
  window_became_input_focus(&window);
  cl_assert_equal_p(s_manager.window, &window);
  cl_assert_equal_i(s_manager.state, RecognizerManagerState_WaitForTouchdown);
  cl_assert_equal_p(s_manager.active_layer, NULL);

  // Losing focus on a window that is not the current one is a no-op
  s_manager.window = &window;
  window_lost_input_focus(&other);
  cl_assert_equal_p(s_manager.window, &window);

  // Losing focus on the current window clears the window pointer
  window_lost_input_focus(&window);
  cl_assert_equal_p(s_manager.window, NULL);
}
