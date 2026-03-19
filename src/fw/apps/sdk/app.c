/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app.h"

#include "applib/app.h"
#include "applib/graphics/perimeter.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window_private.h"
#include "apps/system_app_ids.h"
#include "apps/system/launcher/launcher.h"
#include "apps/system/timeline/timeline.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include "process_state/app_state/app_state.h"
#include "services/common/clock.h"
#include "shell/sdk/shell_sdk.h"
#include "shell/sdk/watchface.h"
#include "system/logging.h"

typedef struct SdkAppData {
  Window window;
  TextLayer info_text_layer;
  TextLayer time_text_layer;
  char time_buffer[TIME_STRING_TIME_LENGTH];
} SdkAppData;

static void prv_sdk_home_update_proc(Layer *layer, GContext *ctx) {
  window_do_layer_update_proc(layer, ctx);

  SdkAppData *data = app_state_get_user_data();
  const int time_max_y = grect_get_max_y(&data->time_text_layer.layer.frame);
  const int line_top_margin = 14;
  const int line_stroke_width = 2;
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, &GRect(0, time_max_y + line_top_margin, DISP_COLS, line_stroke_width));
}

static void prv_update_time(SdkAppData *data) {
  clock_copy_time_string(data->time_buffer, sizeof(data->time_buffer));
  text_layer_set_text(&data->time_text_layer, data->time_buffer);
}

static void prv_update_info(SdkAppData *data) {
  const AppInstallId app_id = shell_sdk_get_last_installed_app();
  const char *text;
  const unsigned int tip_delay_s = 5;
  if ((app_id == INSTALL_ID_INVALID) || shell_sdk_last_installed_app_is_watchface()) {
    text = "Install an app to continue";
  } else if ((rtc_get_time() / tip_delay_s) & 1) {
    // Show the launcher help on every odd set of five seconds since the epoch
    text = "Press Select to access Launcher";
  } else {
    text = "Press Down to access Timeline";
  }
  text_layer_set_text(&data->info_text_layer, text);
}

static void prv_update_ui(SdkAppData *data) {
  prv_update_time(data);
  prv_update_info(data);
}

static void prv_handle_tick_timer(struct tm *tick_time, TimeUnits units_changed) {
  prv_update_ui(app_state_get_user_data());
}

static void prv_config_provider(void *data) {
}

static void prv_init(void) {
  SdkAppData *data = app_malloc_check(sizeof(SdkAppData));

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("SDK Home"));
  window_set_click_config_provider(window, prv_config_provider);
  window_set_overrides_back_button(window, true);
  window_set_fullscreen(window, true);
  layer_set_update_proc(&window->layer, prv_sdk_home_update_proc);

  GRect frame = data->window.layer.frame;

  const int top_margin = PBL_IF_RECT_ELSE(19, 25);
  const int time_height = 46;
  frame.origin.y = top_margin;
  frame.size.h = time_height;

  TextLayer *time_text_layer = &data->time_text_layer;
  text_layer_init(time_text_layer, &frame);
  text_layer_set_font(time_text_layer, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
  text_layer_set_text_alignment(time_text_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(time_text_layer, GTextOverflowModeWordWrap);
  text_layer_set_background_color(&data->time_text_layer, GColorClear);
  layer_add_child(&window->layer, &time_text_layer->layer);

  const int time_padding = 22;
  frame.origin.y += time_height + time_padding;
  frame.size.h = DISP_ROWS - frame.origin.y;

  TextLayer *info_text_layer = &data->info_text_layer;
  text_layer_init(info_text_layer, &frame);
  text_layer_set_font(info_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(info_text_layer, GTextAlignmentCenter);
  text_layer_set_background_color(&data->info_text_layer, GColorClear);
  layer_add_child(&window->layer, &info_text_layer->layer);

#if PBL_ROUND
  const uint8_t inset = 8;
  text_layer_enable_screen_text_flow_and_paging(info_text_layer, inset);
#endif

  app_state_set_user_data(data);

  window_set_background_color(window, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  tick_timer_service_subscribe(SECOND_UNIT, prv_handle_tick_timer);

  prv_update_ui(data);

  app_window_stack_push(window, true /* is_animated */);
}

static void s_main(void) {
  prv_init();

  app_event_loop();
}

const PebbleProcessMd* sdk_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_md = {
    .common = {
      .main_func = s_main,
      // UUID: 1197fc39-47e7-439b-82be-f56d9ba1dbd8
      .uuid = { 0x11, 0x97, 0xfc, 0x39, 0x47, 0xe7, 0x43, 0x9b,
                0x82, 0xbe, 0xf5, 0x6d, 0x9b, 0xa1, 0xdb, 0xd8 },
      .process_type = ProcessTypeWatchface
    },
    .icon_resource_id = RESOURCE_ID_MENU_ICON_TICTOC_WATCH,
    .name = "TicToc"
  };
  return (const PebbleProcessMd*) &s_app_md;
}
