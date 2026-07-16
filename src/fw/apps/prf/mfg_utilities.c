/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/ui/ui.h"
#include "applib/ui/dialogs/confirmation_dialog.h"
#include "apps/prf/mfg_test_result.h"
#include "apps/prf/mfg_utilities.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/bluetooth/bluetooth_persistent_storage.h"
#include "process_state/app_state/app_state.h"
#include "pbl/util/size.h"

typedef struct {
  Window *window;
  SimpleMenuLayer *menu_layer;
  SimpleMenuSection menu_section;
} MfgUtilitiesAppData;

static void prv_clear_bondings_confirmed(ClickRecognizerRef recognizer, void *context) {
  ConfirmationDialog *confirmation_dialog = (ConfirmationDialog *)context;
  confirmation_dialog_pop(confirmation_dialog);

  bool confirmed = (click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP);
  if (confirmed) {
    bt_persistent_storage_delete_all_pairings();
  }
}

static void prv_clear_bondings_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_clear_bondings_confirmed);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_clear_bondings_confirmed);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_clear_bondings_confirmed);
}

static void prv_select_clear_bondings(int index, void *context) {
  ConfirmationDialog *confirmation_dialog = confirmation_dialog_create("Clear Bondings");
  Dialog *dialog = confirmation_dialog_get_dialog(confirmation_dialog);

  dialog_set_text(dialog, "Clear all bondings?\n\nThis action cannot be undone!");
  dialog_set_background_color(dialog, GColorOrange);
  dialog_set_text_color(dialog, GColorWhite);

  confirmation_dialog_set_click_config_provider(confirmation_dialog,
                                                prv_clear_bondings_click_config);

  ActionBarLayer *action_bar = confirmation_dialog_get_action_bar(confirmation_dialog);
  action_bar_layer_set_context(action_bar, confirmation_dialog);

  app_confirmation_dialog_push(confirmation_dialog);
}

#ifdef CONFIG_MFG
static void prv_reset_results_confirmed(ClickRecognizerRef recognizer, void *context) {
  ConfirmationDialog *confirmation_dialog = (ConfirmationDialog *)context;
  confirmation_dialog_pop(confirmation_dialog);

  bool confirmed = (click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP);
  if (confirmed) {
    mfg_test_result_reset();
  }
}

static void prv_reset_results_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_reset_results_confirmed);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_reset_results_confirmed);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_reset_results_confirmed);
}

static void prv_select_reset_results(int index, void *context) {
  ConfirmationDialog *confirmation_dialog = confirmation_dialog_create("Reset Results");
  Dialog *dialog = confirmation_dialog_get_dialog(confirmation_dialog);

  dialog_set_text(dialog, "Reset MFG results?\n\nThis action cannot be undone!");
  dialog_set_background_color(dialog, GColorOrange);
  dialog_set_text_color(dialog, GColorWhite);

  confirmation_dialog_set_click_config_provider(confirmation_dialog,
                                                prv_reset_results_click_config);

  ActionBarLayer *action_bar = confirmation_dialog_get_action_bar(confirmation_dialog);
  action_bar_layer_set_context(action_bar, confirmation_dialog);

  app_confirmation_dialog_push(confirmation_dialog);
}
#endif

static const SimpleMenuItem s_menu_items[] = {
  { .title = "Clear Bondings", .callback = prv_select_clear_bondings },
#ifdef CONFIG_MFG
  { .title = "Reset Results",  .callback = prv_select_reset_results },
#endif
};

static void prv_window_load(Window *window) {
  MfgUtilitiesAppData *data = app_state_get_user_data();

  Layer *window_layer = window_get_root_layer(data->window);
  GRect bounds = window_layer->bounds;

  data->menu_section = (SimpleMenuSection) {
    .num_items = ARRAY_LENGTH(s_menu_items),
    .items = s_menu_items
  };

  data->menu_layer = simple_menu_layer_create(bounds, data->window, &data->menu_section, 1, NULL);
  layer_add_child(window_layer, simple_menu_layer_get_layer(data->menu_layer));
}

static void s_main(void) {
  MfgUtilitiesAppData *data = app_malloc_check(sizeof(MfgUtilitiesAppData));
  *data = (MfgUtilitiesAppData){};

  app_state_set_user_data(data);

  data->window = window_create();
  window_init(data->window, "Utilities");
  window_set_window_handlers(data->window, &(WindowHandlers) {
    .load = prv_window_load,
  });
  window_set_fullscreen(data->window, true);
  app_window_stack_push(data->window, true /*animated*/);

  app_event_loop();
}

const PebbleProcessMd* mfg_utilities_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: 3e7b2c91-5a4d-4f68-b8e2-9c1d5f3a6b47
    .common.uuid = { 0x3e, 0x7b, 0x2c, 0x91, 0x5a, 0x4d, 0x4f, 0x68,
                     0xb8, 0xe2, 0x9c, 0x1d, 0x5f, 0x3a, 0x6b, 0x47 },
    .name = "MfgUtilities",
  };
  return (const PebbleProcessMd*) &s_app_info;
}
