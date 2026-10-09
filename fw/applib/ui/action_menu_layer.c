/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "action_menu_layer.h"

#include "action_menu_window_private.h"

#include <pbl/util/math.h>
#include <pbl/util/testing.h>

#include <applib/applib_malloc.auto.h>
#include <applib/fonts/fonts.h>
#include <applib/graphics/graphics.h>
#include <applib/graphics/text.h>
#include <applib/ui/animation.h>
#include <applib/ui/menu_layer.h>
#include <applib/ui/menu_layer_private.h>
#include <applib/ui/property_animation.h>
#include <applib/ui/scroll_layer_private.h>
#include <board/display.h>
#include <kernel/ui/kernel_ui.h>
#include <resource/resource_ids.auto.h>
#include <shell/system_theme.h>
#include <system/passert.h>

#define INDICATOR                "»"
#define GLYPH_SCRATCH_SIZE       32
#define GLYPH_CELL_SIZE          48
#define GLYPH_CELL_GAP           4
#define GLYPHS_PER_ROUND_PAGE    10
#define GLYPH_PAGE_SWIPE_MIN     24
#define GLYPH_PAGE_SETTLE_MIN_MS 80
#define GLYPH_PAGE_SETTLE_MAX_MS 160

// The enlarged glyph grid needs a color framebuffer and a large display.
#if !defined(CONFIG_RECOVERY_FW) && CONFIG_SCREEN_COLOR_DEPTH_BITS == 8 && DISP_COLS >= 200
#define GLYPH_GRID_SUPPORTED 1
#else
#define GLYPH_GRID_SUPPORTED 0
#endif

#if !PBL_ROUND || (!defined(CONFIG_RECOVERY_FW) && CONFIG_SCREEN_COLOR_DEPTH_BITS == 8)
static const int VERTICAL_PADDING = PBL_IF_COLOR_ELSE(2, 4);
#endif
#if CONFIG_SCREEN_COLOR_DEPTH_BITS == 1
static const int EXTRA_PADDING_1_BIT = 2;
#endif
static const int SHORT_COL_COUNT = 3;
static const int MAX_NUM_VISIBLE_LINES = 2;
#if PBL_ROUND
static const int SHORT_ITEM_MAX_ROWS_SPALDING = 3;
#endif

static GFont prv_get_item_font(void) {
  return system_theme_get_font(TextStyleFont_MenuCellTitle);
}

static bool prv_is_glyph_grid(const ActionMenuLayer *aml) {
#if GLYPH_GRID_SUPPORTED
  return aml->glyph_grid;
#else
  return false;
#endif
}

static bool prv_is_honeycomb(const ActionMenuLayer *aml) {
  return PBL_IF_ROUND_ELSE(prv_is_glyph_grid(aml), false);
}

static int prv_short_row_count(const ActionMenuLayer *aml) {
  const int capacity = prv_is_honeycomb(aml) ? GLYPHS_PER_ROUND_PAGE : SHORT_COL_COUNT;
  return (aml->num_short_items + capacity - 1) / capacity;
}

static int prv_short_row_start(const ActionMenuLayer *aml, int row) {
  if (!prv_is_honeycomb(aml)) {
    return row * SHORT_COL_COUNT;
  }
  const int pages = prv_short_row_count(aml);
  if (!pages) {
    return 0;
  }
  // Distribute the remainder across the first pages instead of leaving an orphan page.
  return row * (aml->num_short_items / pages) + MIN(row, aml->num_short_items % pages);
}

#if PBL_ROUND
//! Only used on round displays to achieve a fish-eye effect
static GFont prv_get_unfocused_item_font(void) {
  return system_theme_get_font(TextStyleFont_Header);
}
#endif

static uint16_t prv_get_num_rows(MenuLayer *menu_layer, uint16_t section_index,
                                 void *callback_context) {
  ActionMenuLayer *aml = callback_context;
  return aml->num_items + prv_short_row_count(aml);
}

static void prv_draw_scaled_glyph(GContext *ctx, const char *label, GFont font, const GRect *cell,
                                  uint8_t *pixels) {
#if CONFIG_SCREEN_COLOR_DEPTH_BITS == 8
  // Reuse the context's font cache while rendering into a small transparent tile.
  memset(pixels, 0, GLYPH_SCRATCH_SIZE * GLYPH_SCRATCH_SIZE);
  const GBitmap saved_bitmap = ctx->dest_bitmap;
  const GDrawState saved_state = ctx->draw_state;
  FrameBuffer *saved_framebuffer = ctx->parent_framebuffer;
  const GRect bounds = GRect(0, 0, GLYPH_SCRATCH_SIZE, GLYPH_SCRATCH_SIZE);
  ctx->dest_bitmap = (GBitmap){
    .addr = pixels,
    .row_size_bytes = GLYPH_SCRATCH_SIZE,
    .info = {.format = GBitmapFormat8Bit},
    .bounds = bounds,
  };
  ctx->parent_framebuffer = NULL;
  ctx->draw_state.clip_box = bounds;
  ctx->draw_state.drawing_box = bounds;
  ctx->draw_state.text_color = GColorWhite;
  graphics_draw_text(ctx, label, font, bounds, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
  ctx->dest_bitmap = saved_bitmap;
  ctx->draw_state = saved_state;
  ctx->parent_framebuffer = saved_framebuffer;

  int min_x = GLYPH_SCRATCH_SIZE;
  int min_y = GLYPH_SCRATCH_SIZE;
  int max_x = -1;
  int max_y = -1;
  for (int y = 0; y < GLYPH_SCRATCH_SIZE; ++y) {
    for (int x = 0; x < GLYPH_SCRATCH_SIZE; ++x) {
      if (pixels[y * GLYPH_SCRATCH_SIZE + x] >> 6) {
        min_x = MIN(min_x, x);
        min_y = MIN(min_y, y);
        max_x = MAX(max_x, x);
        max_y = MAX(max_y, y);
      }
    }
  }
  if (max_x < min_x) {
    return;
  }
  const GPoint origin = GPoint(cell->origin.x + (cell->size.w - 2 * (max_x - min_x + 1)) / 2,
                               cell->origin.y + (cell->size.h - 2 * (max_y - min_y + 1)) / 2);
  // Add a one-pixel edge so black glyph outlines remain visible on the menu background.
  graphics_context_set_fill_color(ctx, GColorLightGray);
  for (int y = min_y; y <= max_y; ++y) {
    for (int x = min_x; x <= max_x;) {
      if (!(pixels[y * GLYPH_SCRATCH_SIZE + x] >> 6)) {
        ++x;
        continue;
      }
      const int start = x++;
      while (x <= max_x && pixels[y * GLYPH_SCRATCH_SIZE + x] >> 6) {
        ++x;
      }
      const GRect edge = GRect(origin.x + 2 * (start - min_x) - 1, origin.y + 2 * (y - min_y) - 1,
                               2 * (x - start) + 2, 4);
      graphics_fill_rect(ctx, &edge);
    }
  }
  for (int y = min_y; y <= max_y; ++y) {
    for (int x = min_x; x <= max_x;) {
      const uint8_t color = pixels[y * GLYPH_SCRATCH_SIZE + x];
      const int start = x++;
      while (x <= max_x && pixels[y * GLYPH_SCRATCH_SIZE + x] == color) {
        ++x;
      }
      if (color >> 6) {
        graphics_context_set_fill_color(ctx, (GColor){.argb = color});
        const GRect run =
            GRect(origin.x + 2 * (start - min_x), origin.y + 2 * (y - min_y), 2 * (x - start), 2);
        graphics_fill_rect(ctx, &run);
      }
    }
  }
  ctx->draw_state = saved_state;
#endif
}

static void prv_get_honeycomb_rows(int num_items, int row_counts[3]) {
  const int middle = num_items <= 4 ? num_items : (num_items >= 8 || num_items % 2 == 0 ? 4 : 3);
  const int outer = num_items - middle;
  row_counts[0] = (outer + 1) / 2;
  row_counts[1] = middle;
  row_counts[2] = outer / 2;
}

static GRect prv_get_glyph_column_rect(const GRect *bounds, int num_items, int index,
                                       int column_width) {
  GRect cell = *bounds;
  cell.origin.x += (cell.size.w - num_items * column_width) / 2 + index * column_width;
  cell.size.w = column_width;
  return cell;
}

static void prv_draw_glyph_columns(GContext *ctx, const GRect *bounds, GFont font,
                                   ActionMenuItem *items, int num_items, int selected,
                                   int column_width, uint8_t *pixels) {
  for (int i = 0; i < num_items; ++i) {
    GRect cell = prv_get_glyph_column_rect(bounds, num_items, i, column_width);
    if (selected == i) {
      const GRect highlight = GRect(cell.origin.x + (column_width - GLYPH_CELL_SIZE) / 2,
                                    cell.origin.y, GLYPH_CELL_SIZE, GLYPH_CELL_SIZE);
      graphics_context_set_fill_color(ctx, GColorDarkGray);
      graphics_fill_round_rect(ctx, &highlight, 8, GCornersAll);
    }
    if (pixels) {
      prv_draw_scaled_glyph(ctx, items[i].label, font, &cell, pixels);
    } else {
      GRect text = cell;
      text.origin.y +=
          (cell.size.h - fonts_get_font_height(font)) / 2 - fonts_get_font_cap_offset(font);
      graphics_context_set_text_color(ctx, GColorWhite);
      graphics_draw_text(ctx, items[i].label, font, text, GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentCenter, NULL);
    }
  }
}

static void prv_cell_column_draw(GContext *ctx, struct Layer const *cell_layer,
                                 ActionMenuLayer *aml, ActionMenuItem *items, int num_items,
                                 int sel_idx) {
  const GFont font = aml->layout_cache.font;
  const int16_t font_height = fonts_get_font_height(font);
  const GRect *layer_bounds = &cell_layer->bounds;
  GRect r = *layer_bounds;
  if (prv_is_glyph_grid(aml)) {
    uint8_t *pixels = applib_malloc(GLYPH_SCRATCH_SIZE * GLYPH_SCRATCH_SIZE);
    if (prv_is_honeycomb(aml)) {
      // Keep a packed, staggered page with the widest row at screen center.
      const int column_width = (r.size.w - 16) / 4;
      int row_counts[3];
      prv_get_honeycomb_rows(num_items, row_counts);
      int first = 0;
      r.size.h = GLYPH_CELL_SIZE;
      for (int row = 0; row < 3; ++row) {
        const int count = row_counts[row];
        if (count) {
          prv_draw_glyph_columns(ctx, &r, font, &items[first], count, sel_idx - first, column_width,
                                 pixels);
        }
        first += count;
        r.origin.y += GLYPH_CELL_SIZE + GLYPH_CELL_GAP;
      }
    } else {
      const int column_width = r.size.w / SHORT_COL_COUNT;
      prv_draw_glyph_columns(ctx, &r, font, items, num_items, sel_idx, column_width, pixels);
    }
    applib_free(pixels);
    return;
  }
#if PBL_ROUND
  // more narrow on round
  r = grect_inset_internal(r, 25, 0);
  // center the columns horizontally if there's only one row
  const bool is_single_short_row = aml->num_short_items <= SHORT_COL_COUNT;
  r.size.w /= is_single_short_row ? num_items : SHORT_COL_COUNT;
#else
  r.size.w /= SHORT_COL_COUNT;
#endif
  r.origin.y += (r.size.h - font_height) / 2 - 4;

  for (int i = 0; i < num_items; i++) {
    if (!items[i].label) {
      break;
    }

    if (sel_idx == i) {
      graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
#if CONFIG_SCREEN_COLOR_DEPTH_BITS == 1
      // We only want to have a background on non-color platforms, while leaving this in with
      // a PBL_IF_COLOR_ELSE makes this a no-op, we'll save some cycles and code space just
      // skipping it.
      graphics_context_set_fill_color(ctx, GColorWhite);

      const int16_t y_offset = 1;
      const int16_t padding = r.size.w / 6;
      const uint16_t corner_radius = 4;
      GRect bg_rect = r;
      bg_rect.origin.y = layer_bounds->origin.y;
      bg_rect.size.h = layer_bounds->size.h;
      bg_rect = grect_inset_internal(bg_rect, padding, y_offset);
      graphics_fill_round_rect(ctx, &bg_rect, corner_radius, GCornersAll);
#endif
    } else {
      graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite));
    }

    graphics_draw_text(ctx, items[i].label, font, r, GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    r.origin.x += r.size.w;
  }
}

static const ActionMenuItem *prv_get_item_for_index(ActionMenuLayer *aml, int idx) {
  if (!aml->num_items && !aml->num_short_items) {
    return NULL;
  }

  PBL_ASSERTN(idx >= 0);

  if (idx < aml->num_items) {
    return &aml->items[idx];
  } else {
    const int short_items_idx = idx - aml->num_items;
    PBL_ASSERTN(short_items_idx < aml->num_short_items);
    return &aml->short_items[short_items_idx];
  }
}

static int16_t prv_get_item_line_height(ActionMenuLayer *aml, int idx) {
  const GFont font = aml->layout_cache.font;
  const ActionMenuItem *item = prv_get_item_for_index(aml, idx);
  GRect box = menu_layer_get_layer(&aml->menu_layer)->bounds;
  // In calculating the item line height for round displays, we need to horizontally inset by the
  // standard focused cell inset since that's the horizontal inset of the cells where we show
  // the vertical scrolling animation of long text cells (where the height is crucial to be correct)
  const int inset = PBL_IF_ROUND_ELSE(MENU_CELL_ROUND_FOCUSED_HORIZONTAL_INSET,
                                      menu_cell_basic_horizontal_inset());
  // Tintin has a rounded rectangle highlight
  box = grect_inset_internal(box, PBL_IF_COLOR_ELSE(inset, 2 * inset), 0);

  GContext *ctx = graphics_context_get_current_context();
  // On rectangular displays, if the indicator is present, the indicator also will be offset,
  // so we add another nudge between the text and the indicator.
#if PBL_RECT
  const int nudge = menu_cell_basic_horizontal_inset();
  if (!item->is_leaf) {
    const GSize indicator_size = graphics_text_layout_get_max_used_size(
        ctx, INDICATOR, font, box, GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
    box.size.w -= (indicator_size.w + nudge);
  }
#endif
  return graphics_text_layout_get_text_height(
      ctx, item->label, font, box.size.w, GTextOverflowModeWordWrap,
      PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
}

// Item Scroll Animation
///////////////////////////////////

static int16_t prv_get_cell_offset(void *subject) {
  ActionMenuLayer *aml = subject;
  return aml->item_animation.current_offset_y;
}

PBL_T_STATIC void prv_set_cell_offset(void *subject, int16_t value) {
  ActionMenuLayer *aml = subject;
  aml->item_animation.current_offset_y = value;
  layer_mark_dirty(&aml->layer);
}

static void prv_cell_animation_stopped_handler(Animation *animation, bool finished, void *context) {
  ActionMenuLayer *aml = context;
  if (finished) {
    prv_set_cell_offset(aml, aml->item_animation.bottom_offset_y);
  }
}

static const PropertyAnimationImplementation s_item_animation_implementation = {
  .base = {.update = (AnimationUpdateImplementation)property_animation_update_int16},
  .accessors = {.setter = {.int16 = prv_set_cell_offset}, .getter = {.int16 = prv_get_cell_offset}}
};

static void prv_unschedule_item_animation(ActionMenuLayer *aml) {
  animation_unschedule(aml->item_animation.animation);
  aml->item_animation.animation = NULL;
}

static void prv_animate_cell(ActionMenuLayer *aml, GRect *label_text_frame, bool *draw_top_shading,
                             bool *draw_bottom_shading) {
  // Check to see if this item spans more than max number of visible lines,
  // in which case we want to make it scroll.
  const int16_t item_height = aml->layout_cache.item_heights[aml->selected_index];
  const int16_t line_height = fonts_get_font_height(aml->layout_cache.font);

#if CONFIG_SCREEN_COLOR_DEPTH_BITS == 1
  // We need to force it to scroll a little extra for 1 bit
  label_text_frame->origin.y -= EXTRA_PADDING_1_BIT;
#endif
  // On rect displays, calculate the visible item height based on a desired number of visible lines
  // On round displays, use the height of the provided box since it might be inset for the indicator
  const int16_t max_visible_item_height =
      PBL_IF_RECT_ELSE(MAX_NUM_VISIBLE_LINES * line_height, label_text_frame->size.h);
  if (item_height > max_visible_item_height) {
    // Compute the limit at which we should bounce back to the top of the layer.  Since
    // there are at most MAX_NUM_VISIBLE_LINES shown at a given time, we want to stop
    // when there are that number of lines in view and no more lines remaining below.
    const int16_t max_scroll_distance = item_height - max_visible_item_height;
    ActionMenuItemAnimation *item_animation = &aml->item_animation;
    if (item_animation->animation == NULL) {
      const int16_t DELAY_PER_LINE = 600; /* milliseconds to delay per line */

      // Top offset represents when the text has scrolled to its minimum y value so the last line of
      // text is visible. Bottom offset represents when the text has scrolled all the way to its
      // maximum y so the first line of text is visible.
      item_animation->top_offset_y = -max_scroll_distance;
      item_animation->bottom_offset_y = 0;
      item_animation->current_offset_y = 0;

      // Create the animation that will scroll us up in the cell
      PropertyAnimation *animation = property_animation_create(
          &s_item_animation_implementation, (void *)aml, NULL, &item_animation->top_offset_y);

      animation_set_duration((Animation *)animation, DELAY_PER_LINE * (item_height / line_height));
      animation_set_curve((Animation *)animation, AnimationCurveLinear);
      animation_set_handlers((Animation *)animation, (AnimationHandlers){}, aml);

      // Create the animation that stalls when we have auto-scrolled up completely
      PropertyAnimation *s_animation =
          property_animation_create(&s_item_animation_implementation, (void *)aml,
                                    &item_animation->top_offset_y, &item_animation->top_offset_y);

      animation_set_duration((Animation *)s_animation, DELAY_PER_LINE /* ms to wait */);
      animation_set_handlers((Animation *)s_animation, (AnimationHandlers){}, aml);

      // Create the reverse animation that takes us from the scrolled up position back down
      PropertyAnimation *r_animation = property_animation_create(
          &s_item_animation_implementation, (void *)aml, &item_animation->top_offset_y,
          &item_animation->bottom_offset_y);

      animation_set_duration((Animation *)r_animation,
                             (DELAY_PER_LINE / 4) * (item_height / line_height));
      animation_set_curve((Animation *)r_animation, AnimationCurveEaseInOut);
      animation_set_handlers((Animation *)r_animation, (AnimationHandlers){}, aml);

      item_animation->animation = animation_sequence_create(
          (Animation *)animation, (Animation *)s_animation, (Animation *)r_animation);

      animation_set_handlers(item_animation->animation,
                             (AnimationHandlers){.stopped = prv_cell_animation_stopped_handler},
                             aml);
      animation_set_play_count(item_animation->animation, PLAY_COUNT_INFINITE);
      animation_set_delay(item_animation->animation, DELAY_PER_LINE /* ms */);
      animation_schedule(item_animation->animation);
    }
    *draw_top_shading = (item_animation->current_offset_y != item_animation->bottom_offset_y);
    *draw_bottom_shading = (item_animation->current_offset_y != item_animation->top_offset_y);

    // update the rect height and offset based on the current animation state
    label_text_frame->origin.y += item_animation->current_offset_y;
    label_text_frame->size.h = item_height;
  }
}

// Menu Layer Drawing Routines
///////////////////////////////

static bool prv_should_center(ActionMenuLayer *aml) {
  // We only center an ActionMenuLayer's items if the user has specified to
  // center the items or there is only one item in the ActionMenuLayer.
  if (aml->num_items == 1 || aml->layout_cache.align == ActionMenuAlignCenter) {
    return true;
  }
  return false;
}

#if PBL_RECT
static void prv_cell_item_content_draw_rect(GContext *ctx, const Layer *cell_layer,
                                            const ActionMenuLayer *aml, const ActionMenuItem *item,
                                            bool selected, GRect *content_box) {
  char *indicator = NULL;
  const int16_t horizontal_padding = menu_cell_basic_horizontal_inset();
  const GFont font = aml->layout_cache.font;
  if (!item->is_leaf) {
    // If an item is not a leaf, then there would be an indicator when it is focused.  Either
    // we draw the indicator or we force the box to be smaller to force the text to render as
    // if the indicator was present in case it would line wrap.
    if (selected) {
      indicator = INDICATOR;
    } else {
      const GSize indicator_size = graphics_text_layout_get_max_used_size(
          ctx, INDICATOR, font, *content_box, GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
      content_box->size.w -= (indicator_size.w + (2 * horizontal_padding));
    }
  } else {
    content_box->size.w -= horizontal_padding;
  }

#if CONFIG_SCREEN_COLOR_DEPTH_BITS == 1
  // Fill in the background layer.  This effectively does nothing on watches where we have the
  // ability to draw with color, but on others, it will render a background behind the selected
  // cell.
  const int x_offset = horizontal_padding;
  const int y_padding = EXTRA_PADDING_1_BIT;
  const uint16_t corner_radius = 4;
  GRect bg_box = grect_inset_internal(cell_layer->bounds, x_offset, 0);
  bg_box.size.h -= y_padding;
  graphics_fill_round_rect(ctx, &bg_box, corner_radius, GCornersAll);
  // We have to adjust the box to compensate for the padding we added.  Note that we can't call
  // inset as it will discard our offset when it standardizes.
  content_box->origin.x += x_offset;
  content_box->size.w -= (2 * x_offset);
  content_box->size.h -= (2 * y_padding);
#endif

  // Cast the cell layer so we can briefly modify its bounds. We do this because we're
  // desperate for stack space and we understand the call hierarchy. We'll restore the state below.
  Layer *mutable_cell_layer = (Layer *)cell_layer;
  const GRect saved_bounds = mutable_cell_layer->bounds;
  mutable_cell_layer->bounds = *content_box;

  // Draw the menu cell specifying that we're allowing word wrapping
  const GTextOverflowMode overflow_mode = GTextOverflowModeWordWrap;
  menu_cell_basic_draw_custom(ctx, mutable_cell_layer, font, item->label, font, indicator, font,
                              NULL, NULL, false, overflow_mode);

  // Restore the cell layer's bounds
  mutable_cell_layer->bounds = saved_bounds;
}
#endif

#if PBL_ROUND
static void prv_cell_item_content_draw_round(GContext *ctx, const Layer *cell_layer,
                                             const ActionMenuLayer *aml, const ActionMenuItem *item,
                                             bool selected, GRect *content_box) {
  const int16_t horizontal_inset = selected ? MENU_CELL_ROUND_FOCUSED_HORIZONTAL_INSET
                                            : MENU_CELL_ROUND_UNFOCUSED_HORIZONTAL_INSET;
  *content_box = grect_inset(*content_box, GEdgeInsets(0, horizontal_inset));

  // Use a smaller font for the unfocused cells to achieve a fish-eye effect
  const GFont font = selected ? aml->layout_cache.font : prv_get_unfocused_item_font();
  const GTextOverflowMode overflow_mode =
      selected ? GTextOverflowModeWordWrap : GTextOverflowModeTrailingEllipsis;
  const GTextAlignment text_alignment = GTextAlignmentCenter;
  const GSize text_size = graphics_text_layout_get_max_used_size(
      ctx, item->label, font, *content_box, overflow_mode, text_alignment, NULL);
  GRect text_box = (GRect){.size = text_size};
  const GAlign item_label_text_alignment = GAlignCenter;
  grect_align(&text_box, content_box, item_label_text_alignment, true /* clip */);
  text_box.origin.y -= fonts_get_font_cap_offset(font);

  graphics_draw_text(ctx, item->label, font, text_box, overflow_mode, text_alignment, NULL);
}

static int16_t prv_get_indicator_height(const ActionMenuLayer *aml) {
  // This magic factor is an approximation of the indicator height in relation to the font line
  // height; it Just Works(tm)
  return fonts_get_font_height(aml->layout_cache.font) * 40 / 100;
}

static void prv_draw_indicator_round(GContext *ctx, const ActionMenuLayer *aml,
                                     const GRect *label_text_container) {
  const int indicator_height = fonts_get_font_height(aml->layout_cache.font);
  const int text_height = aml->layout_cache.item_heights[aml->selected_index];
  const int content_height = MIN(label_text_container->size.h, text_height + indicator_height);

  GRect content_frame = (GRect){.size = GSize(label_text_container->size.w, content_height)};
  GRect indicator_frame = (GRect){.size = GSize(label_text_container->size.w, indicator_height)};

  grect_align(&content_frame, label_text_container, GAlignCenter, true);
  grect_align(&indicator_frame, &content_frame, GAlignBottom, true);

  graphics_draw_text(ctx, INDICATOR, aml->layout_cache.font, indicator_frame,
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}
#endif

static void prv_cell_item_draw(GContext *ctx, const Layer *cell_layer, ActionMenuLayer *aml,
                               const ActionMenuItem *item, bool selected) {
  GRect label_text_container = cell_layer->bounds;
  // bottom_inset won't be used on black and white, using [[maybe_unused]] here quiets the linter
  [[maybe_unused]] int16_t bottom_inset = 0;
#if PBL_ROUND
  // On round displays, inset the box from the bottom to account for drawing the indicator at the
  // bottom center, and then draw the indicator
  const bool selected_with_indicator = (selected && !item->is_leaf);
  if (selected_with_indicator) {
    prv_draw_indicator_round(ctx, aml, &label_text_container);

    const int16_t indicator_text_margin = 7;
    bottom_inset = prv_get_indicator_height(aml) + indicator_text_margin;
    label_text_container.size.h -= bottom_inset;
  }
#endif

  GRect label_text_frame = label_text_container;
  bool draw_top_shading = false;
  bool draw_bottom_shading = false;
  // If we are the selected index, check to see if we have started scrolling.
  // If we have, use our internal box to draw the layer, otherwise use the
  // layer box.
  if (selected) {
    prv_animate_cell(aml, &label_text_frame, &draw_top_shading, &draw_bottom_shading);
#if !defined(CONFIG_RECOVERY_FW) && CONFIG_SCREEN_COLOR_DEPTH_BITS == 8
    // Replace the clip box with a clip box that will render the item in the right place with the
    // right size, without menu layer's selection clipping. Menu layer will responsible for cleaning
    // up the changes made to this clip box.
    ctx->draw_state.clip_box.origin = ctx->draw_state.drawing_box.origin;
    ctx->draw_state.clip_box.size = cell_layer->bounds.size;
    // We have to update the clip box of the drawing state to account for text padding to
    // force it to clip around the shadow.
    if (draw_top_shading) {
      ctx->draw_state.clip_box.origin.y += VERTICAL_PADDING;
      ctx->draw_state.clip_box.size.h -= VERTICAL_PADDING;
    }
    if (draw_bottom_shading) {
      ctx->draw_state.clip_box.size.h -= VERTICAL_PADDING + bottom_inset;
    }
    // Prevent drawing outside of the context bitmap
    grect_clip(&ctx->draw_state.clip_box, &ctx->dest_bitmap.bounds);
#endif
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
  }

  PBL_IF_RECT_ELSE(prv_cell_item_content_draw_rect, prv_cell_item_content_draw_round)(
      ctx, cell_layer, aml, item, selected, &label_text_frame);

#if !defined(CONFIG_RECOVERY_FW) && CONFIG_SCREEN_COLOR_DEPTH_BITS == 8
  const int16_t fade_height = 10;
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  if (draw_top_shading) {
    GRect top_bounds = label_text_container;
    top_bounds.origin.y += VERTICAL_PADDING;
    top_bounds.size.h = fade_height;
    graphics_draw_bitmap_in_rect(ctx, &aml->item_animation.fade_top, &top_bounds);
  }

  if (draw_bottom_shading) {
    GRect bottom_bounds = label_text_container;
    bottom_bounds.size.h = fade_height;
    bottom_bounds.origin.y =
        grect_get_max_y(&label_text_container) - (fade_height + VERTICAL_PADDING);
    graphics_draw_bitmap_in_rect(ctx, &aml->item_animation.fade_bottom, &bottom_bounds);
  }
#endif
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                         void *callback_context) {
  ActionMenuLayer *aml = callback_context;

  if (cell_index->row < aml->num_items) {
    const ActionMenuItem *item = prv_get_item_for_index(aml, cell_index->row);
    const bool selected = menu_layer_is_index_selected(&aml->menu_layer, cell_index);
    prv_cell_item_draw(ctx, cell_layer, aml, item, selected);
  } else {
    const int short_row = cell_index->row - aml->num_items;
    const int base_idx = prv_short_row_start(aml, short_row);
    const int sel_idx = aml->selected_index - (base_idx + aml->num_items);
    const int num_items =
        MIN(aml->num_short_items, prv_short_row_start(aml, short_row + 1)) - base_idx;
    prv_cell_column_draw(ctx, cell_layer, aml, (ActionMenuItem *)&aml->short_items[base_idx],
                         num_items, sel_idx);
  }
}

static int prv_get_menu_layer_row(ActionMenuLayer *aml, int item_index) {
  if (item_index < aml->num_items) {
    return item_index;
  } else {
    if (!prv_is_honeycomb(aml)) {
      return aml->num_items + (item_index - aml->num_items) / SHORT_COL_COUNT;
    }
    int row = 0;
    const int short_index = item_index - aml->num_items;
    while (row + 1 < prv_short_row_count(aml) && short_index >= prv_short_row_start(aml, row + 1)) {
      ++row;
    }
    return aml->num_items + row;
  }
}

static void prv_selection_changed(ActionMenuLayer *aml) {
  const ActionMenuItem *item = prv_get_item_for_index(aml, aml->selected_index);
  if (item && aml->callbacks.selection_changed) {
    aml->callbacks.selection_changed(item, aml->context);
  }
}

static void prv_set_selected_index_with_align(ActionMenuLayer *aml, int new_selected_index,
                                              MenuRowAlign align, bool animated) {
  new_selected_index = CLIP(new_selected_index, 0, aml->num_items + aml->num_short_items - 1);
  const bool selection_changed = (new_selected_index != aml->selected_index);

  if (selection_changed) {
    // Unschedule any running item animation but don't NULL the pointer, to prevent another
    // animation from being accidentally re-scheduled.
    animation_unschedule(aml->item_animation.animation);
  }

  if (new_selected_index >= aml->num_items) {
    // For short columns, aml->selected_index needs to be updated here, because the column index
    // will be lost in the menu layer selection changed callback. Otherwise, it will be updated
    // in prv_selection_changed_cb() to ensure the correct index is used by the draw functions.
    aml->selected_index = new_selected_index;
  }

  const int menu_layer_index = prv_get_menu_layer_row(aml, new_selected_index);
  menu_layer_set_selected_index(&aml->menu_layer, MenuIndex(0, menu_layer_index), align, animated);
  if (selection_changed && new_selected_index >= aml->num_items) {
    prv_selection_changed(aml);
  }
}

PBL_T_STATIC void prv_set_selected_index(ActionMenuLayer *aml, int new_selected_index,
                                         bool animated) {
  prv_set_selected_index_with_align(aml, new_selected_index, MenuRowAlignCenter, animated);
}

static void prv_scroll_handler(ClickRecognizerRef recognizer, void *context) {
  ActionMenuLayer *aml = context;
  const bool up = (click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP);
  const int new_idx = aml->selected_index + (up ? -1 : 1);
  prv_set_selected_index(aml, new_idx, true /* animated */);
}

static void prv_activate_selection(ActionMenuLayer *aml) {
  const ActionMenuItem *item = prv_get_item_for_index(aml, aml->selected_index);
  if (item && aml->callbacks.select) {
    aml->callbacks.select(item, aml->context);
  }
}

static void prv_select_handler(ClickRecognizerRef recognizer, void *context) {
  prv_activate_selection(context);
}

static void prv_select_click_cb(struct MenuLayer *menu_layer, MenuIndex *cell_index,
                                void *callback_context) {
  // Touch tap activation. The AML's own selected_index identifies the item (a short-item menu row
  // holds several columns), matching the SELECT button path.
  prv_activate_selection(callback_context);
}

#ifdef CONFIG_TOUCH
static int prv_touch_find_glyph(ActionMenuLayer *aml, GPoint point_on_screen) {
  MenuLayer *menu = &aml->menu_layer;
  GRect frame;
  layer_get_global_frame(&menu->scroll_layer.layer, &frame);
  if (!grect_contains_point(&frame, &point_on_screen)) {
    return -1;
  }
  GPoint point = point_on_screen;
  gpoint_sub_eq(&point, frame.origin);
  gpoint_sub_eq(&point, scroll_layer_get_content_offset(&menu->scroll_layer));
  MenuCellSpan span;
  if (!menu_layer_touch_find_cell_at_content_y(menu, point.y, &span) ||
      span.index.row < aml->num_items) {
    return -1;
  }
  const int short_row = span.index.row - aml->num_items;
  const int base = prv_short_row_start(aml, short_row);
  const int count = MIN(aml->num_short_items, prv_short_row_start(aml, short_row + 1)) - base;
  const bool round = prv_is_honeycomb(aml);
  const int column_width = round ? (frame.size.w - 16) / 4 : frame.size.w / SHORT_COL_COUNT;
  int row_counts[3] = {count};
  if (round) {
    prv_get_honeycomb_rows(count, row_counts);
  }
  GRect row = GRect(0, span.y, frame.size.w, GLYPH_CELL_SIZE);
  int first = 0;
  for (int r = 0; r < (round ? 3 : 1); ++r) {
    for (int column = 0; column < row_counts[r]; ++column) {
      const GRect cell = prv_get_glyph_column_rect(&row, row_counts[r], column, column_width);
      if (grect_contains_point(&cell, &point)) {
        return aml->num_items + base + first + column;
      }
    }
    first += row_counts[r];
    row.origin.y += GLYPH_CELL_SIZE + GLYPH_CELL_GAP;
  }
  return -1;
}

static void prv_touch_tap(void *widget, GPoint point_on_screen) {
  MenuLayer *menu = widget;
  ActionMenuLayer *aml = menu->callback_context;
  GRect frame;
  layer_get_global_frame(&menu->scroll_layer.layer, &frame);
  if (!grect_contains_point(&frame, &point_on_screen)) {
    return;
  }
  if (!prv_is_glyph_grid(aml)) {
    menu_layer_touch_handle_tap(menu, point_on_screen);
    return;
  }
  if (menu->touch_tap_swallow) {
    menu->touch_tap_swallow = false;
    return;
  }
  const int index = prv_touch_find_glyph(aml, point_on_screen);
  if (index < 0) {
    return;
  }
  prv_set_selected_index_with_align(aml, index, MenuRowAlignNone, false);
  prv_activate_selection(aml);
}

static void prv_touch_down(void *widget) {
  menu_layer_touch_handle_touchdown(widget);
}

static void prv_touch_pan_started(void *widget) {
  menu_layer_touch_get_default_ops()->pan_started(widget);
}

static GPointReturn prv_touch_get_base(void *widget) {
  return scroll_layer_get_content_offset(&((MenuLayer *)widget)->scroll_layer);
}

static void prv_touch_pan_update(void *widget, GPoint base, GPoint delta) {
  MenuLayer *menu = widget;
  if (!prv_is_honeycomb(menu->callback_context)) {
    menu_layer_touch_handle_pan_update(menu, base, delta);
  } else {
    // Follow the finger, with at most one page traversed per gesture.
    const int stride = menu->selection.h + GLYPH_CELL_GAP;
    const int y = base.y + CLIP(delta.y, -stride, stride);
    scroll_layer_set_content_offset(&menu->scroll_layer, GPoint(0, y), false);
  }
}

static void prv_touch_pan_snap(void *widget, GPoint base, GPoint delta, GPoint velocity) {
  MenuLayer *menu = widget;
  ActionMenuLayer *aml = menu->callback_context;
  if (!prv_is_honeycomb(aml)) {
    menu_layer_touch_handle_snap(menu, base, delta, velocity);
    return;
  }
  if (!aml->num_short_items) {
    return;
  }
  const int page = prv_get_menu_layer_row(aml, aml->selected_index) - aml->num_items;
  const bool flick = ABS((int32_t)velocity.y) >= TOUCH_FLING_MIN_VELOCITY_PX_S;
  const int direction = flick ? velocity.y : delta.y;
  const int next_page = page + (direction < 0 ? 1 : -1);
  int index = aml->selected_index;
  if ((ABS(delta.y) >= GLYPH_PAGE_SWIPE_MIN || flick) && next_page >= 0 &&
      next_page < prv_short_row_count(aml)) {
    index = aml->num_items + prv_short_row_start(aml, next_page);
  }
  prv_set_selected_index_with_align(aml, index, MenuRowAlignNone, false);
  const GPoint target = GPoint(
      0, (menu->scroll_layer.layer.frame.size.h - menu->selection.h) / 2 - menu->selection.y);
  const int distance = ABS(target.y - scroll_layer_get_content_offset(&menu->scroll_layer).y);
  const int duration = flick ? CLIP(2000 * distance / ABS((int32_t)velocity.y),
                                    GLYPH_PAGE_SETTLE_MIN_MS, GLYPH_PAGE_SETTLE_MAX_MS)
                             : GLYPH_PAGE_SETTLE_MAX_MS;
  scroll_layer_touch_settle(&menu->scroll_layer, target, duration);
}

static void prv_touch_pan_cancel(void *widget) {
  MenuLayer *menu = widget;
  ActionMenuLayer *aml = menu->callback_context;
  if (prv_is_honeycomb(aml)) {
    if (aml->num_short_items) {
      prv_set_selected_index(aml, aml->selected_index, false);
    }
  } else {
    menu_layer_touch_handle_cancel(menu);
  }
}

static void prv_touch_swipe(void *widget, SwipeDirection direction) {
  menu_layer_touch_handle_swipe(widget, direction);
}

static const TouchNavWidgetOps s_touch_ops = {
  .touchdown = prv_touch_down,
  .pan_started = prv_touch_pan_started,
  .get_base_offset = prv_touch_get_base,
  .pan_update = prv_touch_pan_update,
  .pan_snap = prv_touch_pan_snap,
  .pan_cancel = prv_touch_pan_cancel,
  .tap = prv_touch_tap,
  .swipe = prv_touch_swipe,
};
#endif

static bool prv_aml_is_short(ActionMenuLayer *aml) {
  return (aml->num_short_items != 0 || aml->num_items == 0);
}

static int16_t prv_get_cell_padding(ActionMenuLayer *aml) {
  if (prv_is_glyph_grid(aml)) {
    return GLYPH_CELL_GAP;
  }
  const int16_t default_sep_height = 10;
#if PBL_ROUND
  // when showing columns, set cells further apart
  return prv_aml_is_short(aml) ? default_sep_height : 1;
#elif CONFIG_SCREEN_COLOR_DEPTH_BITS == 1
  return default_sep_height;
#else
  const int16_t line_height = fonts_get_font_height(aml->layout_cache.font);
  const int16_t sep_height =
      MAX(menu_cell_small_cell_height() - line_height, default_sep_height) + 1;
  return sep_height;
#endif
}

static int16_t prv_get_cell_height_cb(struct MenuLayer *menu_layer, MenuIndex *cell_index,
                                      void *context) {
  ActionMenuLayer *aml = (ActionMenuLayer *)context;
  const int16_t line_height = fonts_get_font_height(aml->layout_cache.font);
  // If we have short items, just return the line height.
  if (prv_aml_is_short(aml)) {
    if (prv_is_glyph_grid(aml)) {
      const int rows = PBL_IF_ROUND_ELSE(3, 1);
      return rows * GLYPH_CELL_SIZE + (rows - 1) * GLYPH_CELL_GAP;
    }
    return line_height;
  }

#if PBL_ROUND
  return menu_layer_is_index_selected(menu_layer, cell_index)
             ? MENU_CELL_ROUND_FOCUSED_SHORT_CELL_HEIGHT
             : MENU_CELL_ROUND_UNFOCUSED_TALL_CELL_HEIGHT;
#else
  const int16_t max_visible_height = line_height * MAX_NUM_VISIBLE_LINES;
  const int16_t actual_height = aml->layout_cache.item_heights[cell_index->row];
  return (VERTICAL_PADDING * 2) + MIN(max_visible_height, actual_height);
#endif
}

static int16_t prv_get_separator_height_cb(struct MenuLayer *menu_layer, MenuIndex *cell_index,
                                           void *callback_context) {
  // We use the separator to pad the cells (insert spacing), so we compute the height
  // needed for each separator here.
  ActionMenuLayer *aml = callback_context;
  return prv_get_cell_padding(aml);
}

typedef struct ActionMenuSeparatorConfig {
  GSize separator;
} ActionMenuSeparatorConfig;

static const ActionMenuSeparatorConfig s_separator_configs[NumPreferredContentSizes] = {
  [PreferredContentSizeSmall] =
      {
        .separator = {100, 1},
      },
  [PreferredContentSizeMedium] =
      {
        .separator = {100, 1},
      },
  [PreferredContentSizeLarge] =
      {
        .separator = {162, 2},
      },
  [PreferredContentSizeExtraLarge] = {
    .separator = {162, 2},
  },
};

static void prv_draw_separator_cb(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                                  void *callback_context) {
  ActionMenuLayer *aml = callback_context;
  if (aml->separator_index && cell_index->row == aml->separator_index) {
    const PreferredContentSize runtime_platform_default_size =
        system_theme_get_default_content_size_for_runtime_platform();
    const ActionMenuSeparatorConfig *config = &s_separator_configs[runtime_platform_default_size];

    // If this index is the separator index, we want to draw the separator line
    // in the vertical center of the separator
    const int16_t nudge_down = PBL_IF_RECT_ELSE(3, 0);

    const int16_t separator_width = config->separator.w;
    const GRect *cell_layer_bounds = &cell_layer->bounds;
    const int16_t offset_x = PBL_IF_RECT_ELSE(menu_cell_basic_horizontal_inset() + 1,
                                              (cell_layer->bounds.size.w - separator_width) / 2);
    const int16_t offset_y = (cell_layer_bounds->size.h / 2) + nudge_down;
    GPoint separator_start_point =
        gpoint_add(cell_layer_bounds->origin, GPoint(offset_x, offset_y));
    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite));

    separator_start_point.y += config->separator.h;
    for (int i = 0; i < config->separator.h; i++) {
      // First point from bottom will be +0, second +1, third +0, etc.
      separator_start_point.y--;
      separator_start_point.x += i & 1;
      graphics_draw_horizontal_line_dotted(ctx, separator_start_point, separator_width);
      separator_start_point.x -= i & 1;
    }
  }
}

static int16_t prv_get_header_height_cb(struct MenuLayer *menu_layer, uint16_t second_index,
                                        void *callback_context) {
  ActionMenuLayer *aml = callback_context;
  if (!prv_should_center(aml) || prv_aml_is_short(aml) || aml->num_items == 0) {
    return 0;
  }

  const int16_t line_height = fonts_get_font_height(aml->layout_cache.font);
  const int16_t padding = prv_get_cell_padding(aml);
  const int16_t max_visible_height = line_height * MAX_NUM_VISIBLE_LINES;

  const GRect *bounds = &aml->layer.bounds;
  int16_t total_h = 0;

  for (int16_t idx = 0; idx < aml->num_items; idx++) {
    int16_t item_height = aml->layout_cache.item_heights[idx];
    total_h += MIN(max_visible_height, item_height);
  }

  const int16_t header_padding = 6 * aml->num_items;
  const int16_t header_height = ((bounds->size.h - total_h) / 2) - padding;
  return MAX(header_height - header_padding, 0);
}

static void prv_draw_header_cb(GContext *ctx, const Layer *cell_layer, uint16_t section_index,
                               void *callback_context) {
  // The header here is just being used for padding, so we don't actually need to draw anything.
  return;
}

static void prv_selection_changed_cb(struct MenuLayer *menu_layer, MenuIndex new_index,
                                     MenuIndex old_index, void *callback_context) {
  ActionMenuLayer *aml = callback_context;
  if (new_index.row < aml->num_items) {
    // Enable a new item animation to be scheduled
    prv_unschedule_item_animation(aml);
    aml->selected_index = new_index.row;
    prv_selection_changed(aml);
  } else if (prv_get_menu_layer_row(aml, aml->selected_index) != new_index.row) {
    // A touch tap moves the menu selection directly, bypassing prv_set_selected_index, so no
    // column index was pre-set for this short-item row; adopt its first column.
    prv_unschedule_item_animation(aml);
    aml->selected_index = aml->num_items + prv_short_row_start(aml, new_index.row - aml->num_items);
    prv_selection_changed(aml);
  }
}

static void prv_changed_proc(Layer *layer) {
  ActionMenuLayer *aml = (ActionMenuLayer *)layer;
  const GRect *aml_bounds = &layer->bounds;
  GRect menu_layer_frame = *aml_bounds;
  if (prv_is_honeycomb(aml)) {
    MenuIndex index = MenuIndex(0, 0);
    menu_layer_frame.size.h = prv_get_cell_height_cb(&aml->menu_layer, &index, aml);
    grect_align(&menu_layer_frame, aml_bounds, GAlignCenter, true);
  }
#if PBL_ROUND
  else if (prv_aml_is_short(aml)) {
    // clip the menu layer to show exactly SHORT_ITEM_MAX_ROWS_SPALDING lines at a time
    MenuIndex index = MenuIndex(0, 0);
    const int16_t font_height = prv_get_cell_height_cb(&aml->menu_layer, &index, aml);
    const int16_t cell_padding = prv_get_cell_padding(aml);
    const int num_visible_rows =
        MIN(prv_get_num_rows(&aml->menu_layer, 0, aml), SHORT_ITEM_MAX_ROWS_SPALDING);
    menu_layer_frame.size.h =
        (font_height * num_visible_rows) + (cell_padding * (num_visible_rows - 1));
    grect_align(&menu_layer_frame, aml_bounds, GAlignCenter, true /* clip */);
  }
#endif
  layer_set_frame(menu_layer_get_layer(&aml->menu_layer), &menu_layer_frame);
}

static void prv_update_proc(Layer *layer, GContext *ctx) {
#if PBL_ROUND
  ActionMenuLayer *aml = (ActionMenuLayer *)layer;
  const int num_rows = prv_get_num_rows(&aml->menu_layer, 0, aml);
  const bool paged = prv_is_honeycomb(aml);
  const int max_visible_rows = SHORT_ITEM_MAX_ROWS_SPALDING;
  if (prv_aml_is_short(aml) && (num_rows > (paged ? 1 : max_visible_rows))) {
    // draw some "content indicator" arrows
    const GRect *aml_bounds = &layer->bounds;
    const GRect *menu_layer_frame = &menu_layer_get_layer(&aml->menu_layer)->frame;
    const int16_t arrow_layer_height = (aml_bounds->size.h - menu_layer_frame->size.h) / 2;

    const int row = prv_get_menu_layer_row(aml, aml->selected_index);
    const GColor bg_color = GColorBlack;
    const GColor fg_color = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite);

    GRect arrow_rect = (GRect){.size = GSize(aml_bounds->size.w, arrow_layer_height)};
    if (paged ? row > 0 : row >= max_visible_rows - 1) {
      grect_align(&arrow_rect, aml_bounds, GAlignTop, true /* clip */);
      content_indicator_draw_arrow(ctx, &arrow_rect, ContentIndicatorDirectionUp, fg_color,
                                   bg_color, GAlignTop);
    }
    if (paged ? row + 1 < num_rows : num_rows - row >= max_visible_rows) {
      grect_align(&arrow_rect, aml_bounds, GAlignBottom, true /* clip */);
      content_indicator_draw_arrow(ctx, &arrow_rect, ContentIndicatorDirectionDown, fg_color,
                                   bg_color, GAlignBottom);
    }
  }
#endif
}

static void prv_update_aml_cache(ActionMenuLayer *aml, int selected_index) {
  prv_unschedule_item_animation(aml);
#if GLYPH_GRID_SUPPORTED
  aml->layout_cache.font = prv_is_glyph_grid(aml) ? fonts_get_system_font(FONT_KEY_GOTHIC_28_EMOJI)
                                                  : prv_get_item_font();
#endif

  if (aml->layout_cache.item_heights != NULL) {
    applib_free(aml->layout_cache.item_heights);
    aml->layout_cache.item_heights = NULL;
  }

  if (aml->num_items > 0) {
    // Update the cache of heights.  We do this here to avoid recomputing the same
    // values repeatedly when we call the menu layer height callback.
    aml->layout_cache.item_heights = applib_zalloc(aml->num_items * sizeof(int16_t));
    for (int idx = 0; idx < aml->num_items; idx++) {
      aml->layout_cache.item_heights[idx] = prv_get_item_line_height(aml, idx);
    }
  }

#if PBL_ROUND
  const bool center_focused = !prv_aml_is_short(aml);
  menu_layer_set_center_focused(&aml->menu_layer, center_focused);
#endif
  // Short-item menus select before activation; the glyph grid resolves individual cells.
  menu_layer_set_tap_select_only(&aml->menu_layer, prv_aml_is_short(aml));

  prv_changed_proc(&aml->layer);
  layer_mark_dirty(&aml->layer);
  menu_layer_reload_data(&aml->menu_layer);
  prv_set_selected_index(aml, selected_index, false /* animated */);
}

// Public API
/////////////////////

void action_menu_layer_click_config_provider(ActionMenuLayer *aml) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, prv_scroll_handler);
  window_set_click_context(BUTTON_ID_UP, aml);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, prv_scroll_handler);
  window_set_click_context(BUTTON_ID_DOWN, aml);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_handler);
  window_set_click_context(BUTTON_ID_SELECT, aml);
}

void action_menu_layer_set_callback(ActionMenuLayer *aml, ActionMenuLayerCallback cb,
                                    void *context) {
  aml->callbacks = (ActionMenuLayerCallbacks){
    .select = cb,
  };
  aml->context = context;
}

void action_menu_layer_set_callbacks(ActionMenuLayer *aml, ActionMenuLayerCallbacks callbacks,
                                     void *context) {
  aml->callbacks = callbacks;
  aml->context = context;
}

void action_menu_layer_notify_selection_changed(ActionMenuLayer *aml) {
  prv_selection_changed(aml);
}

void action_menu_layer_init(ActionMenuLayer *aml, const GRect *frame) {
  layer_init(&aml->layer, frame);

  // Since menu_layer_set_callbacks() will call the menu functions, we need to initialize
  // the ActionMenuLayer attributes before setting the callbacks onto the menu.
  aml->item_animation = (ActionMenuItemAnimation){};
  aml->layout_cache = (ActionMenuLayoutCache){.font = prv_get_item_font()};
  aml->callbacks = (ActionMenuLayerCallbacks){};
  aml->layer.property_changed_proc = prv_changed_proc;
  aml->layer.update_proc = prv_update_proc;

  menu_layer_init(&aml->menu_layer, &aml->layer.bounds);
  menu_layer_set_normal_colors(&aml->menu_layer, GColorBlack,
                               PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite));
#if PBL_ROUND
  menu_layer_pad_bottom_enable(&aml->menu_layer, false);
#endif
  menu_layer_set_callbacks(&aml->menu_layer, aml,
                           &(MenuLayerCallbacks){
                             .get_num_rows = prv_get_num_rows,
                             .draw_row = prv_draw_row,
                             .get_cell_height = prv_get_cell_height_cb,
                             .get_separator_height = prv_get_separator_height_cb,
                             .draw_separator = prv_draw_separator_cb,
                             .get_header_height = prv_get_header_height_cb,
                             .draw_header = prv_draw_header_cb,
                             .selection_changed = prv_selection_changed_cb,
                             .select_click = prv_select_click_cb,
                           });
#ifdef CONFIG_TOUCH
  aml->menu_layer.touch_nav_node.ops = (void *)&s_touch_ops;
  // Let swipes start anywhere on the panel, including the picker viewport's margins.
  aml->menu_layer.touch_nav_node.layer = &aml->layer;
#endif

#if !defined(CONFIG_RECOVERY_FW)
  gbitmap_init_with_resource_system(&aml->item_animation.fade_top, SYSTEM_APP,
                                    RESOURCE_ID_ACTION_MENU_FADE_TOP);
  gbitmap_init_with_resource_system(&aml->item_animation.fade_bottom, SYSTEM_APP,
                                    RESOURCE_ID_ACTION_MENU_FADE_BOTTOM);
#endif

  layer_add_child(&aml->layer, menu_layer_get_layer(&aml->menu_layer));
  layer_set_hidden((Layer *)&aml->menu_layer.inverter, true);
  aml->menu_layer.selection_animation_disabled = true;
}

void action_menu_layer_deinit(ActionMenuLayer *aml) {
  if (aml->layout_cache.item_heights) {
    applib_free(aml->layout_cache.item_heights);
  }

  prv_unschedule_item_animation(aml);

#ifndef CONFIG_RECOVERY_FW
  gbitmap_deinit(&aml->item_animation.fade_top);
  gbitmap_deinit(&aml->item_animation.fade_bottom);
#endif

  menu_layer_deinit(&aml->menu_layer);
}

ActionMenuLayer *action_menu_layer_create(GRect frame) {
  ActionMenuLayer *aml = applib_zalloc(sizeof(ActionMenuLayer));
  if (!aml) {
    return NULL;
  }

  action_menu_layer_init(aml, &frame);
  return aml;
}

void action_menu_layer_destroy(ActionMenuLayer *aml) {
  if (!aml) {
    return;
  }

  action_menu_layer_deinit(aml);
  applib_free(aml);
}

void action_menu_layer_set_align(ActionMenuLayer *aml, ActionMenuAlign align) {
  if (!aml) {
    return;
  }
  aml->layout_cache.align = align;
}

void action_menu_layer_set_items(ActionMenuLayer *aml, const ActionMenuItem *items, int num_items,
                                 unsigned default_selected_item, unsigned separator_index) {
  aml->items = items;
  aml->num_items = num_items;
  aml->separator_index = separator_index;
  // Wide items never use the glyph grid, e.g. after returning from a glyph grid level.
  aml->glyph_grid = false;
  prv_update_aml_cache(aml, default_selected_item);
}

void action_menu_layer_set_glyph_grid(ActionMenuLayer *aml, bool glyph_grid) {
  aml->glyph_grid = glyph_grid;
}

void action_menu_layer_set_short_items(ActionMenuLayer *aml, const ActionMenuItem *items,
                                       int num_items, unsigned default_selected_item) {
  aml->short_items = items;
  aml->separator_index = 0;
  aml->num_short_items = num_items;
  prv_update_aml_cache(aml, default_selected_item);
}
