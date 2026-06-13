/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "security.h"
#include "menu.h"
#include "window.h"

#include "applib/ui/app_window_stack.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/i18n/i18n.h"
#include "services/pin_lock/pin_lock.h"
#include "system/passert.h"

typedef struct SecurityData {
  SettingsCallbacks callbacks;
} SecurityData;

static void prv_deinit_cb(SettingsCallbacks *context) {
  SecurityData *data = (SecurityData *)context;
  i18n_free_all(data);
  app_free(data);
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                            const Layer *cell_layer, uint16_t row, bool selected) {
  SecurityData *data = (SecurityData *)context;
  // Only one row: "Lock Now"
  (void)row;
  menu_cell_basic_draw(ctx, cell_layer, i18n_get(i18n_noop("Lock Now"), data), NULL, NULL);
}

static uint16_t prv_num_rows_cb(SettingsCallbacks *context) {
  return 1;
}

static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  pin_lock_lock_now();
}

static Window *prv_init(void) {
  SecurityData *data = app_malloc_check(sizeof(*data));
  *data = (SecurityData){};

  data->callbacks = (SettingsCallbacks){
    .deinit = prv_deinit_cb,
    .draw_row = prv_draw_row_cb,
    .select_click = prv_select_click_cb,
    .num_rows = prv_num_rows_cb,
  };

  return settings_window_create(SettingsMenuItemSecurity, &data->callbacks);
}

const SettingsModuleMetadata *settings_security_get_info(void) {
  static const SettingsModuleMetadata s_module_info = {
    .name = i18n_noop("Security"),
    .init = prv_init,
  };
  return &s_module_info;
}
