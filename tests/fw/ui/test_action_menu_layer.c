/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "pebble_asserts.h"

#include "applib/ui/action_menu_layer.h"
#include "applib/ui/menu_layer.h"
#include "applib/ui/menu_layer_private.h"
#include "applib/ui/property_animation.h"
#include "applib/ui/recognizer/recognizer.h"
#include "applib/ui/recognizer/recognizer_list.h"
#include "applib/ui/recognizer/recognizer_manager.h"
#include "applib/ui/recognizer/touch_nav.h"
#include "shell/system_theme.h"
#include "pbl/util/size.h"

#include "fake_rtc.h"
#include "pbl/drivers/rtc.h"

// Stubs
/////////////////////
#include "stubs_app_state.h"
#include "stubs_app_timer.h"
#include "stubs_click.h"
#include "stubs_fonts.h"
#include "stubs_graphics.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_process_manager.h"
#include "stubs_system_theme.h"
#include "stubs_ui_window.h"
#include "stubs_unobstructed_area.h"
#include "stubs_vibes.h"

// ---------------------------------------------------------------------------------------------
// Touch-navigation harness (CONFIG_TOUCH), mirroring test_menu_layer.c: menu_layer.c resolves the
// per-task touch-nav state through these accessors; the recognizer manager needs a few
// window/layer collaborators to link.

static bool s_nav_enabled = true;
bool sys_touch_nav_enabled(void) {
  return s_nav_enabled;
}
bool sys_touch_app_nav_active(void) {
  return false;
}

static TouchNavState s_touch_nav_state;
struct TouchNavState *app_state_get_touch_nav_state(void) {
  return &s_touch_nav_state;
}
struct TouchNavState *modal_manager_get_touch_nav_state(void) {
  return &s_touch_nav_state;
}

static Layer s_root_layer;
static RecognizerManager s_recognizer_manager;
static RecognizerList s_global_list;

struct Layer *window_get_root_layer(const Window *window) {
  return &s_root_layer;
}
RecognizerList *window_get_recognizer_list(Window *window) {
  return NULL;
}
RecognizerManager *window_get_recognizer_manager(Window *window) {
  return &s_recognizer_manager;
}

static TouchNavOps s_bridge_ops;

static void prv_touch_nav_setup(void) {
  s_bridge_ops = (TouchNavOps){0};
  layer_init(&s_root_layer, &GRect(0, 0, 300, 400));
  recognizer_list_init(&s_global_list);
  recognizer_manager_init(&s_recognizer_manager);
  s_recognizer_manager.window = (Window *)&s_root_layer; // non-NULL sentinel
  s_recognizer_manager.global_list = &s_global_list;
  touch_nav_state_init(&s_touch_nav_state, &s_recognizer_manager, &s_bridge_ops);
}

// Fakes / stubs for action_menu_layer.c dependencies
////////////////////////

PreferredContentSize system_theme_get_content_size(void) {
  return PreferredContentSizeDefault;
}

static GContext s_gcontext;
GContext *graphics_context_get_current_context(void) {
  return &s_gcontext;
}

GDrawState graphics_context_get_drawing_state(GContext *ctx) {
  return (GDrawState){};
}
void graphics_context_set_drawing_state(GContext *ctx, GDrawState draw_state) {
}
void graphics_context_set_fill_color(GContext *ctx, GColor color) {
}
void graphics_context_set_stroke_color(GContext *ctx, GColor color) {
}
void graphics_context_set_text_color(GContext *ctx, GColor color) {
}
void graphics_context_set_compositing_mode(GContext *ctx, GCompOp mode) {
}

void graphics_fill_radial_internal(GContext *ctx, GPoint center, uint16_t radius_inner,
                                   uint16_t radius_outer, int32_t angle_start, int32_t angle_end) {
}
void graphics_draw_bitmap_in_rect(GContext *ctx, const GBitmap *bitmap, const GRect *rect) {
}
void graphics_draw_horizontal_line_dotted(GContext *ctx, GPoint p, uint16_t length) {
}

// Every item lays out as a single stub-font line.
int16_t fonts_get_font_cap_offset(GFont font) {
  return FONT_HEIGHT * 22 / 100;
}

uint16_t graphics_text_layout_get_text_height(GContext *ctx, const char *text, GFont const font,
                                              uint16_t bounds_width,
                                              const GTextOverflowMode overflow_mode,
                                              const GTextAlignment alignment) {
  return FONT_HEIGHT;
}

GSize graphics_text_layout_get_max_used_size(GContext *ctx, const char *text, GFont const font,
                                             const GRect box, const GTextOverflowMode overflow_mode,
                                             const GTextAlignment alignment,
                                             GTextLayoutCacheRef layout) {
  return GSize(10, FONT_HEIGHT);
}

void menu_cell_basic_draw_custom(GContext *ctx, const Layer *cell_layer, GFont const title_font,
                                 const char *title, GFont const value_font, const char *value,
                                 GFont const subtitle_font, const char *subtitle, GBitmap *icon,
                                 bool icon_on_right, GTextOverflowMode overflow_mode) {
}
int16_t menu_cell_basic_horizontal_inset(void) {
  return 8;
}
int16_t menu_cell_small_cell_height(void) {
  return 24;
}
int16_t menu_cell_basic_cell_height(void) {
  return 44;
}

bool gbitmap_init_with_resource_system(GBitmap *bitmap, ResAppNum app_num, uint32_t resource_id) {
  return true;
}
void gbitmap_deinit(GBitmap *bitmap) {
}

Layer *inverter_layer_get_layer(InverterLayer *inverter_layer) {
  return &inverter_layer->layer;
}
void inverter_layer_init(InverterLayer *inverter, const GRect *frame) {
}

void window_long_click_subscribe(ButtonId button_id, uint16_t delay_ms, ClickHandler down_handler,
                                 ClickHandler up_handler) {
}
void window_single_click_subscribe(ButtonId button_id, ClickHandler handler) {
}
void window_single_repeating_click_subscribe(ButtonId button_id, uint16_t repeat_interval_ms,
                                             ClickHandler handler) {
}
void window_set_click_config_provider_with_context(Window *window,
                                                   ClickConfigProvider click_config_provider,
                                                   void *context) {
}
void window_set_click_context(ButtonId button_id, void *context) {
}

void content_indicator_destroy_for_scroll_layer(ScrollLayer *scroll_layer) {
}

static ContentIndicator s_content_indicator;
ContentIndicator *content_indicator_get_for_scroll_layer(ScrollLayer *scroll_layer) {
  return &s_content_indicator;
}
ContentIndicator *content_indicator_get_or_create_for_scroll_layer(ScrollLayer *scroll_layer) {
  return &s_content_indicator;
}
void content_indicator_set_content_available(ContentIndicator *content_indicator,
                                             ContentIndicatorDirection direction, bool available) {
}

void content_indicator_draw_arrow(GContext *ctx, const GRect *rect,
                                  ContentIndicatorDirection direction, GColor foreground,
                                  GColor background, GAlign alignment) {
}

// Observation
////////////////////////

static int s_select_count;
static const ActionMenuItem *s_last_selected_item;
static int s_selection_changed_count;
static const ActionMenuItem *s_last_changed_item;

static void prv_record_select(const ActionMenuItem *item, void *context) {
  s_select_count++;
  s_last_selected_item = item;
}

static void prv_record_selection_changed(const ActionMenuItem *item, void *context) {
  s_selection_changed_count++;
  s_last_changed_item = item;
}

static const ActionMenuItem s_wide_items[] = {
  {.label = "Dismiss", .is_leaf = 1},
  {.label = "Reply", .is_leaf = 1},
  {.label = "Mark as Read", .is_leaf = 1},
};

static const ActionMenuItem s_short_items[] = {
  {.label = "A", .is_leaf = 1}, {.label = "B", .is_leaf = 1}, {.label = "C", .is_leaf = 1},
  {.label = "D", .is_leaf = 1}, {.label = "E", .is_leaf = 1},
};

static ActionMenuLayer s_aml;
static ActionMenuItem s_emoji_items[21];

void prv_set_selected_index(ActionMenuLayer *aml, int selected_index, bool animated);

static const TouchNavWidgetOps *prv_glyph_grid_touch_ops(void) {
  return s_aml.menu_layer.touch_nav_node.ops;
}

static void prv_init_glyph_grid(void) {
  s_aml = (ActionMenuLayer){};
  action_menu_layer_init(&s_aml,
                         &GRect(13, 17, PBL_IF_ROUND_ELSE(260, 200), PBL_IF_ROUND_ELSE(260, 240)));
  layer_add_child(&s_root_layer, &s_aml.layer);
  for (int i = 0; i < ARRAY_LENGTH(s_emoji_items); ++i) {
    s_emoji_items[i] = (ActionMenuItem){.label = "😃", .is_leaf = 1};
  }
  action_menu_layer_set_glyph_grid(&s_aml, true);
  action_menu_layer_set_callbacks(&s_aml,
                                  (ActionMenuLayerCallbacks){
                                    .select = prv_record_select,
                                    .selection_changed = prv_record_selection_changed,
                                  },
                                  NULL);
  action_menu_layer_set_short_items(&s_aml, s_emoji_items, ARRAY_LENGTH(s_emoji_items), 0);
}

static GPoint prv_glyph_point(int index) {
  MenuLayer *menu = &s_aml.menu_layer;
  GRect frame;
  layer_get_global_frame(&menu->scroll_layer.layer, &frame);
  const GPoint offset = scroll_layer_get_content_offset(&menu->scroll_layer);
#if PBL_ROUND
  const int local = index % 7;
  const int row = local < 2 ? 0 : (local < 5 ? 1 : 2);
  const int count = row == 1 ? 3 : 2;
  const int column = local - (row == 0 ? 0 : (row == 1 ? 2 : 5));
  const int width = (frame.size.w - 16) / 4;
  const int y = menu->selection.y + row * 52 + 24;
#else
  const int count = 3;
  const int column = index % 3;
  const int width = frame.size.w / 3;
  const int y = menu->selection.y + 24;
#endif
  return GPoint(
      frame.origin.x + offset.x + (frame.size.w - count * width) / 2 + column * width + width / 2,
      frame.origin.y + offset.y + y);
}

// Return the tap point (in screen coordinates) whose hit-test resolves to \a row, independent of
// per-platform cell geometry.
static GPoint prv_tap_point_for_row(MenuLayer *ml, uint16_t row) {
  const int16_t offset_y = scroll_layer_get_content_offset(&ml->scroll_layer).y;
  for (int16_t y = 0; y < 1000; ++y) {
    MenuIndex idx;
    if (menu_layer_touch_find_row_at_content_y(ml, y, &idx) && idx.row == row) {
      // The first matching y is the row's top edge, which the half-open hit test owns.
      GRect frame;
      layer_get_global_frame(&ml->scroll_layer.layer, &frame);
      return GPoint(frame.origin.x + 10, frame.origin.y + y + offset_y);
    }
  }
  cl_fail("row not found");
  return GPointZero;
}

static void prv_reset_counters(void) {
  s_select_count = 0;
  s_last_selected_item = NULL;
  s_selection_changed_count = 0;
  s_last_changed_item = NULL;
}

static void prv_init_aml_with_wide_items(void) {
  // action_menu_layer_init expects zeroed memory (the firmware always zallocs an AML).
  s_aml = (ActionMenuLayer){};
  action_menu_layer_init(&s_aml, &GRect(0, 0, 144, 168));
  action_menu_layer_set_callbacks(&s_aml,
                                  (ActionMenuLayerCallbacks){
                                    .select = prv_record_select,
                                    .selection_changed = prv_record_selection_changed,
                                  },
                                  NULL);
  action_menu_layer_set_items(&s_aml, s_wide_items, ARRAY_LENGTH(s_wide_items), 0, 0);
  prv_reset_counters();
}

static void prv_advance_past_double_tap_window(void) {
  fake_rtc_increment_ticks((RtcTicks)400 * RTC_TICKS_HZ / 1000);
}

// Setup and Teardown
////////////////////////

void test_action_menu_layer__initialize(void) {
  fake_rtc_init(0, 0);
  prv_touch_nav_setup();
  prv_reset_counters();
}

void test_action_menu_layer__cleanup(void) {
}

// Tests
////////////////////////

// MOB-10624 regression: a tap on the selected action activates it through the AML select callback.
void test_action_menu_layer__tap_on_selected_item_activates(void) {
  prv_init_aml_with_wide_items();

  menu_layer_touch_handle_tap(&s_aml.menu_layer, prv_tap_point_for_row(&s_aml.menu_layer, 0));
  cl_assert_equal_i(s_select_count, 1);
  cl_assert(s_last_selected_item == &s_wide_items[0]);

  action_menu_layer_deinit(&s_aml);
}

// A wide-item row holds exactly one action, so a tap on a different action selects it and
// activates it in the same gesture (plain menus open on a single tap).
void test_action_menu_layer__tap_other_item_selects_and_activates(void) {
  prv_init_aml_with_wide_items();
  menu_layer_set_center_focused(&s_aml.menu_layer, false);

  menu_layer_touch_handle_tap(&s_aml.menu_layer, prv_tap_point_for_row(&s_aml.menu_layer, 2));
  cl_assert_equal_i(s_aml.selected_index, 2);
  cl_assert_equal_i(s_selection_changed_count, 1);
  cl_assert(s_last_changed_item == &s_wide_items[2]);
  cl_assert_equal_i(s_select_count, 1);
  cl_assert(s_last_selected_item == &s_wide_items[2]);

  action_menu_layer_deinit(&s_aml);
}

// A tap onto a short-item (column) row adopts the row's first column, so a follow-up tap
// activates the item that is actually highlighted rather than a stale index.
void test_action_menu_layer__tap_short_row_adopts_first_column(void) {
  s_aml = (ActionMenuLayer){};
  action_menu_layer_init(&s_aml, &GRect(0, 0, 144, 168));
  action_menu_layer_set_callbacks(&s_aml,
                                  (ActionMenuLayerCallbacks){
                                    .select = prv_record_select,
                                    .selection_changed = prv_record_selection_changed,
                                  },
                                  NULL);
  // 5 items in columns of 3: row 0 = items 0-2, row 1 = items 3-4.
  action_menu_layer_set_short_items(&s_aml, s_short_items, ARRAY_LENGTH(s_short_items), 0);
  prv_reset_counters();

  menu_layer_touch_handle_tap(&s_aml.menu_layer, prv_tap_point_for_row(&s_aml.menu_layer, 1));
  cl_assert_equal_i(s_select_count, 0);
  cl_assert_equal_i(s_aml.selected_index, 3);
  cl_assert_equal_i(s_selection_changed_count, 1);
  cl_assert(s_last_changed_item == &s_short_items[3]);

  prv_advance_past_double_tap_window();
  menu_layer_touch_handle_tap(&s_aml.menu_layer, prv_tap_point_for_row(&s_aml.menu_layer, 1));
  cl_assert_equal_i(s_select_count, 1);
  cl_assert(s_last_selected_item == &s_short_items[3]);

  action_menu_layer_deinit(&s_aml);
}

void test_action_menu_layer__glyph_grid_touch_activates_each_item_immediately(void) {
  prv_init_glyph_grid();
  const TouchNavWidgetOps *ops = prv_glyph_grid_touch_ops();
  for (int i = 0; i < ARRAY_LENGTH(s_emoji_items); ++i) {
    const int capacity = PBL_IF_ROUND_ELSE(7, 3);
    const int anchor = (i / capacity) * capacity + (i % capacity == 0);
    prv_set_selected_index(&s_aml, anchor, false);
    const GPoint point = prv_glyph_point(i);
    const GPoint offset = scroll_layer_get_content_offset(&s_aml.menu_layer.scroll_layer);
    prv_reset_counters();
    ops->touchdown(&s_aml.menu_layer);
    ops->tap(&s_aml.menu_layer, point);
    cl_assert_equal_i(s_aml.selected_index, i);
    cl_assert_equal_i(s_select_count, 1);
    cl_assert(s_last_selected_item == &s_emoji_items[i]);
    cl_assert(s_last_changed_item == &s_emoji_items[i]);
    const GPoint after = scroll_layer_get_content_offset(&s_aml.menu_layer.scroll_layer);
    cl_assert_equal_i(after.x, offset.x);
    cl_assert_equal_i(after.y, offset.y);

    ops->touchdown(&s_aml.menu_layer);
    ops->tap(&s_aml.menu_layer, point);
    cl_assert_equal_i(s_select_count, 2);
    cl_assert(s_last_selected_item == &s_emoji_items[i]);
  }
  action_menu_layer_deinit(&s_aml);
}

void test_action_menu_layer__glyph_grid_touch_ignores_empty_space(void) {
  prv_init_glyph_grid();
  prv_reset_counters();
  const TouchNavWidgetOps *ops = prv_glyph_grid_touch_ops();
  GRect frame;
  layer_get_global_frame(&s_aml.menu_layer.scroll_layer.layer, &frame);
  ops->tap(&s_aml.menu_layer, GPoint(frame.origin.x - 1, frame.origin.y + 24));
  ops->tap(&s_aml.menu_layer, GPoint(frame.origin.x + 1, frame.origin.y - 1));
#if PBL_ROUND
  ops->tap(&s_aml.menu_layer, GPoint(frame.origin.x + 1, frame.origin.y + 24));
  ops->tap(&s_aml.menu_layer, GPoint(frame.origin.x + frame.size.w / 2, frame.origin.y + 50));
#endif
  cl_assert_equal_i(s_aml.selected_index, 0);
  cl_assert_equal_i(s_select_count, 0);
  cl_assert_equal_i(s_selection_changed_count, 0);
  action_menu_layer_deinit(&s_aml);
}

void test_action_menu_layer__glyph_grid_touch_page_swipes(void) {
#if PBL_ROUND
  prv_init_glyph_grid();
  prv_reset_counters();
  const TouchNavWidgetOps *ops = prv_glyph_grid_touch_ops();
  const GPoint base = ops->get_base_offset(&s_aml.menu_layer);
  ops->pan_started(&s_aml.menu_layer);
  ops->pan_update(&s_aml.menu_layer, base, GPoint(0, -40));
  cl_assert_equal_i(ops->get_base_offset(&s_aml.menu_layer).y, base.y - 40);
  ops->pan_update(&s_aml.menu_layer, base, GPoint(0, -100));
  cl_assert_equal_i(s_aml.selected_index, 0);
  cl_assert_equal_i(ops->get_base_offset(&s_aml.menu_layer).y, base.y - 100);
  ops->pan_update(&s_aml.menu_layer, base, GPoint(0, -1000));
  cl_assert_equal_i(ops->get_base_offset(&s_aml.menu_layer).y,
                    base.y - s_aml.menu_layer.selection.h - 4);
  ops->pan_cancel(&s_aml.menu_layer);
  cl_assert_equal_i(s_aml.selected_index, 0);
  cl_assert_equal_i(ops->get_base_offset(&s_aml.menu_layer).y, base.y);
  ops->pan_update(&s_aml.menu_layer, base, GPoint(0, 100));
  cl_assert_equal_i(ops->get_base_offset(&s_aml.menu_layer).y, base.y);
  ops->pan_snap(&s_aml.menu_layer, base, GPoint(0, -23), GPointZero);
  cl_assert_equal_i(s_aml.selected_index, 0);
  ops->pan_snap(&s_aml.menu_layer, base, GPoint(0, -100), GPointZero);
  cl_assert_equal_i(s_aml.selected_index, 7);
  ops->pan_snap(&s_aml.menu_layer, base, GPoint(0, -100), GPointZero);
  cl_assert_equal_i(s_aml.selected_index, 14);
  ops->pan_snap(&s_aml.menu_layer, base, GPoint(0, -100), GPointZero);
  cl_assert_equal_i(s_aml.selected_index, 14);
  ops->pan_snap(&s_aml.menu_layer, base, GPoint(0, 100), GPointZero);
  cl_assert_equal_i(s_aml.selected_index, 7);
  ops->pan_snap(&s_aml.menu_layer, base, GPoint(0, 100), GPointZero);
  cl_assert_equal_i(s_aml.selected_index, 0);
  ops->pan_snap(&s_aml.menu_layer, base, GPoint(0, 100), GPointZero);
  cl_assert_equal_i(s_aml.selected_index, 0);
  cl_assert_equal_i(s_select_count, 0);
  action_menu_layer_deinit(&s_aml);
#endif
}

void test_action_menu_layer__glyph_grid_touch_flicks_settle_quickly(void) {
#if PBL_ROUND
  prv_init_glyph_grid();
  const TouchNavWidgetOps *ops = prv_glyph_grid_touch_ops();
  const GPoint base = ops->get_base_offset(&s_aml.menu_layer);
  ops->pan_started(&s_aml.menu_layer);
  ops->pan_update(&s_aml.menu_layer, base, GPoint(0, -10));
  ops->pan_snap(&s_aml.menu_layer, base, GPoint(0, -10), GPoint(0, -200));
  cl_assert_equal_i(s_aml.selected_index, 0);
  Animation *animation = property_animation_get_animation(s_aml.menu_layer.scroll_layer.animation);
  cl_assert_equal_i(animation_get_duration(animation, false, false), 160);

  ops->pan_started(&s_aml.menu_layer);
  ops->pan_snap(&s_aml.menu_layer, base, GPoint(0, -10), GPoint(0, -4000));
  cl_assert_equal_i(s_aml.selected_index, 7);
  cl_assert_equal_i(animation_get_duration(animation, false, false), 80);
  ops->pan_snap(&s_aml.menu_layer, base, GPoint(0, -10), GPoint(0, -4000));
  cl_assert_equal_i(s_aml.selected_index, 14);
  ops->pan_snap(&s_aml.menu_layer, base, GPoint(0, -10), GPoint(0, -4000));
  cl_assert_equal_i(s_aml.selected_index, 14);
  ops->pan_snap(&s_aml.menu_layer, base, GPoint(0, 10), GPoint(0, 4000));
  cl_assert_equal_i(s_aml.selected_index, 7);
  cl_assert_equal_i(s_select_count, 0);
  action_menu_layer_deinit(&s_aml);
#endif
}

void test_action_menu_layer__glyph_grid_touch_dispatch_reaches_bottom_row(void) {
  prv_init_glyph_grid();
  const int index = PBL_IF_ROUND_ELSE(6, 2);
  const GPoint point = prv_glyph_point(index);
  const TouchEvent down = {.type = TouchEvent_Touchdown, .x = point.x, .y = point.y};
  const TouchEvent up = {.type = TouchEvent_Liftoff};
  prv_reset_counters();
  touch_nav_dispatch(&down, &s_touch_nav_state);
  fake_rtc_increment_ticks(RTC_TICKS_HZ / 20);
  touch_nav_dispatch(&up, &s_touch_nav_state);
  cl_assert_equal_i(s_aml.selected_index, index);
  cl_assert_equal_i(s_select_count, 1);
  cl_assert(s_last_selected_item == &s_emoji_items[index]);
  cl_assert_equal_i(s_touch_nav_state.route, TouchNavRoute_Tier1);
#if PBL_ROUND
  prv_reset_counters();
  GRect frame;
  layer_get_global_frame(&s_aml.menu_layer.scroll_layer.layer, &frame);
  const TouchEvent swipe_down = {
    .type = TouchEvent_Touchdown,
    .x = frame.origin.x + frame.size.w / 2,
    .y = frame.origin.y + frame.size.h - 4,
  };
  TouchEvent move = {
    .type = TouchEvent_PositionUpdate,
    .x = swipe_down.x,
    .y = swipe_down.y - 20,
  };
  touch_nav_dispatch(&swipe_down, &s_touch_nav_state);
  fake_rtc_increment_ticks(RTC_TICKS_HZ / 20);
  touch_nav_dispatch(&move, &s_touch_nav_state);
  move.y -= 80;
  fake_rtc_increment_ticks(RTC_TICKS_HZ / 20);
  touch_nav_dispatch(&move, &s_touch_nav_state);
  cl_assert_equal_i(scroll_layer_get_content_offset(&s_aml.menu_layer.scroll_layer).y, -80);
  touch_nav_dispatch(&up, &s_touch_nav_state);
  cl_assert_equal_i(s_aml.selected_index, 7);
  cl_assert_equal_i(s_select_count, 0);
#endif
  action_menu_layer_deinit(&s_aml);
}
