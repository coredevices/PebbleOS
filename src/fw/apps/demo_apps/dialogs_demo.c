/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "dialogs_demo.h"

#include "applib/app.h"
#include "applib/graphics/gdraw_command_image.h"
#include "applib/graphics/gdraw_command_transforms.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/dialogs/dialog.h"
#include "applib/ui/dialogs/simple_dialog.h"
#include "applib/ui/dialogs/actionable_dialog.h"
#include "applib/ui/dialogs/expandable_dialog.h"
#include "applib/ui/window.h"
#include "applib/voice/transcription_dialog.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_menu_data_source.h"
#include "shell/normal/watchface.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "services/common/i18n/i18n.h"
#include "system/passert.h"
#include "util/size.h"

#include <stdio.h>
#include <string.h>


typedef struct DialogsData {
  Window window;
  MenuLayer menu_layer;
  const char *lorem_ipsum;
  const char *long_message;
  uint32_t resource_id_80;
  uint32_t resource_id_25;
} DialogsData;

/////////////////////////////
// Simple Dialog with timeout

static void prv_show_simple_dialog(DialogsData *data) {
  SimpleDialog *simple_dialog = simple_dialog_create("Simple Dialog");
  Dialog *dialog = simple_dialog_get_dialog(simple_dialog);
  dialog_set_text(dialog, "Mama");
  dialog_set_background_color(dialog, GColorRajah);
  dialog_set_icon(dialog, data->resource_id_80);
  dialog_set_timeout(dialog, DIALOG_TIMEOUT_DEFAULT);

  app_simple_dialog_push(simple_dialog);
}

///////////////////////////////
// Simple Dialog with vibration

static void prv_show_simple_dialog_vibe(DialogsData *data) {
  SimpleDialog *simple_dialog = simple_dialog_create("Simple Vibe Dialog");
  Dialog *dialog = simple_dialog_get_dialog(simple_dialog);
  dialog_set_text(dialog, "A Simple Dialog For Flow Testing!\nHello!");
  dialog_set_background_color(dialog, GColorLavenderIndigo);
  dialog_set_icon(dialog, data->resource_id_80);
  dialog_set_vibe(dialog, true);
  dialog_show_status_bar_layer(dialog, true);

  app_simple_dialog_push(simple_dialog);
}

//////////////////////
// Confirmation Dialog

static void prv_confirm_click_handler(ClickRecognizerRef recognizer, void *context) {
  app_window_stack_pop(true);
}

static void prv_confirm_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_confirm_click_handler);
}

static void prv_show_confirm_dialog(DialogsData *data) {
  ActionableDialog *actionable_dialog = actionable_dialog_create("Confirm Dialog");
  Dialog *dialog = actionable_dialog_get_dialog(actionable_dialog);
  dialog_set_text(dialog, "Confirmation");
  dialog_set_background_color(dialog, GColorGreen);
  dialog_set_icon(dialog, data->resource_id_80);
  dialog_show_status_bar_layer(dialog, true);

  actionable_dialog_set_action_bar_type(actionable_dialog, DialogActionBarConfirm, NULL);
  actionable_dialog_set_click_config_provider(actionable_dialog,
      prv_confirm_config_provider);
  app_actionable_dialog_push(actionable_dialog);
}

/////////////////
// Decline Dialog

static void prv_decline_click_handler(ClickRecognizerRef recognizer, void *context) {
  app_window_stack_pop(true);
}

static void prv_decline_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_decline_click_handler);
}

static void prv_show_decline_dialog(DialogsData *data) {
  ActionableDialog *actionable_dialog = actionable_dialog_create("Decline Dialog");
  Dialog *dialog = actionable_dialog_get_dialog(actionable_dialog);
  dialog_set_text(dialog, i18n_get("Decline dialog.", dialog));
  dialog_set_background_color(dialog, GColorRed);
  dialog_set_icon(dialog, data->resource_id_80);

  actionable_dialog_set_action_bar_type(actionable_dialog, DialogActionBarDecline, NULL);
  actionable_dialog_set_click_config_provider(actionable_dialog,
      prv_decline_config_provider);
  app_actionable_dialog_push(actionable_dialog);
}

//////////////////////////////////////////
// ActionableDialog with custom action bar

static void prv_custom_action_bar_click_up(ClickRecognizerRef recognizer, void *context) {
  Dialog *dialog = context;
  dialog_set_text(dialog, "The text has changed!");
  layer_mark_dirty(&dialog->text_layer.layer);
}

static void prv_custom_action_bar_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_custom_action_bar_click_up);
}

static void prv_show_custom_actionable_dialog(DialogsData *data) {
  ActionableDialog *actionable_dialog = actionable_dialog_create("Custom Actionable Dialog");
  Dialog *dialog = actionable_dialog_get_dialog(actionable_dialog);
  dialog_set_text(dialog, "Custom Actionable Dialog");
  dialog_set_background_color(dialog, GColorRed);
  dialog_set_icon(dialog, data->resource_id_80);

  // Create custom action bar for the dialog.
  // TODO destroy action bar / icon in unload callback
  ActionBarLayer *custom_action_bar = action_bar_layer_create();
  action_bar_layer_set_icon(custom_action_bar, BUTTON_ID_UP,
      gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_CHECK));
  action_bar_layer_set_context(custom_action_bar, dialog);
  action_bar_layer_set_click_config_provider(custom_action_bar,
      prv_custom_action_bar_config_provider);

  actionable_dialog_set_action_bar_type(actionable_dialog, DialogActionBarCustom,
                                        custom_action_bar);
  app_actionable_dialog_push(actionable_dialog);
}

////////////////////
// Expandable Dialog
// Has a custom icon/clickhandler for the select button

static void prv_my_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  ExpandableDialog *expandable_dialog = context;
  Dialog *dialog = expandable_dialog_get_dialog(expandable_dialog);
  dialog_set_background_color(dialog, GColorRed);
}

static void prv_show_expandable_dialog(DialogsData *data) {
  ExpandableDialog *expandable_dialog = expandable_dialog_create("Expandable Dialog");
  Dialog *dialog = expandable_dialog_get_dialog(expandable_dialog);
  dialog_set_text(dialog, data->lorem_ipsum);
  dialog_set_background_color(dialog, GColorLightGray);
  dialog_set_icon(dialog, data->resource_id_25);
  dialog_show_status_bar_layer(dialog, true);

  expandable_dialog_set_select_action(expandable_dialog,
      RESOURCE_ID_ACTION_BAR_ICON_CHECK, prv_my_select_click_handler);
  app_expandable_dialog_push(expandable_dialog);
}

////////////////////////////////
// Expandable Dialog with header

static void prv_show_expandable_dialog_header(DialogsData *data) {
  ExpandableDialog *expandable_dialog = expandable_dialog_create("Expandable Dialog");
  Dialog *dialog = expandable_dialog_get_dialog(expandable_dialog);
  dialog_set_text(dialog, data->lorem_ipsum);
  dialog_set_background_color(dialog, GColorLightGray);
  dialog_set_icon(dialog, data->resource_id_25);
  dialog_show_status_bar_layer(dialog, true);

  expandable_dialog_set_header(expandable_dialog, "Header");
  app_expandable_dialog_push(expandable_dialog);
}

////////////////////////////////
// Expandable Dialog with multi line header

static void prv_show_expandable_dialog_long_header(DialogsData *data) {
  ExpandableDialog *expandable_dialog = expandable_dialog_create("Expandable Dialog");
  Dialog *dialog = expandable_dialog_get_dialog(expandable_dialog);
  dialog_set_text(dialog, data->lorem_ipsum);
  dialog_set_background_color(dialog, GColorLightGray);
  dialog_set_icon(dialog, data->resource_id_25);
  dialog_show_status_bar_layer(dialog, true);

  expandable_dialog_set_header(expandable_dialog, "A very long header");
  app_expandable_dialog_push(expandable_dialog);
}

////////////////////////////////////////
// Expandable Dialog no icon and header

static void prv_show_expandable_dialog_header_no_icon(DialogsData *data) {
  ExpandableDialog *expandable_dialog = expandable_dialog_create("Expandable Dialog");
  Dialog *dialog = expandable_dialog_get_dialog(expandable_dialog);
  dialog_set_text(dialog, data->lorem_ipsum);
  dialog_set_background_color(dialog, GColorLightGray);
  dialog_show_status_bar_layer(dialog, true);

  expandable_dialog_set_header(expandable_dialog, "Header");
  app_expandable_dialog_push(expandable_dialog);
}

////////////////////////////////
// Expandable Dialog with header

static void prv_show_expandable_dialog_no_action_bar(DialogsData *data) {
  ExpandableDialog *expandable_dialog = expandable_dialog_create("Expandable Dialog");
  Dialog *dialog = expandable_dialog_get_dialog(expandable_dialog);
  dialog_set_text(dialog, data->lorem_ipsum);
  dialog_set_background_color(dialog, GColorLightGray);
  dialog_set_icon(dialog, data->resource_id_25);
  dialog_show_status_bar_layer(dialog, true);

  expandable_dialog_show_action_bar(expandable_dialog, false);
  expandable_dialog_set_header(expandable_dialog, "Header");
  app_expandable_dialog_push(expandable_dialog);
}

////////////////////////////////
// Expandable Dialog with icon
static void prv_show_expandable_dialog_no_icon(DialogsData *data) {
  ExpandableDialog *expandable_dialog = expandable_dialog_create("Expandable Dialog");
  Dialog *dialog = expandable_dialog_get_dialog(expandable_dialog);
  dialog_set_text(dialog, data->lorem_ipsum);
  dialog_set_background_color(dialog, GColorLightGray);
  dialog_show_status_bar_layer(dialog, true);

  expandable_dialog_show_action_bar(expandable_dialog, true);
  app_expandable_dialog_push(expandable_dialog);
}

///////////////////////////////////
// Expandable Dialog with no scroll
static void prv_show_expandable_dialog_no_scroll(DialogsData *data) {
  ExpandableDialog *expandable_dialog = expandable_dialog_create("Expandable Dialog");
  Dialog *dialog = expandable_dialog_get_dialog(expandable_dialog);
  dialog_set_text(dialog, "Look mah, no scroll!");
  dialog_set_background_color(dialog, GColorLightGray);
  dialog_show_status_bar_layer(dialog, true);

  expandable_dialog_show_action_bar(expandable_dialog, true);
  app_expandable_dialog_push(expandable_dialog);
}

////////////////////////////////
// Voice Dialog
static void prv_transcription_dialog_cb(void *context) {
  DialogsData *data = context;
  SimpleDialog *simple_dialog = simple_dialog_create("Simple Dialog");
  Dialog *dialog = simple_dialog_get_dialog(simple_dialog);
  dialog_set_text(dialog, "Pop!");
  dialog_set_background_color(dialog, GColorRed);
  dialog_set_icon(dialog, data->resource_id_80);
  dialog_set_timeout(dialog, DIALOG_TIMEOUT_DEFAULT);

  app_simple_dialog_push(simple_dialog);
}

static void prv_show_transcription_dialog(DialogsData *data) {
  TranscriptionDialog *transcription_dialog = transcription_dialog_create();
  static char transcription[500] = {0};
  uint16_t len = strlen(data->long_message);
  strncpy(transcription, data->long_message, len);
  transcription_dialog_update_text(transcription_dialog, transcription, len);
  app_transcription_dialog_push(transcription_dialog);
  transcription_dialog_set_callback(transcription_dialog, prv_transcription_dialog_cb, data);
}

/////////////////////////////////////
// Set up dialog labels and callbacks

typedef struct DialogNode {
  const char *label;
  void (*show)(DialogsData *data);
} DialogNode;

static const DialogNode nodes[] = {
  { .label = "D1 - Confirm",              .show = prv_show_confirm_dialog,                  },
  { .label = "D2 - Decline",              .show = prv_show_decline_dialog,                  },
  { .label = "D3 - Actionable",           .show = prv_show_custom_actionable_dialog,        },
  { .label = "D4 - Expandable",           .show = prv_show_expandable_dialog,               },
  { .label = "D4 - Exp with header",      .show = prv_show_expandable_dialog_header,        },
  { .label = "D4 - Exp with long header", .show = prv_show_expandable_dialog_long_header,   },
  { .label = "D5 - Simple Timeout",       .show = prv_show_simple_dialog,                   },
  { .label = "D5 - Simple Vibe",          .show = prv_show_simple_dialog_vibe,              },
  { .label = "D6 - Exp no action bar",    .show = prv_show_expandable_dialog_no_action_bar, },
  { .label = "D7 - Exp no icon",          .show = prv_show_expandable_dialog_no_icon,       },
  { .label = "D8 - Exp no scroll",        .show = prv_show_expandable_dialog_no_scroll,     },
  { .label = "D9 - Exp header only",      .show = prv_show_expandable_dialog_header_no_icon },
  { .label = "D10 - Transcription",       .show = prv_show_transcription_dialog,            }
};

static const uint16_t NUM_ITEMS = ARRAY_LENGTH(nodes);

//////////////
// MenuLayer callbacks

static void prv_draw_row_callback(GContext* ctx, Layer *cell_layer, MenuIndex *cell_index,
                                  DialogsData *data) {
  menu_cell_basic_draw(ctx, cell_layer, nodes[cell_index->row].label, NULL, NULL);
}

static void prv_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index,
                                DialogsData *data) {
  nodes[cell_index->row].show(data);
}

static uint16_t prv_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index,
                                          DialogsData *data) {
  return NUM_ITEMS;
}

///////////////////
// Window callbacks

static void prv_window_load(Window *window) {
  DialogsData *data = window_get_user_data(window);

  MenuLayer *menu_layer = &data->menu_layer;
  menu_layer_init(menu_layer, &window->layer.bounds);

  data->lorem_ipsum = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
                      "tempor incididunt ut labore et dolore magna aliqua. Utem ad happy.";
  data->long_message = "Don't you see how great this is? You, you are a... Jesse look at me. "
                       "You... are a blowfish.  A blowfish! Think about it. Small in stature, "
                       "not swift, not cunning. Easy prey for predators but the blowfish has a "
                       "secret weapon doesn't he. Doesn't he? What does the blowfish do, Jesse."
                       " What does the blowfish do? The blowfish puffs up, okay?";
  data->resource_id_80 = RESOURCE_ID_GENERIC_CONFIRMATION_LARGE;
  data->resource_id_25 = RESOURCE_ID_GENERIC_CONFIRMATION_TINY;

  menu_layer_set_callbacks(menu_layer, data, &(MenuLayerCallbacks) {
    .get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback) prv_get_num_rows_callback,
    .draw_row = (MenuLayerDrawRowCallback) prv_draw_row_callback,
    .select_click = (MenuLayerSelectCallback) prv_select_callback,
  });
  menu_layer_set_click_config_onto_window(menu_layer, window);
  layer_add_child(&window->layer, menu_layer_get_layer(menu_layer));
}

static void prv_handle_init(void) {
  DialogsData *data = app_zalloc_check(sizeof(DialogsData));

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Dialogs"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load,
  });
  const bool animated = true;
  app_window_stack_push(window, animated);
}

static void prv_handle_deinit() {
  DialogsData *data = app_state_get_user_data();
  menu_layer_deinit(&data->menu_layer);
  i18n_free_all(data);
  app_free(data);
}

////////////////////
// App boilerplate

static void s_main(void) {
  prv_handle_init();
  app_event_loop();
  prv_handle_deinit();
}

const PebbleProcessMd* dialogs_demo_get_app_info() {
  static const PebbleProcessMdSystem s_app_md = {
    .common = {
      .main_func = s_main,
      // UUID: ab470e5f-5ffd-46f2-9aa9-f48352ea5499
      .uuid = {0xab, 0x47, 0x0e, 0x5f, 0x5f, 0xfd, 0x46, 0xf2,
               0x9a, 0xa9, 0xf4, 0x83, 0x52, 0xea, 0x54, 0x99},
    },
    .name = "Dialogs",
  };
  return (const PebbleProcessMd*) &s_app_md;
}
