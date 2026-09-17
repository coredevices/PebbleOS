/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/app_menu_data_source.h"

#include "pbl/kernel/compiler.h"

PBL_WEAK void app_menu_data_source_init(AppMenuDataSource *source,
                                        const AppMenuDataSourceCallbacks *handlers,
                                        void *callback_context) {
}

PBL_WEAK void app_menu_data_source_deinit(AppMenuDataSource *source) {
}

PBL_WEAK void app_menu_data_source_enable_icons(AppMenuDataSource *source,
                                                uint32_t fallback_icon_id) {
}

PBL_WEAK AppMenuNode *app_menu_data_source_get_node_at_index(AppMenuDataSource *source,
                                                             uint16_t row_index) {
  return NULL;
}

PBL_WEAK uint16_t app_menu_data_source_get_count(AppMenuDataSource *source) {
  return 0;
}

PBL_WEAK uint16_t app_menu_data_source_get_index_of_app_with_install_id(AppMenuDataSource *source,
                                                                        AppInstallId install_id) {
  return 0;
}

PBL_WEAK void app_menu_data_source_draw_row(AppMenuDataSource *source, GContext *ctx,
                                            Layer *cell_layer, MenuIndex *cell_index) {
}

PBL_WEAK void app_menu_data_source_set_default_icon(AppMenuDataSource *source, uint32_t icon_id) {
}

PBL_WEAK GBitmap *app_menu_data_source_get_node_icon(AppMenuDataSource *source, AppMenuNode *node) {
  return NULL;
}
