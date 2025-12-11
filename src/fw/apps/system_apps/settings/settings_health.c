/* SPDX-License-Identifier: Apache-2.0 */

#include "settings_health.h"
#include "settings_menu.h"
#include "settings_option_menu.h"
#include "settings_window.h"

#include "kernel/pbl_malloc.h"
#include "services/common/i18n/i18n.h"
#include "shell/prefs.h"
#include "system/passert.h"

#include <string.h>

typedef struct SettingsHealthData {
    SettingsCallbacks callbacks;
} SettingsHealthData;

static const char *s_units_distance_labels[] = {
    i18n_noop("Kilometers"),
    i18n_noop("Miles"),
};

enum SettingsHealthItem {
    SettingsHealthUnitDistance,
    NumSettingsHealthItems
};

static void prv_deinit_cb(SettingsCallbacks *context) {
    SettingsHealthData *data = (SettingsHealthData*)context;

    i18n_free_all(data);
    app_free(data);
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                            const Layer *cell_layer, uint16_t row, bool selected) {
    SettingsHealthData *data = (SettingsHealthData*) context;

    const char *title = NULL;
    const char *subtitle = NULL;

    switch (row) {
        case SettingsHealthUnitDistance:
            title = i18n_noop("Distance Unit");
            UnitsDistance unit = shell_prefs_get_units_distance();
            if (unit >= UnitsDistanceCount) {
                subtitle = i18n_noop("Unknown");
            } else {
                subtitle = s_units_distance_labels[unit];
            }
            break;
        default:
            WTF;
    }
    menu_cell_basic_draw(ctx, cell_layer, title, subtitle, NULL);
}

static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
    SettingsHealthData *data = (SettingsHealthData*)context;
    switch (row) {
        case SettingsHealthUnitDistance:
            UnitsDistance unit = shell_prefs_get_units_distance();
            unit = (unit + 1) % UnitsDistanceCount;
            shell_prefs_set_units_distance(unit);
            break;
        default:
            WTF;
    }
    settings_menu_reload_data(SettingsMenuItemHealth);
    settings_menu_mark_dirty(SettingsMenuItemHealth);
}

static uint16_t prv_num_rows_cb(SettingsCallbacks *context) {
    uint16_t rows = NumSettingsHealthItems;

    return rows;
}

static void prv_appear_cb(SettingsCallbacks *context) {
    SettingsHealthData *data = (SettingsHealthData*)context;
}

static void prv_hide_cb(SettingsCallbacks *context) {
    SettingsHealthData *data = (SettingsHealthData*)context;
}

static Window *prv_init(void) {
    SettingsHealthData *data = app_malloc_check(sizeof(*data));
    *data = (SettingsHealthData){};

    data->callbacks = (SettingsCallbacks) {
        .deinit = prv_deinit_cb,
        .draw_row = prv_draw_row_cb,
        .select_click = prv_select_click_cb,
        .num_rows = prv_num_rows_cb,
        .appear = prv_appear_cb,
        .hide = prv_hide_cb,
    };

    return settings_window_create(SettingsMenuItemHealth, &data->callbacks);
}

const SettingsModuleMetadata *settings_health_get_info(void) {
    static const SettingsModuleMetadata s_module_info = {
        .name = i18n_noop("Health"),
        .init = prv_init,
    };

    return &s_module_info;
}