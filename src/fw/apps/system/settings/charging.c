/* SPDX-FileCopyrightText: 2026 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "charging.h"
#include "menu.h"
#include "window.h"

#include "applib/ui/menu_cell_layer.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/i18n/i18n.h"
#include "shell/prefs.h"
#include "system/passert.h"

typedef struct SettingsChargingData {
    SettingsCallbacks callbacks;
} SettingsChargingData;

enum SettingsChargingItem {
    SettingsChargingBlinkWhenFull,
    SettingsChargingVibeWhenFull,
    NumSettingsChargingItems
};

static void prv_deinit_cb(SettingsCallbacks *context) {
    SettingsChargingData *data = (SettingsChargingData *)context;
    i18n_free_all(data);
    app_free(data);
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                             const Layer *cell_layer, uint16_t row, bool selected) {
    SettingsChargingData *data = (SettingsChargingData *)context;

    const char *title = NULL;
    const char *subtitle = NULL;

    switch (row) {
        case SettingsChargingBlinkWhenFull:
            title = i18n_noop("Blink When Full");
            subtitle = charging_blink_when_full_enabled() ? i18n_noop("On") : i18n_noop("Off");
            break;
        case SettingsChargingVibeWhenFull:
            title = i18n_noop("Vibe When Full");
            subtitle = charging_vibe_when_full_enabled() ? i18n_noop("On") : i18n_noop("Off");
            break;
        default:
            WTF;
    }
    menu_cell_basic_draw(ctx, cell_layer, i18n_get(title, data), i18n_get(subtitle, data), NULL);
}

static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
    switch (row) {
        case SettingsChargingBlinkWhenFull:
            charging_set_blink_when_full_enabled(!charging_blink_when_full_enabled());
            break;
        case SettingsChargingVibeWhenFull:
            charging_set_vibe_when_full_enabled(!charging_vibe_when_full_enabled());
            break;
        default:
            WTF;
    }
    settings_menu_reload_data(SettingsMenuItemCharging);
    settings_menu_mark_dirty(SettingsMenuItemCharging);
}

static uint16_t prv_num_rows_cb(SettingsCallbacks *context) {
    return NumSettingsChargingItems;
}

static Window *prv_init(void) {
    SettingsChargingData *data = app_malloc_check(sizeof(*data));
    *data = (SettingsChargingData){};

    data->callbacks = (SettingsCallbacks) {
        .deinit = prv_deinit_cb,
        .draw_row = prv_draw_row_cb,
        .select_click = prv_select_click_cb,
        .num_rows = prv_num_rows_cb,
    };

    return settings_window_create(SettingsMenuItemCharging, &data->callbacks);
}

const SettingsModuleMetadata *settings_charging_get_info(void) {
    static const SettingsModuleMetadata s_module_info = {
        .name = i18n_noop("Charging"),
        .init = prv_init,
    };

    return &s_module_info;
}