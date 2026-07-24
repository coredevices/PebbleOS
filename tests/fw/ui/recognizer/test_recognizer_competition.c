/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Manager-level competition between the pan, swipe and tap recognizers registered together on a
// single layer. Verifies that a vertical drag starts the (vertical) pan while the swipe and tap
// fail, and that a fast horizontal flick completes the swipe while the pan and tap fail.

#include "clar.h"

#include "applib/ui/layer.h"
#include "applib/ui/window.h"
#include "applib/ui/recognizer/recognizer.h"
#include "applib/ui/recognizer/recognizer_impl.h"
#include "applib/ui/recognizer/recognizer_list.h"
#include "applib/ui/recognizer/recognizer_manager.h"
#include "applib/ui/recognizer/recognizer_private.h"
#include "applib/ui/recognizer/pan.h"
#include "applib/ui/recognizer/swipe.h"
#include "applib/ui/recognizer/tap.h"

#include "pbl/drivers/rtc.h"

#include <stdint.h>

// Fakes
#include "fake_rtc.h"

// Stubs
#include "stubs_app_state.h"
#include "stubs_gbitmap.h"
#include "stubs_graphics.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_process_manager.h"
#include "stubs_ui_window.h"
#include "stubs_unobstructed_area.h"
#include "test_recognizer_impl.h"

static RecognizerList *s_app_list;
static Layer *s_active_layer;
static RecognizerManager *s_manager;

RecognizerList *app_state_get_recognizer_list(void) {
  return s_app_list;
}

RecognizerList *window_get_recognizer_list(Window *window) {
  if (!window) {
    return NULL;
  }
  return layer_get_recognizer_list(&window->layer);
}

RecognizerManager *window_get_recognizer_manager(Window *window) {
  return s_manager;
}

struct Layer *window_get_root_layer(const Window *window) {
  if (!window) {
    return NULL;
  }
  return &((Window *)window)->layer;
}

// The active layer is resolved by the manager on touchdown; short-circuit the geometry lookup.
Layer *layer_find_layer_containing_point(const Layer *node, const GPoint *point) {
  return s_active_layer;
}

// Recognizer identity, carried as the recognizer's user data so the shared event callback can tell
// which recognizer fired.
typedef enum RecId {
  RecId_Pan,
  RecId_Swipe,
  RecId_Tap,
  RecIdCount,
} RecId;

// Last event seen per recognizer, and the swipe direction captured at the instant the swipe
// completed. The manager resets recognizers once a gesture ends, so completion must be observed via
// the event callback (before the reset), not via post-liftoff state.
static RecognizerEvent s_last_event[RecIdCount];
static SwipeDirection s_completed_swipe_dir;

static void prv_sub_event_handler(const Recognizer *recognizer, RecognizerEvent event) {
  const RecId id = (RecId)(intptr_t)recognizer_get_user_data((Recognizer *)recognizer);
  s_last_event[id] = event;
  if ((id == RecId_Swipe) && (event == RecognizerEvent_Completed)) {
    s_completed_swipe_dir = swipe_recognizer_get_direction(recognizer);
  }
}

// setup and teardown
void test_recognizer_competition__initialize(void) {
  s_app_list = NULL;
  s_active_layer = NULL;
  s_manager = NULL;
  for (int i = 0; i < RecIdCount; i++) {
    s_last_event[i] = -1;
  }
  s_completed_swipe_dir = SwipeDirection_None;
  fake_rtc_init(0, 0);
}

void test_recognizer_competition__cleanup(void) {}

// Helpers
static void prv_dispatch(RecognizerManager *manager, TouchEventType type, int16_t x, int16_t y) {
  const TouchEvent e = {
    .type = type,
    .x = x,
    .y = y,
  };
  recognizer_manager_handle_touch_event(&e, manager);
}

static void prv_advance_ms(uint32_t ms) {
  fake_rtc_increment_ticks((RtcTicks)ms * RTC_TICKS_HZ / MS_PER_SECOND);
}

typedef struct CompetitionScene {
  Window window;
  RecognizerList app_list;
  RecognizerManager manager;
  Layer layer;
  Recognizer *pan;
  Recognizer *swipe;
  Recognizer *tap;
} CompetitionScene;

// Build a scene with a vertical pan, a horizontal swipe and a tap all registered on one layer.
static void prv_scene_init(CompetitionScene *scene) {
  *scene = (CompetitionScene){};
  layer_init(&scene->window.layer, &GRectZero);
  layer_init(&scene->layer, &GRectZero);
  layer_add_child(&scene->window.layer, &scene->layer);

  s_app_list = &scene->app_list;
  s_manager = &scene->manager;
  recognizer_manager_init(&scene->manager);
  scene->manager.window = &scene->window;
  scene->manager.global_list = &scene->app_list;
  scene->manager.active_layer = &scene->layer;
  s_active_layer = &scene->layer;

  scene->pan = pan_recognizer_create(prv_sub_event_handler, (void *)(intptr_t)RecId_Pan,
                                     PanAxis_Vertical);
  scene->swipe = swipe_recognizer_create(prv_sub_event_handler, (void *)(intptr_t)RecId_Swipe,
                                         SwipeDirection_Left | SwipeDirection_Right);
  scene->tap = tap_recognizer_create(prv_sub_event_handler, (void *)(intptr_t)RecId_Tap);
  cl_assert(scene->pan && scene->swipe && scene->tap);

  recognizer_add_to_list(scene->pan, &scene->layer.recognizer_list);
  recognizer_add_to_list(scene->swipe, &scene->layer.recognizer_list);
  recognizer_add_to_list(scene->tap, &scene->layer.recognizer_list);
}

static void prv_scene_deinit(CompetitionScene *scene) {
  recognizer_remove_from_list(scene->pan, &scene->layer.recognizer_list);
  recognizer_remove_from_list(scene->swipe, &scene->layer.recognizer_list);
  recognizer_remove_from_list(scene->tap, &scene->layer.recognizer_list);
  recognizer_destroy(scene->pan);
  recognizer_destroy(scene->swipe);
  recognizer_destroy(scene->tap);
}

// tests

// A slow, straight vertical drag: the vertical pan starts recognizing, and the manager fails the
// competing horizontal swipe and the tap.
void test_recognizer_competition__vertical_drag_starts_pan(void) {
  CompetitionScene scene;
  prv_scene_init(&scene);

  prv_dispatch(&scene.manager, TouchEvent_Touchdown, 50, 50);
  prv_advance_ms(30);
  prv_dispatch(&scene.manager, TouchEvent_PositionUpdate, 50, 55);   // 5px, nothing decides yet
  prv_advance_ms(30);
  prv_dispatch(&scene.manager, TouchEvent_PositionUpdate, 50, 70);   // 20px down: pan starts

  cl_assert_equal_i(recognizer_get_state(scene.pan), RecognizerState_Started);
  cl_assert_equal_i(recognizer_get_state(scene.swipe), RecognizerState_Failed);
  cl_assert_equal_i(recognizer_get_state(scene.tap), RecognizerState_Failed);

  prv_scene_deinit(&scene);
}

// A fast, straight horizontal flick: the swipe completes on liftoff, while the vertical pan and the
// tap both fail (the pan on the foreign axis, the tap on movement past its threshold).
void test_recognizer_competition__fast_horizontal_completes_swipe(void) {
  CompetitionScene scene;
  prv_scene_init(&scene);

  prv_dispatch(&scene.manager, TouchEvent_Touchdown, 20, 80);
  prv_advance_ms(20);
  prv_dispatch(&scene.manager, TouchEvent_PositionUpdate, 50, 80);
  prv_advance_ms(20);
  prv_dispatch(&scene.manager, TouchEvent_PositionUpdate, 90, 80);
  // Pan and tap have already failed on the horizontal movement; the swipe is still tracking.
  cl_assert_equal_i(recognizer_get_state(scene.pan), RecognizerState_Failed);
  cl_assert_equal_i(recognizer_get_state(scene.tap), RecognizerState_Failed);

  prv_advance_ms(20);
  prv_dispatch(&scene.manager, TouchEvent_Liftoff, 0, 0);

  // The swipe completed (observed via its event, since the manager resets recognizers afterwards)
  // and its direction was captured as Right; neither pan nor tap ever completed.
  cl_assert_equal_i(s_last_event[RecId_Swipe], RecognizerEvent_Completed);
  cl_assert_equal_i(s_completed_swipe_dir, SwipeDirection_Right);
  cl_assert(s_last_event[RecId_Pan] != RecognizerEvent_Completed);
  cl_assert(s_last_event[RecId_Tap] != RecognizerEvent_Completed);

  prv_scene_deinit(&scene);
}
