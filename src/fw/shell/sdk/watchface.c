/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "watchface.h"

#include "applib/ui/dialogs/expandable_dialog.h"
#include "apps/system_app_ids.h"
#include "apps/system_apps/launcher/launcher_app.h"
#include "apps/system_apps/timeline/timeline.h"
#include "kernel/event_loop.h"
#include "kernel/ui/modals/modal_manager.h"
#include "popups/timeline/peek.h"
#include "process_management/app_manager.h"
#include "process_management/pebble_process_md.h"
#include "services/common/analytics/analytics.h"
#include "services/common/compositor/compositor_transitions.h"
#include "shell/sdk/shell_sdk.h"
#include "shell/system_app_state_machine.h"
#include "system/logging.h"
#include "system/passert.h"

typedef struct WatchfaceData {
  ClickManager click_manager;
  ButtonId button_pressed;
  AppInstallId active_watchface;
} WatchfaceData;

static WatchfaceData s_watchface_data;

void watchface_launch_default(const CompositorTransition *animation) {
  app_manager_put_launch_app_event(&(AppLaunchEventConfig) {
    .id = watchface_get_default_install_id(),
    .common.transition = animation,
  });
}

static void prv_launch_app_via_button(AppLaunchEventConfig *config,
                                      ClickRecognizerRef recognizer) {
  config->common.button = click_recognizer_get_button_id(recognizer);
  app_manager_put_launch_app_event(config);
}

#if CAPABILITY_HAS_SDK_SHELL4
static void prv_launch_launcher(ClickRecognizerRef recognizer, void *data) {
  static const LauncherMenuArgs s_launcher_args = { .reset_scroll = true };
  prv_launch_app_via_button(&(AppLaunchEventConfig) {
    .id = APP_ID_LAUNCHER_MENU,
    .common.args = &s_launcher_args,
  }, recognizer);
}

static void prv_launch_timeline(ClickRecognizerRef recognizer, void *data) {
  static TimelineArgs s_timeline_args = {
    .pin_id = UUID_INVALID_INIT,
  };
  switch (click_recognizer_get_button_id(recognizer)) {
    case BUTTON_ID_DOWN:
      s_timeline_args.direction = TimelineIterDirectionFuture;
      break;
    case BUTTON_ID_UP:
      s_timeline_args.direction = TimelineIterDirectionPast;
      break;
    default:
      WTF;
  }

  prv_launch_app_via_button(&(AppLaunchEventConfig) {
    .id = APP_ID_TIMELINE,
    .common.args = &s_timeline_args,
  }, recognizer);
}

static void prv_configure_click(ButtonId button_id, ClickHandler click_handler) {
  WatchfaceData *data = &s_watchface_data;
  ClickConfig *cfg = &data->click_manager.recognizers[button_id].config;
  cfg->click.handler = click_handler;
}

static void prv_watchface_configure_click_handlers(void) {
  prv_configure_click(BUTTON_ID_SELECT, prv_launch_launcher);
  prv_configure_click(BUTTON_ID_DOWN, prv_launch_timeline);
  prv_configure_click(BUTTON_ID_UP, prv_launch_timeline);
}
#endif

void watchface_init(void) {
#if CAPABILITY_HAS_SDK_SHELL4
  WatchfaceData *data = &s_watchface_data;
  click_manager_init(&data->click_manager);
  prv_watchface_configure_click_handlers();
#endif
}

void watchface_handle_button_event(PebbleEvent *e) {
#if CAPABILITY_HAS_SDK_SHELL4
  // Only handle button press if app state indicates that the app is still running
  // which is not in the process of closing
  WatchfaceData *data = &s_watchface_data;
  if (app_manager_get_task_context()->closing_state == ProcessRunState_Running) {
    data->button_pressed = e->button.button_id;
    switch (e->type) {
    case PEBBLE_BUTTON_DOWN_EVENT:
      click_recognizer_handle_button_down(&data->click_manager.recognizers[e->button.button_id]);
      break;
    case PEBBLE_BUTTON_UP_EVENT:
      click_recognizer_handle_button_up(&data->click_manager.recognizers[e->button.button_id]);
      break;
    default:
      PBL_CROAK("Invalid event type: %u", e->type);
      break;
    }
  }
#else
  if ((e->button.button_id == BUTTON_ID_SELECT) && (e->type == PEBBLE_BUTTON_DOWN_EVENT)) {
    app_manager_put_launch_app_event(&(AppLaunchEventConfig) {
      .id = system_app_state_machine_get_last_registered_app(),
      .common.reason = APP_LAUNCH_USER,
      .common.button = e->button.button_id,
    });
  }
#endif
}

void watchface_reset_click_manager(void) {
  WatchfaceData *data = &s_watchface_data;
  click_manager_reset(&data->click_manager);
}
