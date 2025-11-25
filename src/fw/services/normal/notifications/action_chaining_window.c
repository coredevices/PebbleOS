/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "action_chaining_window.h"

#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"

typedef struct {
  Window window;
  MenuLayer menu_layer;
  StatusBarLayer status_layer;

  const char *title;
  TimelineItemActionGroup *action_group;
  ActionChainingMenuSelectCb select_cb;
  ActionChainingMenuClosedCb closed_cb;
  void *select_cb_context;
  void *closed_cb_context;
} ChainingWindowData;

#if PBL_ROUND
static int16_t prv_get_header_height(struct MenuLayer *menu_layer,
                                     uint16_t section_index,
                                     void *callback_context) {
  return MENU_CELL_ROUND_UNFOCUSED_TALL_CELL_HEIGHT;
}

static void prv_draw_header(GContext *ctx,
                            const Layer *cell_layer,
                            uint16_t section_index,
                            void *callback_context) {
  ChainingWindowData *data = callback_context;
  const GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  menu_cell_basic_draw_custom(ctx, cell_layer, font, data->title, font, NULL, font,
                              NULL, NULL, false, GTextOverflowModeWordWrap);
}
#endif

static uint16_t prv_get_num_rows(MenuLayer *menu_layer, uint16_t section_index,
                                 void *callback_context) {
  ChainingWindowData *data = callback_context;
  return data->action_group->num_actions;
}

static int16_t prv_get_cell_height(struct MenuLayer *menu_layer,
                                   MenuIndex *cell_index,
                                   void *callback_context) {
#if PBL_ROUND
  MenuIndex selected_index = menu_layer_get_selected_index(menu_layer);
  bool is_selected = menu_index_compare(cell_index, &selected_index) == 0;
  return is_selected ? MENU_CELL_ROUND_FOCUSED_SHORT_CELL_HEIGHT :
                       MENU_CELL_ROUND_UNFOCUSED_TALL_CELL_HEIGHT;
#else
  return menu_cell_basic_cell_height();
#endif
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer,
                         MenuIndex *cell_index, void *callback_context) {
  ChainingWindowData *data = callback_context;

  AttributeList *attrs = &data->action_group->actions[cell_index->row].attr_list;
  Attribute *title_attr = attribute_find(attrs, AttributeIdTitle);
  Attribute *subtitle_attr = attribute_find(attrs, AttributeIdSubtitle);

  const char *title = title_attr ? title_attr->cstring : NULL;
  const char *subtitle = subtitle_attr ? subtitle_attr->cstring : NULL;

  menu_cell_basic_draw(ctx, cell_layer, title, subtitle, NULL);
}

static void prv_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index,
                                void *callback_context) {
  ChainingWindowData *data = callback_context;

  if (data->select_cb) {
    data->select_cb(&data->window, &data->action_group->actions[cell_index->row],
                    data->select_cb_context);
  }
}

static void prv_chaining_window_unload(Window *window) {
  ChainingWindowData *data = window_get_user_data(window);
  if (data->closed_cb) {
    data->closed_cb(data->closed_cb_context);
  }

  menu_layer_deinit(&data->menu_layer);
#if PBL_RECT
  status_bar_layer_deinit(&data->status_layer);
#endif
  kernel_free(data);
}

static void prv_chaining_window_load(Window *window) {
  ChainingWindowData *data = window_get_user_data(window);

  const GRect bounds = grect_inset(data->window.layer.bounds, (GEdgeInsets) {
    .top = STATUS_BAR_LAYER_HEIGHT,
#if PBL_ROUND
    .bottom = STATUS_BAR_LAYER_HEIGHT
#endif
  });
  menu_layer_init(&data->menu_layer, &bounds);
  menu_layer_set_callbacks(&data->menu_layer, data, &(MenuLayerCallbacks) {
#if PBL_ROUND
    .get_header_height = prv_get_header_height,
    .draw_header = prv_draw_header,
#endif
    .get_num_rows = prv_get_num_rows,
    .get_cell_height = prv_get_cell_height,
    .draw_row = prv_draw_row,
    .select_click = prv_select_callback,
  });

  menu_layer_set_highlight_colors(
      &data->menu_layer, PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorBlack), GColorWhite);
  menu_layer_set_click_config_onto_window(&data->menu_layer, &data->window);
  layer_add_child(&data->window.layer, menu_layer_get_layer(&data->menu_layer));

#if PBL_RECT
  status_bar_layer_init(&data->status_layer);
  status_bar_layer_set_colors(&data->status_layer, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack),
                              PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
  status_bar_layer_set_title(&data->status_layer, data->title,
                             false /* revert */, false /* animated */);
  status_bar_layer_set_separator_mode(&data->status_layer, StatusBarLayerSeparatorModeDotted);
  layer_add_child(&data->window.layer, status_bar_layer_get_layer(&data->status_layer));
#endif
}


void action_chaining_window_push(WindowStack *window_stack, const char *title,
                                 TimelineItemActionGroup *action_group,
                                 ActionChainingMenuSelectCb select_cb,
                                 void *select_cb_context,
                                 ActionChainingMenuClosedCb closed_cb,
                                 void *closed_cb_context) {
  ChainingWindowData *data = kernel_zalloc_check(sizeof(ChainingWindowData));
  *data = (ChainingWindowData) {
    .title = title,
    .action_group = action_group,
    .select_cb = select_cb,
    .select_cb_context = select_cb_context,
    .closed_cb = closed_cb,
    .closed_cb_context = closed_cb_context,
  };

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Action Chaining"));
  window_set_user_data(window, data);

  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_chaining_window_load,
    .unload = prv_chaining_window_unload,
  });

  window_stack_push(window_stack, window, true);
}
