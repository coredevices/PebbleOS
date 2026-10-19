/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/util/size.h>

#include <applib/ui/layer.h>
#include <applib/ui/layer_private.h>
#include <applib/ui/window.h>
#include <clar.h>
#include <pebble_asserts.h>

// Stubs
////////////////////////////////////
#include <stubs_app_state.h>

// Setup
////////////////////////////////////

void test_layer__initialize(void) {
}

void test_layer__cleanup(void) {
}

GDrawState graphics_context_get_drawing_state(GContext *ctx) {
  return (GDrawState){};
}

bool graphics_release_frame_buffer(GContext *ctx, GBitmap *buffer) {
  return false;
}

void graphics_context_set_drawing_state(GContext *ctx, GDrawState draw_state) {
}

void window_schedule_render(struct Window *window) {
}

void recognizer_destroy(Recognizer *recognizer) {
}

void recognizer_add_to_list(Recognizer *recognizer, RecognizerList *list) {
}

void recognizer_remove_from_list(Recognizer *recognizer, RecognizerList *list) {
}

RecognizerManager *window_get_recognizer_manager(Window *window) {
  return nullptr;
}

void recognizer_manager_cancel_and_reset(RecognizerManager *manager) {
}

bool recognizer_list_iterate(RecognizerList *list, RecognizerListIteratorCb iter_cb,
                             void *context) {
  return false;
}

void recognizer_manager_register_recognizer(RecognizerManager *manager, Recognizer *recognizer) {
}

void recognizer_manager_deregister_recognizer(RecognizerManager *manager, Recognizer *recognizer) {
}
// Tests
////////////////////////////////////

void test_layer__add_child_and_remove_from_parent(void) {
  Layer parent, child_a, child_b, child_c, grand_child_a;
  Layer *layers[] = {&parent, &child_a, &child_b, &child_c, &grand_child_a};
  for (int i = 0; i < ARRAY_LENGTH(layers); ++i) {
    layer_init(layers[i], &GRectZero);
  }
  // Create this hierarchy:
  // This hits first_child and next_sibling add code paths.
  //
  // +-parent
  //     |
  //     '->child_a->child_b->child_c
  //           |
  //           '->grand_child_a
  //
  cl_assert(parent.first_child == nullptr);
  layer_add_child(&parent, &child_a);
  cl_assert(parent.first_child == &child_a);
  layer_add_child(&parent, &child_b);
  cl_assert(parent.first_child == &child_a);
  cl_assert(child_a.next_sibling == &child_b);
  cl_assert(child_a.parent == &parent);
  cl_assert(child_b.parent == &parent);
  layer_add_child(&parent, &child_c);
  cl_assert(child_c.parent == &parent);
  cl_assert(child_b.next_sibling == &child_c);
  layer_add_child(&child_a, &grand_child_a);
  cl_assert(grand_child_a.parent == &child_a);

  // Remove non-first-child (child_b):
  //
  // +-parent
  //     |
  //     '->child_a->child_c
  //           |
  //           '->grand_child_a
  //
  // +-child_b
  //
  layer_remove_from_parent(&child_b);
  cl_assert(child_b.parent == nullptr);
  cl_assert(child_b.next_sibling == nullptr);
  cl_assert(parent.first_child == &child_a);
  cl_assert(child_a.next_sibling == &child_c);
  cl_assert(grand_child_a.parent == &child_a);
  cl_assert(child_c.parent == &parent);

  // Remove first-child (child_a):
  //
  // +-parent
  //     |
  //     '->child_c
  //
  // +-child_a
  //       |
  //       '->grand_child_a
  //
  layer_remove_from_parent(&child_a);
  cl_assert(parent.first_child == &child_c);
  cl_assert(child_c.parent == &parent);
  cl_assert(child_a.parent == nullptr);
  cl_assert(child_a.next_sibling == nullptr);
  cl_assert(grand_child_a.parent == &child_a);

  // Return early when (parent->paren == NULL):
  layer_remove_from_parent(&parent);
}

void test_layer__remove_child_layers(void) {
  Layer parent, child_a, child_b;
  Layer *layers[] = {&parent, &child_a, &child_b};
  for (int i = 0; i < ARRAY_LENGTH(layers); ++i) {
    layer_init(layers[i], &GRectZero);
  }
  // Create this hierarchy:
  //
  // +-parent
  //     |
  //     '->child_a->child_b
  //
  layer_add_child(&parent, &child_a);
  layer_add_child(&parent, &child_b);
  layer_remove_child_layers(&parent);
  cl_assert(child_a.parent == nullptr);
  cl_assert(child_a.next_sibling == nullptr);
  cl_assert(child_b.parent == nullptr);
  cl_assert(parent.first_child == nullptr);
}

void test_layer__insert_below(void) {
  Layer parent, child_a, child_b, child_c;
  Layer *layers[] = {&parent, &child_a, &child_b, &child_c};
  for (int i = 0; i < ARRAY_LENGTH(layers); ++i) {
    layer_init(layers[i], &GRectZero);
  }
  // Create this hierarchy:
  //
  // +-parent
  //     |
  //     '->child_a
  //
  layer_add_child(&parent, &child_a);

  // Insert child_b below child_b (first_child code path):
  //
  // +-parent
  //     |
  //     '->child_b->child_a
  //
  layer_insert_below_sibling(&child_b, &child_a);
  cl_assert(child_b.parent == &parent);
  cl_assert(child_b.next_sibling == &child_a);
  cl_assert(child_a.next_sibling == nullptr);

  // Insert child_c below child_a (next_sibling code path):
  //
  // +-parent
  //     |
  //     '->child_b->child_c->child_a
  //
  layer_insert_below_sibling(&child_c, &child_a);
  cl_assert(parent.first_child == &child_b);
  cl_assert(child_b.next_sibling == &child_c);
  cl_assert(child_c.parent == &parent);
  cl_assert(child_c.next_sibling == &child_a);
  cl_assert(child_a.next_sibling == nullptr);
}

void test_layer__insert_above(void) {
  Layer parent, child_a, child_b, child_c;
  Layer *layers[] = {&parent, &child_a, &child_b, &child_c};
  for (int i = 0; i < ARRAY_LENGTH(layers); ++i) {
    layer_init(layers[i], &GRectZero);
  }
  // Create this hierarchy:
  //
  // +-parent
  //     |
  //     '->child_b
  //
  layer_add_child(&parent, &child_b);

  // Insert child_a above child_b (first_child code path):
  //
  // +-parent
  //     |
  //     '->child_b->child_a
  //
  layer_insert_above_sibling(&child_a, &child_b);
  cl_assert(child_b.parent == &parent);
  cl_assert(child_b.next_sibling == &child_a);
  cl_assert(child_a.next_sibling == nullptr);

  // Insert child_c above child_b (next_sibling code path):
  //
  // +-parent
  //     |
  //     '->child_b->child_c->child_a
  //
  layer_insert_above_sibling(&child_c, &child_b);
  cl_assert(parent.first_child == &child_b);
  cl_assert(child_b.next_sibling == &child_c);
  cl_assert(child_c.parent == &parent);
  cl_assert(child_c.next_sibling == &child_a);
  cl_assert(child_a.next_sibling == nullptr);
}

void test_layer__traverse(void) {
  Layer *stack[5];
  uint8_t current_stack = 0;

  Layer *a = layer_create(GRectZero);
  Layer *aa = layer_create(GRectZero);
  Layer *aaa = layer_create(GRectZero);
  Layer *aaaa = layer_create(GRectZero);
  Layer *ab = layer_create(GRectZero);
  Layer *b = layer_create(GRectZero);

  layer_add_child(a, aa);
  layer_add_child(aa, aaa);
  layer_add_child(aaa, aaaa);
  layer_add_child(a, ab);
  a->next_sibling = b;

  stack[0] = a;

  // go to child if possible
  Layer *actual =
      __layer_tree_traverse_next__test_accessor(stack, ARRAY_LENGTH(stack), &current_stack, true);
  cl_assert_equal_p(aa, actual);
  cl_assert_equal_i(1, current_stack);
  cl_assert_equal_p(a, stack[0]);
  cl_assert_equal_p(aa, stack[1]);

  // go to child if possible
  actual =
      __layer_tree_traverse_next__test_accessor(stack, ARRAY_LENGTH(stack), &current_stack, true);
  cl_assert_equal_p(aaa, actual);
  cl_assert_equal_i(2, current_stack);
  cl_assert_equal_p(a, stack[0]);
  cl_assert_equal_p(aa, stack[1]);
  cl_assert_equal_p(aaa, stack[2]);

  // go to child if possible
  actual =
      __layer_tree_traverse_next__test_accessor(stack, ARRAY_LENGTH(stack), &current_stack, true);
  cl_assert_equal_p(aaaa, actual);
  cl_assert_equal_i(3, current_stack);
  cl_assert_equal_p(a, stack[0]);
  cl_assert_equal_p(aa, stack[1]);
  cl_assert_equal_p(aaa, stack[2]);
  cl_assert_equal_p(aaaa, stack[3]);

  // go back two levels and then to sibling
  actual =
      __layer_tree_traverse_next__test_accessor(stack, ARRAY_LENGTH(stack), &current_stack, true);
  cl_assert_equal_p(ab, actual);
  cl_assert_equal_i(1, current_stack);
  cl_assert_equal_p(a, stack[0]);
  cl_assert_equal_p(ab, stack[1]);

  // go back one level and then to sibling
  actual =
      __layer_tree_traverse_next__test_accessor(stack, ARRAY_LENGTH(stack), &current_stack, true);
  cl_assert_equal_p(b, actual);
  cl_assert_equal_i(0, current_stack);
  cl_assert_equal_p(b, stack[0]);

  // no more siblings on root level
  actual =
      __layer_tree_traverse_next__test_accessor(stack, ARRAY_LENGTH(stack), &current_stack, true);
  cl_assert_equal_p(nullptr, actual);
  cl_assert_equal_i(0, current_stack);

  // do not descend
  stack[0] = a;
  current_stack = 0;
  actual =
      __layer_tree_traverse_next__test_accessor(stack, ARRAY_LENGTH(stack), &current_stack, false);
  cl_assert_equal_p(b, actual);
  cl_assert_equal_i(0, current_stack);
  cl_assert_equal_p(b, stack[0]);

  // test limited stack size (go to sibling instead of child)
  stack[0] = a;
  current_stack = 0;
  actual = __layer_tree_traverse_next__test_accessor(stack, 1, &current_stack, true);
  cl_assert_equal_p(b, actual);
  cl_assert_equal_i(0, current_stack);
  cl_assert_equal_p(b, stack[0]);

  // test limited stack size (go to sibling of parent instead of child)
  stack[0] = a;
  stack[1] = aa;
  stack[2] = aaa;
  current_stack = 2;
  actual = __layer_tree_traverse_next__test_accessor(stack, 3, &current_stack, true);
  cl_assert_equal_p(ab, actual);
  cl_assert_equal_i(1, current_stack);
  cl_assert_equal_p(a, stack[0]);
  cl_assert_equal_p(ab, stack[1]);

  // not necessary to free memory on unit tests
}

void test_layer__is_ancestor(void) {
  Layer parent, child_a, child_b, child_c;
  Layer *layers[] = {&parent, &child_a, &child_b, &child_c};
  for (int i = 0; i < ARRAY_LENGTH(layers); ++i) {
    layer_init(layers[i], &GRectZero);
  }

  layer_add_child(&parent, &child_a);
  cl_assert(layer_is_descendant(&child_a, &parent));
  cl_assert(!layer_is_descendant(&parent, &child_a));
  cl_assert(!layer_is_descendant(&child_b, &parent));

  layer_add_child(&parent, &child_b);
  cl_assert(layer_is_descendant(&child_b, &parent));
  cl_assert(!layer_is_descendant(&child_b, &child_a));
  cl_assert(!layer_is_descendant(&child_c, &child_a));

  layer_add_child(&child_a, &child_c);
  cl_assert(layer_is_descendant(&child_c, &child_a));
  cl_assert(layer_is_descendant(&child_c, &parent));
  cl_assert(!layer_is_descendant(&child_c, &child_b));
}

void test_layer__find_layer_contains_point(void) {
  Layer parent, child_a, child_b, child_c, child_d, child_e, child_f;
  Layer *layers[] = {&parent, &child_a, &child_b, &child_c, &child_d, &child_e, &child_f};
  for (int i = 0; i < ARRAY_LENGTH(layers); ++i) {
    layer_init(layers[i], &GRectZero);
  }
  layer_set_frame(&parent, &GRect(0, 0, 20, 20));
  layer_set_frame(&child_a, &GRect(0, 0, 10, 10));
  layer_set_frame(&child_b, &GRect(2, 2, 6, 6));
  layer_set_frame(&child_c, &GRect(10, 10, 10, 10));
  layer_set_frame(&child_d, &GRect(2, 2, 6, 6));
  layer_set_frame(&child_e, &GRect(10, 10, 10, 10));
  layer_set_frame(&child_f, &GRect(-10, -10, 40, 40));
  layer_add_child(&parent, &child_a);

  cl_assert_equal_p(layer_find_layer_containing_point(&child_a, &GPoint(11, 11)), nullptr);
  cl_assert_equal_p(layer_find_layer_containing_point(&child_a, &GPoint(10, 10)), nullptr);
  cl_assert_equal_p(layer_find_layer_containing_point(&child_a, &GPoint(9, 9)), &child_a);
  cl_assert_equal_p(layer_find_layer_containing_point(&child_a, &GPoint(0, 0)), &child_a);

  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(9, 9)), &child_a);
  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(10, 10)), &parent);

  layer_add_child(&child_a, &child_f);
  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(9, 9)), &child_f);
  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(1, 1)), &child_f);

  // child layers are subject to their parents' bounds as well as their own (parent layers clip the
  // bounds of child layers)
  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(15, 15)), &parent);
  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(-5, -5)), nullptr);
  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(21, 21)), nullptr);
  layer_remove_from_parent(&child_f);

  layer_add_child(&parent, &child_b);
  layer_add_child(&parent, &child_c);
  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(9, 9)), &child_a);
  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(6, 6)), &child_b);
  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(15, 15)), &child_c);

  layer_add_child(&child_a, &child_d);
  layer_add_child(&child_c, &child_e);
  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(9, 9)), &child_a);
  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(6, 6)), &child_d);
  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(15, 15)), &child_e);
}

static bool prv_override_layer_contains_point(const Layer *layer, const GPoint *point) {
  return true;
}

void test_layer__find_layer_contains_point_override_layer_contains_point(void) {
  Layer parent, child_a, child_b;
  Layer *layers[] = {&parent, &child_a, &child_b};
  for (int i = 0; i < ARRAY_LENGTH(layers); ++i) {
    layer_init(layers[i], &GRectZero);
  }
  layer_set_frame(&parent, &GRect(0, 0, 20, 20));
  layer_set_frame(&child_a, &GRect(0, 0, 10, 10));
  layer_set_frame(&child_b, &GRect(2, 2, 6, 6));
  layer_add_child(&parent, &child_a);
  layer_add_child(&child_a, &child_b);
  layer_set_contains_point_override(&child_b, prv_override_layer_contains_point);

  cl_assert_equal_p(layer_find_layer_containing_point(&child_b, &GPoint(9, 9)), &child_b);
  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(9, 9)), &child_b);
  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(0, 0)), &child_b);

  // outside the bounds of child a, so child b is not found
  cl_assert_equal_p(layer_find_layer_containing_point(&parent, &GPoint(15, 15)), &parent);
}

void test_layer__convert_point_to_screen(void) {
  Layer parent, child, grandchild;
  layer_init(&parent, &GRect(10, 20, 100, 100));
  layer_init(&child, &GRect(5, 6, 50, 50));
  layer_init(&grandchild, &GRect(1, 2, 10, 10));
  layer_add_child(&parent, &child);
  layer_add_child(&child, &grandchild);

  cl_assert_equal_gpoint(layer_convert_point_to_screen(&parent, GPointZero), GPoint(10, 20));
  cl_assert_equal_gpoint(layer_convert_point_to_screen(&grandchild, GPoint(3, 4)),
                         GPoint(10 + 5 + 1 + 3, 20 + 6 + 2 + 4));

  // Bounds origins (e.g. scrolling) shift the children
  layer_set_bounds(&child, &GRect(-7, -8, 50, 50));
  cl_assert_equal_gpoint(layer_convert_point_to_screen(&grandchild, GPointZero),
                         GPoint(10 + 5 - 7 + 1, 20 + 6 - 8 + 2));

  cl_assert_equal_gpoint(layer_convert_point_to_screen(nullptr, GPoint(3, 4)), GPoint(3, 4));
}

void test_layer__convert_point_to_screen_stops_at_window_root(void) {
  Window window = {};
  layer_init(&window.layer, &GRect(30, 40, 100, 100));
  window.layer.window = &window;
  Layer child;
  layer_init(&child, &GRect(5, 6, 50, 50));
  layer_add_child(&window.layer, &child);

  // The window's root layer frame and bounds are not taken into account
  cl_assert_equal_gpoint(layer_convert_point_to_screen(&window.layer, GPoint(1, 2)), GPoint(1, 2));
  cl_assert_equal_gpoint(layer_convert_point_to_screen(&child, GPoint(1, 2)), GPoint(6, 8));
}

void test_layer__convert_rect_to_screen(void) {
  Layer parent, child;
  layer_init(&parent, &GRect(10, 20, 100, 100));
  layer_init(&child, &GRect(5, 6, 50, 50));
  layer_add_child(&parent, &child);

  cl_assert_equal_grect(layer_convert_rect_to_screen(&child, GRect(1, 2, 3, 4)),
                        GRect(16, 28, 3, 4));

  GRect global_frame;
  layer_get_global_frame(&child, &global_frame);
  cl_assert_equal_grect(global_frame, GRect(15, 26, 50, 50));
}
