/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "option_menu_window.h"

#include "applib/applib_malloc.auto.h"
#include "resource/resource_ids.auto.h"
#include "shell/system_theme.h"
#include "system/passert.h"

typedef struct OptionMenuStyle {
#if PBL_RECT
  uint16_t cell_heights[OptionMenuContentTypeCount];
#endif
  int16_t top_inset;
  int16_t right_icon_spacing;
  int16_t text_inset_single;
  int16_t text_inset_multi;
  int16_t right_text_inset_with_icon;
} OptionMenuStyle;

static const OptionMenuStyle s_style_medium = {
#if PBL_RECT
  .cell_heights[OptionMenuContentType_DoubleLine] = 56,
#endif
  .right_icon_spacing = PBL_IF_RECT_ELSE(7, 35),
};

static const OptionMenuStyle s_style_large = {
#if PBL_RECT
  .cell_heights[OptionMenuContentType_SingleLine] = 46,
#endif
  .top_inset = 1,
  .right_icon_spacing = PBL_IF_RECT_ELSE(10, 35),
  .text_inset_single = -1,
  .text_inset_multi = -3,
  .right_text_inset_with_icon = 4,
};

static const OptionMenuStyle * const s_styles[NumPreferredContentSizes] = {
  [PreferredContentSizeSmall] = &s_style_medium,
  [PreferredContentSizeMedium] = &s_style_medium,
  [PreferredContentSizeLarge] = &s_style_large,
  [PreferredContentSizeExtraLarge] = &s_style_large,
};

static const OptionMenuStyle *prv_get_style(void) {
  return s_styles[PreferredContentSizeDefault];
}

static uint16_t prv_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index,
                                          void *context) {
  OptionMenu *option_menu = context;
  if (option_menu->callbacks.get_num_rows) {
    return option_menu->callbacks.get_num_rows(option_menu, option_menu->context);
  }
  return 0;
}

uint16_t option_menu_default_cell_height(OptionMenuContentType content_type, bool selected) {
  const OptionMenuStyle * const PBL_UNUSED style = prv_get_style();
  const int16_t cell_height =
      PBL_IF_ROUND_ELSE(selected ? MENU_CELL_ROUND_FOCUSED_SHORT_CELL_HEIGHT :
                                   MENU_CELL_ROUND_UNFOCUSED_TALL_CELL_HEIGHT,
                        style->cell_heights[content_type]);
  return cell_height ?: menu_cell_basic_cell_height();
}

static int16_t prv_get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index,
                                            void *context) {
  const bool is_selected = menu_layer_is_index_selected(menu_layer, cell_index);
  OptionMenu *option_menu = context;
  if (option_menu->callbacks.get_cell_height) {
    return option_menu->callbacks.get_cell_height(option_menu, cell_index->row, is_selected,
                                                  option_menu->context);
  } else {
    return option_menu_default_cell_height(option_menu->content_type, is_selected);
  }
}

static int32_t prv_draw_selection_icon(const OptionMenu *option_menu, GContext *ctx,
                                       const GRect *cell_layer_bounds, bool is_chosen) {
  const int32_t left_icon_spacing = PBL_IF_RECT_ELSE(0, 14);
  const GSize not_chosen_icon_bounds = gbitmap_get_bounds(&option_menu->not_chosen_image).size;
  const GSize chosen_icon_bounds = gbitmap_get_bounds(&option_menu->chosen_image).size;
  PBL_ASSERTN(gsize_equal(&not_chosen_icon_bounds, &chosen_icon_bounds));
  GRect icon_frame = { .size = chosen_icon_bounds };
  grect_align(&icon_frame, cell_layer_bounds, GAlignRight, false);

  const OptionMenuStyle * const style = prv_get_style();
  icon_frame.origin.x -= style->right_icon_spacing;

  const GBitmap *const icon =
      is_chosen ? &option_menu->chosen_image : &option_menu->not_chosen_image;
  graphics_context_set_compositing_mode(ctx, GCompOpTint);
  graphics_draw_bitmap_in_rect(ctx, icon, &icon_frame);
  return icon_frame.size.w + left_icon_spacing + style->right_icon_spacing;
}

static void prv_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                                  void *context) {
  OptionMenu *option_menu = context;

  const MenuIndex selected = menu_layer_get_selected_index(&option_menu->menu_layer);
  const bool is_selected = (menu_index_compare(&selected, cell_index) == 0);

  const GRect *cell_layer_bounds = &cell_layer->bounds;
  GRect remaining_rect = *cell_layer_bounds;

  if (option_menu->icons_enabled) {
    const bool is_chosen = (cell_index->row == option_menu->choice);
    const int32_t left_inset_x = PBL_IF_RECT_ELSE(0, 14);
    const int32_t right_inset_x = prv_draw_selection_icon(option_menu, ctx, &remaining_rect,
                                                          is_chosen);
    remaining_rect = grect_inset(remaining_rect, GEdgeInsets(0, right_inset_x, 0, left_inset_x));
  }

#if PBL_ROUND
  if (!is_selected && option_menu->icons_enabled) {
    const int32_t left_text_inset_to_prevent_clipping = 8;
    remaining_rect = grect_inset(remaining_rect,
                                 GEdgeInsets(0, 0, 0, left_text_inset_to_prevent_clipping));
  }
#else
  const OptionMenuStyle * const style = prv_get_style();
  const int32_t left_text_inset = menu_cell_basic_horizontal_inset();
  const int32_t right_text_inset = option_menu->icons_enabled ? style->right_text_inset_with_icon :
                                                                left_text_inset;
  remaining_rect = grect_inset(remaining_rect, GEdgeInsets(style->top_inset, right_text_inset, 0,
                                                           left_text_inset));
#endif

  if (option_menu->callbacks.draw_row) {
    option_menu->callbacks.draw_row(option_menu, ctx, cell_layer, &remaining_rect, cell_index->row,
                                    is_selected, option_menu->context);
  }

}

static void prv_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  OptionMenu *option_menu = context;
  option_menu->choice = cell_index->row;
  layer_mark_dirty((Layer *)&option_menu->menu_layer);
  if (option_menu->callbacks.select) {
    option_menu->callbacks.select(option_menu, option_menu->choice, option_menu->context);
  }
}

static void prv_window_load(Window *window) {
  OptionMenu *option_menu = window_get_user_data(window);

  menu_layer_set_callbacks(&option_menu->menu_layer, option_menu, &(MenuLayerCallbacks) {
      .get_cell_height = prv_get_cell_height_callback,
      .get_num_rows = prv_get_num_rows_callback,
      .draw_row = prv_draw_row_callback,
      .select_click = prv_select_callback
  });
  menu_layer_set_click_config_onto_window(&option_menu->menu_layer, window);
  if (option_menu->choice != OPTION_MENU_CHOICE_NONE) {
    menu_layer_set_selected_index(&option_menu->menu_layer, MenuIndex(0, option_menu->choice),
                                  MenuRowAlignCenter, false);
  }
  layer_add_child(window_get_root_layer(window), menu_layer_get_layer(&option_menu->menu_layer));
}

static void prv_window_unload(Window *window) {
  OptionMenu *option_menu = window_get_user_data(window);
  if (option_menu->callbacks.unload) {
    option_menu->callbacks.unload(option_menu, option_menu->context);
  }
}

void option_menu_set_status_colors(OptionMenu *option_menu, GColor background, GColor foreground) {
  option_menu->status_colors.background = background;
  option_menu->status_colors.foreground = foreground;
  status_bar_layer_set_colors(&option_menu->status_layer,
                              option_menu->status_colors.background,
                              option_menu->status_colors.foreground);
}

void option_menu_set_normal_colors(OptionMenu *option_menu, GColor background, GColor foreground) {
  option_menu->normal_colors.background = background;
  option_menu->normal_colors.foreground = foreground;
  menu_layer_set_normal_colors(&option_menu->menu_layer,
                               option_menu->normal_colors.background,
                               option_menu->normal_colors.foreground);
}

void option_menu_set_highlight_colors(OptionMenu *option_menu, GColor background,
                                      GColor foreground) {
  option_menu->highlight_colors.background = background;
  option_menu->highlight_colors.foreground = foreground;
  menu_layer_set_highlight_colors(&option_menu->menu_layer,
                                  option_menu->highlight_colors.background,
                                  option_menu->highlight_colors.foreground);
}

void option_menu_set_callbacks(OptionMenu *option_menu, const OptionMenuCallbacks *callbacks,
                               void *context) {
  option_menu->callbacks = *callbacks;
  option_menu->context = context;
}

void option_menu_set_title(OptionMenu *option_menu, const char *title) {
  option_menu->title = title;
  status_bar_layer_set_title(&option_menu->status_layer, title, false, false);
}

void option_menu_set_choice(OptionMenu *option_menu, int choice) {
  option_menu->choice = choice;
  layer_mark_dirty((Layer *)&option_menu->menu_layer);
}

void option_menu_set_content_type(OptionMenu *option_menu, OptionMenuContentType content_type) {
  option_menu->content_type = content_type;
}

void option_menu_reload_data(OptionMenu *option_menu) {
  menu_layer_reload_data(&option_menu->menu_layer);
}

void option_menu_set_icons_enabled(OptionMenu *option_menu, bool icons_enabled) {
  option_menu->icons_enabled = icons_enabled;
}

void option_menu_configure(OptionMenu *option_menu,
                           const OptionMenuConfig *config) {
  option_menu_set_title(option_menu, config->title);
  option_menu_set_choice(option_menu, config->choice);
  option_menu_set_content_type(option_menu, config->content_type);
  option_menu_set_status_colors(option_menu, config->status_colors.background,
                                config->status_colors.foreground);
  option_menu_set_highlight_colors(option_menu, config->highlight_colors.background,
                                   config->highlight_colors.foreground);
  option_menu_set_icons_enabled(option_menu, config->icons_enabled);
}

void option_menu_init(OptionMenu *option_menu) {
  *option_menu = (OptionMenu) {
    .choice = OPTION_MENU_CHOICE_NONE,
    .title_font = system_theme_get_font_for_default_size(TextStyleFont_MenuCellTitle),
  };

  // radio button icons are enabled by default
  option_menu_set_icons_enabled(option_menu, true);

  GBitmap *chosen_image = &option_menu->chosen_image;
  gbitmap_init_with_resource(chosen_image, RESOURCE_ID_CHECKED_RADIO_BUTTON);
  GBitmap *not_chosen_image = &option_menu->not_chosen_image;
  gbitmap_init_with_resource(not_chosen_image, RESOURCE_ID_UNCHECKED_RADIO_BUTTON);

  window_init(&option_menu->window, WINDOW_NAME("OptionMenu"));
  window_set_user_data(&option_menu->window, option_menu);
  window_set_window_handlers(&option_menu->window, &(WindowHandlers) {
      .load = prv_window_load,
      .unload = prv_window_unload,
  });

  StatusBarLayer *status_layer = &option_menu->status_layer;
  status_bar_layer_init(status_layer);
  status_bar_layer_set_separator_mode(status_layer, OPTION_MENU_STATUS_SEPARATOR_MODE);
  layer_add_child(&option_menu->window.layer, &status_layer->layer);

  MenuLayer *menu_layer = &option_menu->menu_layer;
  GRect bounds = grect_inset(option_menu->window.layer.bounds, (GEdgeInsets) {
    .top = STATUS_BAR_LAYER_HEIGHT,
    .bottom = PBL_IF_RECT_ELSE(0, STATUS_BAR_LAYER_HEIGHT),
  });
  menu_layer_init(menu_layer, &bounds);
}

void option_menu_deinit(OptionMenu *option_menu) {
  menu_layer_deinit(&option_menu->menu_layer);
  status_bar_layer_deinit(&option_menu->status_layer);
  window_deinit(&option_menu->window);

  gbitmap_deinit(&option_menu->chosen_image);
  gbitmap_deinit(&option_menu->not_chosen_image);
}

OptionMenu *option_menu_create(void) {
  OptionMenu *option_menu = applib_type_malloc(OptionMenu);
  if (!option_menu) {
    return NULL;
  }
  option_menu_init(option_menu);
  return option_menu;
}

void option_menu_destroy(OptionMenu *option_menu) {
  option_menu_deinit(option_menu);
  applib_free(option_menu);
}

void option_menu_system_draw_row(OptionMenu *option_menu, GContext *ctx, const Layer *cell_layer,
                                 const GRect *cell_frame, const char *title, bool selected,
                                 void *context) {
  const GTextOverflowMode overflow_mode = GTextOverflowModeTrailingEllipsis;
  // On rectangular, always align to the left. On round, align to the right if we have an icon and
  // otherwise to the center. Icons on the right with text in the center looks very bad and wastes
  // text space.
  const GTextAlignment text_alignment =
      PBL_IF_RECT_ELSE(GTextAlignmentLeft,
                       option_menu->icons_enabled ? GTextAlignmentRight : GTextAlignmentCenter);
  GFont const title_font = option_menu->title_font;
  const GSize text_size = graphics_text_layout_get_max_used_size(ctx, title, title_font,
                                                                 *cell_frame, overflow_mode,
                                                                 text_alignment, NULL);
  GRect text_frame = *cell_frame;
  const int min_text_height = fonts_get_font_height(title_font);
  text_frame.size = text_size;
  const GAlign text_frame_alignment =
      PBL_IF_RECT_ELSE(GAlignLeft, option_menu->icons_enabled ? GAlignRight : GAlignCenter);
  grect_align(&text_frame, cell_frame, text_frame_alignment, true /* clips */);
  const OptionMenuStyle * const style = prv_get_style();
  const int16_t text_inset = (text_size.h > min_text_height) ? style->text_inset_multi :
                                                               style->text_inset_single;
  text_frame = grect_inset(text_frame, GEdgeInsets(0, text_inset));
  text_frame.origin.y -= fonts_get_font_cap_offset(title_font);

  if (title) {
    graphics_draw_text(ctx, title, title_font, text_frame, overflow_mode, text_alignment, NULL);
  }
}
