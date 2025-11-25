/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/ui/layer.h"
#include "applib/ui/window.h"
#include "applib/ui/recognizer/recognizer.h"
#include "applib/ui/recognizer/recognizer_impl.h"
#include "applib/ui/recognizer/recognizer_list.h"
#include "applib/ui/recognizer/recognizer_manager.h"
#include "applib/ui/recognizer/recognizer_private.h"
#include "util/size.h"


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
static TestImplData s_test_impl_data;

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

struct Layer* window_get_root_layer(const Window *window) {
  if (!window) {
    return NULL;
  }
  return &((Window *)window)->layer;
}

// Override find layer function so we don't have to muck around with points and layer bounds (also
// this process can change and this test will keep on working)
Layer *layer_find_layer_containing_point(const Layer *node, const GPoint *point) {
  return s_active_layer;
}

typedef struct RecognizerHandled {
  ListNode node;
  int idx;
} RecognizerHandled;

static ListNode *s_recognizers_handled;
static ListNode *s_recognizers_reset;

static bool prv_simultaneous_with_cb(const Recognizer *recognizer,
                                     const Recognizer *simultaneous_with) {
  return true;
}

static void prv_handle_touch_event (Recognizer *recognizer, const TouchEvent *touch_event) {

}

static bool prv_cancel(Recognizer *recognizer) {
  return false;
}

static void prv_reset (Recognizer *recognizer) {

}

static RecognizerImpl s_dummy_impl;

static void prv_clear_recognizers_processed(ListNode **list) {
  ListNode *node = *list;
  while (node) {
    ListNode *next = list_pop_head(node);
    free(node);
    node = next;
  }
  *list = NULL;
}

static void prv_compare_recognizers_processed(int indices[], uint32_t count, ListNode **list) {
  printf(list == &s_recognizers_handled ? "Handle touch: " : "");
  printf(list == &s_recognizers_reset ? "Reset: " : "");
  printf("{ ");
  for (uint32_t i = 0; i < count; i++) {
    printf("%d, ", indices[i]);
  }
  printf("}");

  ListNode *node = *list;
  int list_num = list_count(node);
  bool failed = list_num != count;
  if (failed) {
    count = list_num;
  }

  if (!failed) {
    for (uint32_t i = 0; (i < count) && !failed; i++) {
      failed = (indices[i] != ((RecognizerHandled *)node)->idx);
      node = list_get_next(node);
    }
  }
  if (failed) {
    node = *list;
    printf(" != { ");
    for (uint32_t i = 0; i < count; i++) {
      printf("%d, ", ((RecognizerHandled *)node)->idx);
      node = list_get_next(node);
    }
    printf("}");
  }
  printf("\n");
  cl_assert(!failed);

  prv_clear_recognizers_processed(list);
}

static void prv_sub_event_handler(const Recognizer *recognizer, RecognizerEvent event) {

}

// setup and teardown
void test_recognizer_manager__initialize(void) {
  s_test_impl_data = (TestImplData){};
  s_app_list = NULL;
  s_active_layer = NULL;
  s_manager = NULL;
  s_dummy_impl = (RecognizerImpl) {
    .handle_touch_event = prv_handle_touch_event,
    .cancel = prv_cancel,
    .reset = prv_reset,
  };
}

void test_recognizer_manager__cleanup(void) {
  prv_clear_recognizers_processed(&s_recognizers_handled);
  prv_clear_recognizers_processed(&s_recognizers_reset);
}

static void prv_store_recognizer_idx(Recognizer *recognizer, ListNode **list) {
  int *idx = recognizer_get_impl_data(recognizer, &s_dummy_impl);
  if (idx) {
    RecognizerHandled *rec = malloc(sizeof(RecognizerHandled));
    cl_assert(rec);
    *rec = (RecognizerHandled){ .idx = *idx };
    *list = list_get_head(list_append(*list, &rec->node));
  }
}

static bool prv_handle_dummy_touch_event(Recognizer *recognizer, void *unused) {
  prv_store_recognizer_idx(recognizer, &s_recognizers_handled);
  return true;
}

static Recognizer **prv_create_recognizers(int count) {
  Recognizer **recognizers = malloc(sizeof(Recognizer*) * count);
  cl_assert(recognizers);
  for (int i = 0; i < count; i++) {
    recognizers[i] = recognizer_create_with_data(&s_dummy_impl, &i,
                                                 sizeof(i), prv_sub_event_handler,
                                                 NULL);
    cl_assert(recognizers[i]);
  }
  return recognizers;
}

static void prv_destroy_recognizers(Recognizer **recognizers, int count) {
  for (int i = 0; i < count; i++) {
    recognizer_destroy(recognizers[i]);
  }
  free(recognizers);
}
// tests



bool prv_process_all_recognizers(RecognizerManager *manager,
                                 RecognizerListIteratorCb iter_cb, void *context);

void test_recognizer_manager__process_all_recognizers(void) {
  const int k_rec_count = 7;
  Recognizer **recognizers = prv_create_recognizers(k_rec_count);
  RecognizerManager manager;
  recognizer_manager_init(&manager);

  // ensure this runs without crashing even if there are no recognizer lists
  prv_process_all_recognizers(&manager, prv_handle_dummy_touch_event, NULL);

  RecognizerList app_list = {};
  s_app_list = &app_list;
  Window window = {};
  layer_init(&window.layer, &GRectZero);
  manager.window = &window;

  Layer layer_a, layer_b, layer_c;
  layer_init(&layer_a, &GRectZero);
  layer_init(&layer_b, &GRectZero);
  layer_init(&layer_c, &GRectZero);
  layer_add_child(&window.layer, &layer_a);
  layer_add_child(&layer_a, &layer_b);
  layer_add_child(&window.layer, &layer_c);
  manager.active_layer = &layer_c;

  // ensure that this runs without crashing even if all the lists are empty
  prv_process_all_recognizers(&manager, prv_handle_dummy_touch_event, NULL);

  // One recognizer attached to the active layer
  recognizer_add_to_list(recognizers[0], &layer_c.recognizer_list);
  cl_assert(prv_process_all_recognizers(&manager, prv_handle_dummy_touch_event, NULL));
  prv_compare_recognizers_processed((int[]) {0}, 1, &s_recognizers_handled);

  // Two recognizers attached to the active layer - processed in order that they were added
  recognizer_add_to_list(recognizers[1], &layer_c.recognizer_list);
  cl_assert(prv_process_all_recognizers(&manager, prv_handle_dummy_touch_event, NULL));
  prv_compare_recognizers_processed((int[]) {0, 1}, 2, &s_recognizers_handled);

  // Recognizers that attached to layers other than the active layer and its ancestors will not be
  // processed
  recognizer_add_to_list(recognizers[2], &layer_a.recognizer_list);
  recognizer_add_to_list(recognizers[3], &layer_a.recognizer_list);
  recognizer_add_to_list(recognizers[4], &layer_b.recognizer_list);
  cl_assert(prv_process_all_recognizers(&manager, prv_handle_dummy_touch_event, NULL));
  prv_compare_recognizers_processed((int[]) {0, 1}, 2, &s_recognizers_handled);

  // Recognizers attached to children of active layer will not be evaluated
  manager.active_layer = &layer_a;
  cl_assert(prv_process_all_recognizers(&manager, prv_handle_dummy_touch_event, NULL));
  prv_compare_recognizers_processed((int[]) {2, 3}, 2, &s_recognizers_handled);

  // Recognizers attached to active layer will be processed before those attached to their ancestors
  manager.active_layer = &layer_b;
  cl_assert(prv_process_all_recognizers(&manager, prv_handle_dummy_touch_event, NULL));
  prv_compare_recognizers_processed((int[]) {4, 2, 3}, 3, &s_recognizers_handled);

  // Recognizers attached to window processed before layer recognizers
  recognizer_add_to_list(recognizers[5], window_get_recognizer_list(&window));
  cl_assert(prv_process_all_recognizers(&manager, prv_handle_dummy_touch_event, NULL));
  prv_compare_recognizers_processed((int[]) {5, 4, 2, 3}, 4, &s_recognizers_handled);

  // Recognizers attached to app processed before window and layer recognizers
  recognizer_add_to_list(recognizers[6], &app_list);
  cl_assert(prv_process_all_recognizers(&manager, prv_handle_dummy_touch_event, NULL));
  prv_compare_recognizers_processed((int[]) {6, 5, 4, 2, 3}, 5, &s_recognizers_handled);

  prv_destroy_recognizers(recognizers, k_rec_count);
}

bool prv_dispatch_touch_event(Recognizer *recognizer, void *context);

void test_recognizer_manager__dispatch_touch_event(void) {
  bool handled = false;
  s_test_impl_data.handled = &handled;
  NEW_RECOGNIZER(r) = test_recognizer_create(&s_test_impl_data, NULL);

  // Copied from recognizer_manager.c
  TouchEvent t;
  struct ProcessTouchCtx {
    Recognizer *triggered;
    const TouchEvent *touch_event;
  } ctx = { .triggered = NULL, .touch_event = &t };

  cl_assert(prv_dispatch_touch_event(r, &ctx));
  cl_assert(handled);
  cl_assert(!ctx.triggered);

  handled = false;
  // Recognizer should not get a touch event when it is in inactive states
  r->state = RecognizerState_Failed;
  cl_assert(prv_dispatch_touch_event(r, &ctx));
  cl_assert(!handled);
  cl_assert(!ctx.triggered);

  r->state = RecognizerState_Cancelled;
  cl_assert(prv_dispatch_touch_event(r, &ctx));
  cl_assert(!handled);
  cl_assert(!ctx.triggered);

  r->state = RecognizerState_Completed;
  cl_assert(prv_dispatch_touch_event(r, &ctx));
  cl_assert(!handled);
  cl_assert(!ctx.triggered);

  r->state = RecognizerState_Started;
  cl_assert(prv_dispatch_touch_event(r, &ctx));
  cl_assert(handled);
  cl_assert_equal_p(ctx.triggered, r);
  ctx.triggered = NULL;
  handled = false;

  r->state = RecognizerState_Updated;
  cl_assert(prv_dispatch_touch_event(r, &ctx));
  cl_assert(handled);
  cl_assert_equal_p(ctx.triggered, r);
  handled = false;
  ctx.triggered = NULL;

  NEW_RECOGNIZER(s) = test_recognizer_create(&s_test_impl_data, NULL);
  s->state = RecognizerState_Started;
  r->state = RecognizerState_Possible;
  ctx.triggered = s;
  cl_assert(prv_dispatch_touch_event(r, &ctx));
  cl_assert(!handled);

  recognizer_set_simultaneous_with(r, prv_simultaneous_with_cb);
  cl_assert(prv_dispatch_touch_event(r, &ctx));
  cl_assert(handled);
  cl_assert_equal_p(ctx.triggered, s);
}


bool prv_fail_recognizer(Recognizer *recognizer, void *context);

void test_recognizer_manager__fail_recognizer(void) {
  NEW_RECOGNIZER(r1) = test_recognizer_create(&s_test_impl_data, NULL);
  NEW_RECOGNIZER(r2) = test_recognizer_create(&s_test_impl_data, NULL);
  r2->state = RecognizerState_Started;

  // copied from recognizer_manager.c
  struct FailRecognizerCtx {
    Recognizer *triggered;
    bool recognizers_active;
  } ctx = { .triggered = r2, .recognizers_active = false };

  cl_assert(prv_fail_recognizer(r2, &ctx));
  cl_assert_equal_i(r2->state, RecognizerState_Started);
  cl_assert(!ctx.recognizers_active);

  ctx.recognizers_active = false;
  r1->state = RecognizerState_Possible;
  cl_assert(prv_fail_recognizer(r1, &ctx));
  cl_assert_equal_i(r1->state, RecognizerState_Failed);
  cl_assert(!ctx.recognizers_active);

  // Make sure that we don't try to fail a recognizer twice (causing an assert)
  cl_assert(prv_fail_recognizer(r1, &ctx));
  cl_assert_equal_i(r1->state, RecognizerState_Failed);

  r1->state = RecognizerState_Possible;
  recognizer_set_simultaneous_with(r1, prv_simultaneous_with_cb);
  cl_assert(prv_fail_recognizer(r1, &ctx));
  cl_assert_equal_i(r1->state, RecognizerState_Possible);
  cl_assert(ctx.recognizers_active);

}


void prv_cancel_layer_tree_recognizers(RecognizerManager *manager, Layer *top_layer,
                                       Layer *bottom_layer);

static void prv_set_all_states(Recognizer **recognizers, int count, RecognizerState state) {
  for(int i = 0; i < count; i++) {
    recognizers[i]->state = state;
  }
}

void test_recognizer_manager__cancel_layer_tree_recognizers(void) {
  const int k_rec_count = 4;
  Recognizer **recognizers = prv_create_recognizers(k_rec_count);

  Window window = {};
  layer_init(&window.layer, &GRectZero);
  Layer *root = &window.layer;
  RecognizerManager manager;
  recognizer_manager_init(&manager);
  manager.window = &window;

  Layer layer_a, layer_b, layer_c;
  layer_init(&layer_a, &GRectZero);
  layer_init(&layer_b, &GRectZero);
  layer_init(&layer_c, &GRectZero);
  layer_add_child(root, &layer_a);
  layer_add_child(root, &layer_b);
  layer_add_child(&layer_a, &layer_c);

  recognizer_add_to_list(recognizers[0], window_get_recognizer_list(&window));
  recognizer_add_to_list(recognizers[1], &layer_a.recognizer_list);
  recognizer_add_to_list(recognizers[2], &layer_b.recognizer_list);
  recognizer_add_to_list(recognizers[3], &layer_c.recognizer_list);

  prv_set_all_states(recognizers, k_rec_count, RecognizerState_Started);

  // Layer C's recognizers reset when layer A becomes the new active layer
  manager.active_layer = &layer_c;
  prv_cancel_layer_tree_recognizers(&manager, &layer_a, &layer_c);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Started);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Started);
  cl_assert_equal_i(recognizers[2]->state, RecognizerState_Started);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Cancelled);

  // Layer C's and layer A's recognizers get reset when layer B becomes the new active layer
  prv_set_all_states(recognizers, k_rec_count, RecognizerState_Started);
  prv_cancel_layer_tree_recognizers(&manager, &layer_b, &layer_c);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Started);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Cancelled);
  cl_assert_equal_i(recognizers[2]->state, RecognizerState_Started);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Cancelled);

  // Layer C's and layer A's recognizers get cancelled when there is no new active layer
  prv_set_all_states(recognizers, k_rec_count, RecognizerState_Started);
  prv_cancel_layer_tree_recognizers(&manager, NULL, &layer_c);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Started);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Cancelled);
  cl_assert_equal_i(recognizers[2]->state, RecognizerState_Started);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Cancelled);

  // If recognizers are in the possible state, they will be failed, rather than cancelled
  prv_set_all_states(recognizers, k_rec_count, RecognizerState_Possible);
  prv_cancel_layer_tree_recognizers(&manager, NULL, &layer_c);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Failed);
  cl_assert_equal_i(recognizers[2]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Failed);

}

static RecognizerState s_next_state = RecognizerStateCount;
static int s_idx_to_change = -1;
static void prv_handle_touch_event_test(Recognizer *recognizer, const TouchEvent *touch_event) {
  int *idx = recognizer_get_impl_data(recognizer, &s_dummy_impl);
  prv_store_recognizer_idx(recognizer, &s_recognizers_handled);
  if ((s_idx_to_change >= 0) && (*idx == s_idx_to_change)) {
    recognizer_transition_state(recognizer, s_next_state);
    s_idx_to_change = -1;
    s_next_state = RecognizerStateCount;
  }
}

static void prv_reset_test(Recognizer *recognizer) {
  prv_store_recognizer_idx(recognizer, &s_recognizers_reset);
}

void test_recognizer_manager__handle_touch_event(void) {
  const int k_rec_count = 5;
  s_dummy_impl.handle_touch_event = prv_handle_touch_event_test;
  s_dummy_impl.reset = prv_reset_test;
  Recognizer **recognizers = prv_create_recognizers(k_rec_count);

  RecognizerList app_list = {};
  s_app_list = &app_list;

  Window window = {};
  layer_init(&window.layer, &GRectZero);
  Layer *root = &window.layer;
  RecognizerManager manager;
  recognizer_manager_init(&manager);
  manager.window = &window;

  Layer layer_a, layer_b, layer_c;
  layer_init(&layer_a, &GRectZero);
  layer_init(&layer_b, &GRectZero);
  layer_init(&layer_c, &GRectZero);
  layer_add_child(root, &layer_a);
  layer_add_child(root, &layer_b);
  layer_add_child(&layer_a, &layer_c);

  recognizer_add_to_list(recognizers[0], window_get_recognizer_list(&window));
  recognizer_add_to_list(recognizers[1], &layer_a.recognizer_list);
  recognizer_add_to_list(recognizers[2], &layer_b.recognizer_list);
  recognizer_add_to_list(recognizers[3], &layer_c.recognizer_list);
  recognizer_add_to_list(recognizers[4], s_app_list);

  s_active_layer = &layer_c;
  TouchEvent e = { .type = TouchEvent_PositionUpdate };

  // No active recognizers because manager is waiting for a touchdown event
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed(NULL, 0, &s_recognizers_handled);

  // Touchdown event occurs, active layer is found and all applicable recognizers receive events
  // while none have started recognizing
  e.type = TouchEvent_Touchdown;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {4, 0, 3, 1}, 4, &s_recognizers_handled);
  cl_assert_equal_p(manager.active_layer, &layer_c);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersActive);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Possible);

  // All recognizers receive events while none have started recognizing
  e.type = TouchEvent_PositionUpdate;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {4, 0, 3, 1}, 4, &s_recognizers_handled);
  cl_assert_equal_p(manager.active_layer, &layer_c);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersActive);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Possible);

  // Same as above. Different event type
  e.type = TouchEvent_Liftoff;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {4, 0, 3, 1}, 4, &s_recognizers_handled);
  cl_assert_equal_p(manager.active_layer, &layer_c);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersActive);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Possible);

  // Layer A recognizer's gesture starts to be recognized. All other recognizers failed
  e.type = TouchEvent_Touchdown;
  s_next_state = RecognizerState_Started;
  s_idx_to_change = 3;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {4, 0, 3}, 3, &s_recognizers_handled);
  cl_assert_equal_p(manager.active_layer, &layer_c);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersTriggered);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Failed);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Failed);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Started);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Failed);

  // Only layer A recognizer's gesture receives touch events
  e.type = TouchEvent_PositionUpdate;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {3}, 1, &s_recognizers_handled);
  cl_assert_equal_p(manager.active_layer, &layer_c);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersTriggered);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Failed);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Failed);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Started);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Failed);

  // Layer A recognizer's gesture updates. Only that recognizer receives touch events
  e.type = TouchEvent_Liftoff;
  s_next_state = RecognizerState_Updated;
  s_idx_to_change = 3;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {3}, 1, &s_recognizers_handled);
  cl_assert_equal_p(manager.active_layer, &layer_c);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersTriggered);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Updated);

  // Layer A recognizer's gesture completes and all recognizers are reset
  e.type = TouchEvent_Liftoff;
  s_next_state = RecognizerState_Completed;
  s_idx_to_change = 3;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {3}, 1, &s_recognizers_handled);
  prv_compare_recognizers_processed((int[]) {4, 0, 3, 1}, 4, &s_recognizers_reset);
  cl_assert_equal_i(manager.state, RecognizerManagerState_WaitForTouchdown);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[2]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Possible);

  // Layer A recognizer's gesture does not complete because there is no active layer until a
  // touchdown occurs
  e.type = TouchEvent_PositionUpdate;
  s_next_state = RecognizerState_Completed;
  s_idx_to_change = 3;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed(NULL, 0, &s_recognizers_handled);
  prv_compare_recognizers_processed(NULL, 0, &s_recognizers_reset);
  cl_assert_equal_i(manager.state, RecognizerManagerState_WaitForTouchdown);

  // Layer A's recognizer's gesture completes immediately. All recognizers receive the touch event
  // because Layer A's recognizers receive the touch events last. All recognizers in the chain are
  // reset
  e.type = TouchEvent_Touchdown;
  s_next_state = RecognizerState_Completed;
  s_idx_to_change = 1;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {4, 0, 3, 1}, 4, &s_recognizers_handled);
  prv_compare_recognizers_processed((int[]) {4, 0, 3, 1}, 4, &s_recognizers_reset);
  cl_assert_equal_i(manager.state, RecognizerManagerState_WaitForTouchdown);

  // The app's recognizer's gesture completes immediately. Only the app's recognizer sees the touch
  // events. All recognizers in the chain are reset
  e.type = TouchEvent_Touchdown;
  s_next_state = RecognizerState_Completed;
  s_idx_to_change = 4;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {4}, 1, &s_recognizers_handled);
  prv_compare_recognizers_processed((int[]) {4, 0, 3, 1}, 4, &s_recognizers_reset);
  cl_assert_equal_i(manager.state, RecognizerManagerState_WaitForTouchdown);

  // Layer C recognizer starts recognizing a gesture, failing other recognizers
  e.type = TouchEvent_Touchdown;
  s_next_state = RecognizerState_Started;
  s_idx_to_change = 1;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {4, 0, 3, 1}, 4, &s_recognizers_handled);
  prv_compare_recognizers_processed(NULL, 0, &s_recognizers_reset);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersTriggered);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Failed);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Started);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Failed);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Failed);

  // A second touchdown event occurs while recognizers are active. A different layer is touched, so
  // the active recognizers on non-touched layers in the tree are cancelled
  s_active_layer = &layer_b;
  e.type = TouchEvent_Touchdown;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {4, 0, 2}, 3, &s_recognizers_handled);
  prv_compare_recognizers_processed((int[]) {4, 0, 2}, 3, &s_recognizers_reset);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersActive);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Cancelled);
  cl_assert_equal_i(recognizers[2]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Failed);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Possible);

  // Window recognizer becomes triggered
  e.type = TouchEvent_PositionUpdate;
  s_next_state = RecognizerState_Started;
  s_idx_to_change = 0;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {4, 0 }, 2, &s_recognizers_handled);
  prv_compare_recognizers_processed(NULL, 0, &s_recognizers_reset);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersTriggered);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Started);
  cl_assert_equal_i(recognizers[2]->state, RecognizerState_Failed);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Failed);

  // Another layer in a separate branch becomes active while a window recognizer is triggered
  e.type = TouchEvent_Touchdown;
  s_active_layer = &layer_a;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) { 0 }, 1, &s_recognizers_handled);
  prv_compare_recognizers_processed(NULL, 0, &s_recognizers_reset);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersTriggered);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Started);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Cancelled); // was already cancelled
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Failed);

  // A child layer of the active layer becomes active when a window recognizer is triggered
  e.type = TouchEvent_Touchdown;
  s_active_layer = &layer_c;
  recognizers[3]->state = RecognizerState_Possible;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) { 0 }, 1, &s_recognizers_handled);
  prv_compare_recognizers_processed(NULL, 0, &s_recognizers_reset);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersTriggered);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Started);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Cancelled);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Failed);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Failed);

  // A touchdown occurs where no layers are touched while a window recognizer is active
  e.type = TouchEvent_Touchdown;
  s_active_layer = NULL;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) { 0 }, 1, &s_recognizers_handled);
  prv_compare_recognizers_processed(NULL, 0, &s_recognizers_reset);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersTriggered);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Started);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Cancelled);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Failed);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Failed);

  // Touchdown occurs, Window recognizer completes, active layer becomes non-null
  e.type = TouchEvent_Touchdown;
  s_next_state = RecognizerState_Completed;
  s_idx_to_change = 0;
  s_active_layer = &layer_a;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) { 0 }, 1, &s_recognizers_handled);
  prv_compare_recognizers_processed((int[]) { 4, 0, 1 }, 3, &s_recognizers_reset);
  cl_assert_equal_i(manager.state, RecognizerManagerState_WaitForTouchdown);
  cl_assert_equal_p(manager.triggered, NULL);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Possible);

  // A touchdown occurs where no layers are touched
  s_active_layer = NULL;
  e.type = TouchEvent_Touchdown;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {4, 0}, 2, &s_recognizers_handled);
  prv_compare_recognizers_processed(NULL, 0, &s_recognizers_reset);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersActive);
  cl_assert_equal_p(manager.triggered, NULL);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Possible);

  // A touchdown occurs and the active layer goes from non-null to null. All layer recognizers get
  // reset. All recognizers remain in the possible state.
  s_active_layer = &layer_a;
  e.type = TouchEvent_Touchdown;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {4, 0, 1}, 3, &s_recognizers_handled);
  prv_compare_recognizers_processed((int[]) {1}, 1, &s_recognizers_reset);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersActive);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Possible);

  // A touchdown occurs and a child of the previous active recognizer becomes the active layer. The
  // child is reset. All recognizers remain in the possible state.
  s_active_layer = &layer_c;
  e.type = TouchEvent_Touchdown;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {4, 0, 3, 1}, 4, &s_recognizers_handled);
  prv_compare_recognizers_processed((int[]) {3}, 1, &s_recognizers_reset);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersActive);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Possible);

  // A touchdown occurs and the parent of the previous active recognizer becomes the active layer.
  // No recognizers are reset and all recognizers remain in the possible state. The child is failed.
  s_active_layer = &layer_a;
  e.type = TouchEvent_Touchdown;
  recognizer_manager_handle_touch_event(&e, &manager);
  prv_compare_recognizers_processed((int[]) {4, 0, 1}, 3, &s_recognizers_handled);
  prv_compare_recognizers_processed(NULL, 0, &s_recognizers_reset);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersActive);
  cl_assert_equal_i(recognizers[0]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[1]->state, RecognizerState_Possible);
  cl_assert_equal_i(recognizers[3]->state, RecognizerState_Failed);
  cl_assert_equal_i(recognizers[4]->state, RecognizerState_Possible);

  prv_destroy_recognizers(recognizers, k_rec_count);
}

void test_recognizer_manager__deregister_recognizer(void) {
  NEW_RECOGNIZER(r1) = test_recognizer_create(&s_test_impl_data, NULL);
  NEW_RECOGNIZER(r2) = test_recognizer_create(&s_test_impl_data, NULL);

  Window window = {};
  layer_init(&window.layer, &GRectZero);
  Layer *root = &window.layer;
  RecognizerManager manager;
  recognizer_manager_init(&manager);

  Layer layer_a;
  layer_init(&layer_a, &GRectZero);
  layer_add_child(root, &layer_a);

  manager.window = &window;
  manager.active_layer = &layer_a;

  recognizer_add_to_list(r1, &layer_a.recognizer_list);
  recognizer_add_to_list(r2, &layer_a.recognizer_list);

  RecognizerManager manager2;
  recognizer_set_manager(r1, &manager2);

  recognizer_manager_deregister_recognizer(&manager, r1);
  cl_assert_equal_p(manager.active_layer,  &layer_a);
  cl_assert_equal_p(recognizer_get_manager(r1), &manager2);

  recognizer_set_manager(r1, &manager);

  recognizer_manager_deregister_recognizer(&manager, r1);
  cl_assert(!recognizer_get_manager(r1));
  cl_assert_equal_p(manager.active_layer, &layer_a);

  recognizer_set_manager(r1, &manager);
  r1->state = RecognizerState_Started;
  r2->state = RecognizerState_Failed;
  manager.triggered = r1;
  manager.state = RecognizerManagerState_RecognizersTriggered;
  recognizer_manager_deregister_recognizer(&manager, r1);
  cl_assert(!manager.triggered);
  cl_assert(!manager.active_layer);
  cl_assert_equal_i(manager.state, RecognizerManagerState_WaitForTouchdown);
  cl_assert_equal_i(r2->state, RecognizerState_Possible);
  cl_assert(!recognizer_get_manager(r1));

  recognizer_set_manager(r1, &manager);
  r1->state = RecognizerState_Possible;
  r2->state = RecognizerState_Started;
  manager.active_layer = &layer_a;
  manager.triggered = r2;
  manager.state = RecognizerManagerState_RecognizersTriggered;
  recognizer_manager_deregister_recognizer(&manager, r1);
  cl_assert_equal_p(manager.triggered, r2);
  cl_assert_equal_p(manager.active_layer, &layer_a);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersTriggered);
  cl_assert_equal_i(r2->state, RecognizerState_Started);
  cl_assert(!recognizer_get_manager(r1));

  recognizer_set_manager(r1, &manager);
  recognizer_set_simultaneous_with(r2, prv_simultaneous_with_cb);
  r1->state = RecognizerState_Started;
  r2->state = RecognizerState_Started;
  manager.triggered = r1;
  manager.state = RecognizerManagerState_RecognizersTriggered;
  recognizer_manager_deregister_recognizer(&manager, r1);
  cl_assert_equal_p(manager.triggered, r2);
  cl_assert_equal_p(manager.active_layer, &layer_a);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersTriggered);
  cl_assert_equal_i(r2->state, RecognizerState_Started);
  cl_assert(!recognizer_get_manager(r1));
}

void test_recognizer_manager__handle_state_change(void) {
  const int k_rec_count = 2;
  s_dummy_impl.handle_touch_event = prv_handle_touch_event_test;
  s_dummy_impl.reset = prv_reset_test;
  Recognizer **r = prv_create_recognizers(k_rec_count);

  Window window = {};
  layer_init(&window.layer, &GRectZero);
  Layer *root = &window.layer;
  RecognizerManager manager;
  recognizer_manager_init(&manager);

  Layer layer_a;
  layer_init(&layer_a, &GRectZero);
  layer_add_child(root, &layer_a);

  manager.window = &window;
  manager.active_layer = &layer_a;
  manager.state = RecognizerManagerState_RecognizersActive;

  recognizer_add_to_list(r[0], &layer_a.recognizer_list);
  recognizer_add_to_list(r[1], &layer_a.recognizer_list);

  recognizer_set_manager(r[0], &manager);
  recognizer_set_manager(r[1], &manager);

  r[0]->state = RecognizerState_Failed;
  recognizer_manager_handle_state_change(&manager, r[0]);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersActive);
  cl_assert_equal_p(manager.active_layer, &layer_a);
  prv_compare_recognizers_processed(NULL, 0, &s_recognizers_reset);

  r[1]->state = RecognizerState_Failed;
  recognizer_manager_handle_state_change(&manager, r[1]);
  cl_assert_equal_i(manager.state, RecognizerManagerState_WaitForTouchdown);
  cl_assert_equal_p(manager.active_layer, NULL);
  prv_compare_recognizers_processed((int []) { 0, 1 }, 2, &s_recognizers_reset);

  manager.active_layer = &layer_a;
  manager.state = RecognizerManagerState_RecognizersActive;
  manager.triggered = NULL;
  r[0]->state = RecognizerState_Started;
  r[1]->state = RecognizerState_Possible;
  recognizer_manager_handle_state_change(&manager, r[0]);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersTriggered);
  cl_assert_equal_p(manager.triggered, r[0]);
  cl_assert_equal_p(manager.active_layer, &layer_a);
  prv_compare_recognizers_processed(NULL, 0, &s_recognizers_reset);
  cl_assert_equal_i(r[0]->state, RecognizerState_Started);
  cl_assert_equal_i(r[1]->state, RecognizerState_Failed);

  r[0]->state = RecognizerState_Updated;
  recognizer_manager_handle_state_change(&manager, r[0]);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersTriggered);
  cl_assert_equal_p(manager.triggered, r[0]);
  cl_assert_equal_p(manager.active_layer, &layer_a);
  prv_compare_recognizers_processed(NULL, 0, &s_recognizers_reset);

  r[0]->state = RecognizerState_Completed;
  recognizer_manager_handle_state_change(&manager, r[0]);
  cl_assert_equal_i(manager.state, RecognizerManagerState_WaitForTouchdown);
  cl_assert_equal_p(manager.triggered, NULL);
  cl_assert_equal_p(manager.active_layer, NULL);
  prv_compare_recognizers_processed((int []) { 0, 1 }, 2, &s_recognizers_reset);
  cl_assert_equal_i(r[0]->state, RecognizerState_Possible);
  cl_assert_equal_i(r[1]->state, RecognizerState_Possible);

  r[0]->state = RecognizerState_Completed;
  manager.active_layer = &layer_a;
  manager.state = RecognizerManagerState_RecognizersActive;
  manager.triggered = NULL;
  recognizer_manager_handle_state_change(&manager, r[0]);
  cl_assert_equal_i(manager.state, RecognizerManagerState_WaitForTouchdown);
  cl_assert_equal_p(manager.triggered, NULL);
  cl_assert_equal_p(manager.active_layer, NULL);
  prv_compare_recognizers_processed((int []) { 0, 1 }, 2, &s_recognizers_reset);
  cl_assert_equal_i(r[0]->state, RecognizerState_Possible);
  cl_assert_equal_i(r[1]->state, RecognizerState_Possible);

  r[0]->state = RecognizerState_Cancelled;
  manager.active_layer = &layer_a;
  manager.state = RecognizerManagerState_RecognizersActive;
  manager.triggered = r[0];
  recognizer_manager_handle_state_change(&manager, r[0]);
  cl_assert_equal_i(manager.state, RecognizerManagerState_WaitForTouchdown);
  cl_assert_equal_p(manager.triggered, NULL);
  cl_assert_equal_p(manager.active_layer, NULL);
  prv_compare_recognizers_processed((int []) { 0, 1 }, 2, &s_recognizers_reset);

  recognizer_set_simultaneous_with(r[0], prv_simultaneous_with_cb);
  r[0]->state = RecognizerState_Started;
  r[1]->state = RecognizerState_Completed;
  manager.active_layer = &layer_a;
  manager.state = RecognizerManagerState_RecognizersTriggered;
  manager.triggered = r[0];
  recognizer_manager_handle_state_change(&manager, r[1]);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersTriggered);
  cl_assert_equal_p(manager.triggered, r[0]);
  cl_assert_equal_p(manager.active_layer, &layer_a);
  prv_compare_recognizers_processed(NULL, 0, &s_recognizers_reset);
  cl_assert_equal_i(r[0]->state, RecognizerState_Started);
  cl_assert_equal_i(r[1]->state, RecognizerState_Completed);

  recognizer_set_simultaneous_with(r[0], prv_simultaneous_with_cb);
  r[0]->state = RecognizerState_Started;
  r[1]->state = RecognizerState_Completed;
  manager.active_layer = &layer_a;
  manager.state = RecognizerManagerState_RecognizersTriggered;
  manager.triggered = r[1];
  recognizer_manager_handle_state_change(&manager, r[1]);
  cl_assert_equal_i(manager.state, RecognizerManagerState_RecognizersTriggered);
  cl_assert_equal_p(manager.triggered, r[0]);
  cl_assert_equal_p(manager.active_layer, &layer_a);
  prv_compare_recognizers_processed(NULL, 0, &s_recognizers_reset);
  cl_assert_equal_i(r[0]->state, RecognizerState_Started);
  cl_assert_equal_i(r[1]->state, RecognizerState_Completed);
}
