/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system_apps/timeline/text_node.h"

#include "clar.h"
#include "pebble_asserts.h"

// Stubs
/////////////////////

#include "stubs_app_state.h"
#include "stubs_heap.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_process_manager.h"

// TODO: PBL-22271 Complete timeline text node unit tests

// Fakes
/////////////////////

typedef struct GTextNodeTestData {
  GContext gcontext;
  GRect clip_box;
  GTextNode *text_node;
  GSize max_used_size;
} GTextNodeTestData;

GTextNodeTestData s_data;

#define TEST_TEXT "DUMMY TEXT"
#define TEST_FONT ((void *)0xf0a7f0a7)
#define TEST_TEXT_SIZE GSize(50, 18)
#define TEST_TEXT_BOX GRect(10, 10, 140, 200)

void graphics_draw_text(GContext *ctx, const char *text, GFont const font, const GRect box,
                        const GTextOverflowMode overflow_mode, const GTextAlignment alignment,
                        const GTextLayoutCacheRef layout_ref) {
  GTextNodeText *text_node = (GTextNodeText *)s_data.text_node;
  cl_assert(text_node);
  cl_assert_equal_s(text_node->text, text);
  cl_assert_equal_p(text_node->font, font);
  cl_assert_equal_p(text_node->overflow, overflow_mode);
  cl_assert_equal_p(text_node->alignment, alignment);

  TextLayoutExtended *layout = (TextLayoutExtended *)layout_ref;
  cl_assert_equal_p(text_node->line_spacing_delta, layout->line_spacing_delta);

  layout->max_used_size = s_data.max_used_size;
}

GSize graphics_text_layout_get_max_used_size(GContext *ctx, const char *text,
                                             GFont const font, const GRect box,
                                             const GTextOverflowMode overflow_mode,
                                             const GTextAlignment alignment,
                                             GTextLayoutCacheRef layout_ref) {
  GTextNodeText *text_node = (GTextNodeText *)s_data.text_node;
  cl_assert(text_node);
  cl_assert_equal_s(text_node->text, text);
  cl_assert_equal_p(text_node->font, font);
  cl_assert_equal_p(text_node->overflow, overflow_mode);
  cl_assert_equal_p(text_node->alignment, alignment);

  TextLayoutExtended *layout = (TextLayoutExtended *)layout_ref;
  cl_assert_equal_p(text_node->line_spacing_delta, layout->line_spacing_delta);

  layout->max_used_size = s_data.max_used_size;
  return layout->max_used_size;
}

// Setup and Teardown
////////////////////////////////////

void test_graphics_text_node__initialize(void) {
  s_data = (GTextNodeTestData) {};
}

void test_graphics_text_node__cleanup(void) {
}

// Tests
////////////////////////////////////

void test_graphics_text_node__text_draw(void) {
  GContext *ctx = &s_data.gcontext;
  GTextNodeText text_node = {};
  text_node.text = (char *)TEST_TEXT;
  text_node.font = TEST_FONT;
  text_node.overflow = GTextOverflowModeTrailingEllipsis;
  text_node.alignment = GTextAlignmentCenter;

  s_data.text_node = &text_node.node;
  s_data.max_used_size = TEST_TEXT_SIZE;

  GSize size;
  graphics_text_node_draw(&text_node.node, ctx, &TEST_TEXT_BOX, NULL, &size);
  cl_assert(size.w > 0);
  cl_assert(size.h > 0);
  cl_assert_equal_i(size.w, TEST_TEXT_SIZE.w);
  cl_assert_equal_i(size.h, TEST_TEXT_SIZE.h);
}

void test_graphics_text_node__text_size(void) {
  GContext *ctx = &s_data.gcontext;
  GTextNodeText text_node = {};
  text_node.text = (char *)TEST_TEXT;
  text_node.font = TEST_FONT;
  text_node.overflow = GTextOverflowModeTrailingEllipsis;
  text_node.alignment = GTextAlignmentCenter;

  s_data.text_node = &text_node.node;
  s_data.max_used_size = TEST_TEXT_SIZE;

  GSize size;
  graphics_text_node_get_size(&text_node.node, ctx, &TEST_TEXT_BOX, NULL, &size);
  cl_assert(size.w > 0);
  cl_assert(size.h > 0);
  cl_assert_equal_i(size.w, TEST_TEXT_SIZE.w);
  cl_assert_equal_i(size.h, TEST_TEXT_SIZE.h);
}

static int s_num_draw_custom_calls = 0;

static void prv_draw_custom(GContext *ctx, const GRect *box, const GTextNodeDrawConfig *config,
                            bool render, GSize *size_out, void *user_data) {
  s_num_draw_custom_calls++;
  *size_out = TEST_TEXT_SIZE;
}

void test_graphics_text_node__custom_cached_size(void) {
  GContext *ctx = &s_data.gcontext;
  GTextNodeCustom custom_node = {};
  custom_node.node.type = GTextNodeType_Custom;
  custom_node.callback = prv_draw_custom;

  GSize size;
  graphics_text_node_get_size(&custom_node.node, ctx, &TEST_TEXT_BOX, NULL, &size);
  cl_assert(size.w > 0);
  cl_assert(size.h > 0);
  cl_assert_equal_i(size.w, TEST_TEXT_SIZE.w);
  cl_assert_equal_i(size.h, TEST_TEXT_SIZE.h);
  cl_assert_equal_i(size.w, custom_node.node.cached_size.w);
  cl_assert_equal_i(size.h, custom_node.node.cached_size.h);
  cl_assert_equal_i(s_num_draw_custom_calls, 1);

  graphics_text_node_get_size(&custom_node.node, ctx, &TEST_TEXT_BOX, NULL, &size);
  cl_assert(size.w > 0);
  cl_assert(size.h > 0);
  cl_assert_equal_i(size.w, TEST_TEXT_SIZE.w);
  cl_assert_equal_i(size.h, TEST_TEXT_SIZE.h);
  cl_assert_equal_i(size.w, custom_node.node.cached_size.w);
  cl_assert_equal_i(size.h, custom_node.node.cached_size.h);
  cl_assert_equal_i(s_num_draw_custom_calls, 1);
}

void test_graphics_text_node__create_container_nodes_buffer(void) {
  GTextNodeHorizontal *h_empty = graphics_text_node_create_horizontal(0);
  cl_assert_equal_i(h_empty->container.max_nodes, 0);
  cl_assert_equal_p(NULL, h_empty->container.nodes);

  GTextNodeHorizontal *h_nodes = graphics_text_node_create_horizontal(3);
  cl_assert_equal_i(h_nodes->container.max_nodes, 3);
  cl_assert_equal_p(h_nodes + 1, (void *)h_nodes->container.nodes);

  GTextNodeVertical *v_empty = graphics_text_node_create_vertical(0);
  cl_assert_equal_i(v_empty->container.max_nodes, 0);
  cl_assert_equal_p(NULL, v_empty->container.nodes);

  GTextNodeVertical *v_nodes = graphics_text_node_create_vertical(3);
  cl_assert_equal_i(v_nodes->container.max_nodes, 3);
  cl_assert_equal_p(v_nodes + 1, (void *)v_nodes->container.nodes);
}

void test_graphics_text_node__destroy(void) {
  const char *str_a = "A";
  GTextNodeText *text_a = graphics_text_node_create_text(strlen(str_a) + 1);
  cl_assert(text_a->node.free_on_destroy);
  strcpy((char *)text_a->text, str_a);

  const char *str_b = task_strdup("B");
  GTextNodeText *text_b = graphics_text_node_create_text(0);
  cl_assert(text_b->node.free_on_destroy);
  text_b->text = (char *)str_b;

  const char *str_c = "C";
  GTextNodeText text_c = {
    .text = (char *)str_c,
  };
  cl_assert(!text_c.node.free_on_destroy);

  GTextNodeCustom *custom_a = graphics_text_node_create_custom(NULL, NULL);
  cl_assert(custom_a->node.free_on_destroy);

  GTextNodeHorizontal *horizontal_a = graphics_text_node_create_horizontal(3);
  cl_assert(horizontal_a->container.node.free_on_destroy);
  cl_assert_equal_i(horizontal_a->container.max_nodes, 3);
  cl_assert_equal_i(horizontal_a->container.num_nodes, 0);
  cl_assert(graphics_text_node_container_add_child(&horizontal_a->container, &text_a->node));
  cl_assert(graphics_text_node_container_add_child(&horizontal_a->container, &text_b->node));
  cl_assert(graphics_text_node_container_add_child(&horizontal_a->container, &text_c.node));
  cl_assert(!graphics_text_node_container_add_child(&horizontal_a->container, &custom_a->node));
  cl_assert_equal_i(horizontal_a->container.num_nodes, 3);

  GTextNodeVertical *vertical_a = graphics_text_node_create_vertical(2);
  cl_assert(vertical_a->container.node.free_on_destroy);
  cl_assert_equal_i(vertical_a->container.max_nodes, 2);
  cl_assert_equal_i(vertical_a->container.num_nodes, 0);
  cl_assert(graphics_text_node_container_add_child(&vertical_a->container, &horizontal_a->container.node));
  cl_assert(graphics_text_node_container_add_child(&vertical_a->container, &custom_a->node));
  cl_assert(!graphics_text_node_container_add_child(&vertical_a->container, &text_c.node));
  cl_assert_equal_i(vertical_a->container.num_nodes, 2);

  graphics_text_node_destroy(&vertical_a->container.node);

  task_free((char *)str_b);
}

static void prv_draw_custom_clip(GContext *ctx, const GRect *box,
                                 const GTextNodeDrawConfig *config, bool render, GSize *size_out,
                                 void *user_data) {
  cl_assert_equal_grect(ctx->draw_state.clip_box, s_data.clip_box);
}

#define TEST_CLIP_BOX GRect(10, 20, 30, 40)

void test_graphics_text_node__clip(void) {
  GContext *ctx = &s_data.gcontext;
  GTextNodeCustom custom_node = {};
  custom_node.node.type = GTextNodeType_Custom;
  custom_node.callback = prv_draw_custom_clip;

  // Clipping off
  s_data.gcontext.draw_state.clip_box = DISP_FRAME;
  s_data.clip_box = DISP_FRAME;
  graphics_text_node_draw(&custom_node.node, ctx, &TEST_CLIP_BOX, NULL, NULL);
  cl_assert_equal_grect(ctx->draw_state.clip_box, DISP_FRAME);

  // Clipping on
  custom_node.node.clip = true;
  s_data.gcontext.draw_state.clip_box = DISP_FRAME;
  s_data.clip_box = TEST_CLIP_BOX;
  graphics_text_node_draw(&custom_node.node, ctx, &TEST_CLIP_BOX, NULL, NULL);
  cl_assert_equal_grect(ctx->draw_state.clip_box, DISP_FRAME);
}
