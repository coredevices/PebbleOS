/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "watchface.h"

#include "apps/system_app_ids.h"
#include "apps/system/launcher/launcher.h"
#include "apps/system/settings/quick_launch.h"
#include "apps/system/settings/quick_launch_app_menu.h"
#include "apps/system/settings/quick_launch_setup_menu.h"
#include "apps/system/timeline/timeline.h"
#include "apps/watch/low_power/face.h"
#include "kernel/event_loop.h"
#include "kernel/low_power.h"
#include "kernel/ui/modals/modal_manager.h"
#include "popups/timeline/peek.h"
#include "process_management/app_manager.h"
#include "process_management/pebble_process_md.h"
#include "services/common/analytics/analytics.h"
#include "services/common/compositor/compositor_transitions.h"
#include "applib/app_timer.h"
#include "applib/ui/click_internal.h"
#include "services/normal/notifications/do_not_disturb.h"
#include "system/logging.h"
#include "system/passert.h"

#define QUICK_LAUNCH_HOLD_MS (400)
#define BIT_SET (1)
#define BIT_CLEAR (0)
#define COMBO_BACK_UP_BUTTONS ((BIT_SET << BUTTON_ID_BACK) | (BIT_SET << BUTTON_ID_UP))

static ClickManager s_click_manager;
static uint8_t s_buttons_pressed = BIT_CLEAR;
static AppTimer *s_combo_back_up_timer = NULL;

static bool prv_should_ignore_button_click(void) {
  if (app_manager_get_task_context()->closing_state != ProcessRunState_Running) {
    // Ignore if the app is not running (such as if it is in the process of closing)
    return true;
  }
  if (low_power_is_active()) {
    // If we're in low power mode we dont allow any interaction
    return true;
  }
  return false;
}

static void prv_launch_app_via_button(AppLaunchEventConfig *config,
                                      ClickRecognizerRef recognizer) {
  config->common.button = click_recognizer_get_button_id(recognizer);
  app_manager_put_launch_app_event(config);
}

static void prv_combo_back_up_timer_callback(void *data) {
  s_combo_back_up_timer = NULL;
  // Double-check that both buttons are still pressed before executing combo
  if ((s_buttons_pressed & COMBO_BACK_UP_BUTTONS) == COMBO_BACK_UP_BUTTONS) {
    if (quick_launch_combo_back_up_is_enabled()) {
      AppInstallId app_id = quick_launch_combo_back_up_get_app();
      if (app_id != INSTALL_ID_INVALID) {
        // Reset all button states before launching app to prevent state corruption
        s_buttons_pressed = BIT_CLEAR;  // Reset our own tracking
        app_manager_put_launch_app_event(&(AppLaunchEventConfig) {
          .id = app_id,
          .common.reason = APP_LAUNCH_QUICK_LAUNCH,
          .common.button = BUTTON_ID_BACK,
        });
        return;
      }
    }
  }
}

static void prv_check_combo_back_up(void) {
  bool back_pressed = (s_buttons_pressed & (BIT_SET << BUTTON_ID_BACK)) != BIT_CLEAR;
  bool up_pressed = (s_buttons_pressed & (BIT_SET << BUTTON_ID_UP)) != BIT_CLEAR;
  bool both_pressed = back_pressed && up_pressed;
  
  if (both_pressed) {
    if (s_combo_back_up_timer == NULL) {
      // Cancel individual button timers to prevent them from firing
      // This ensures only the combo executes, not individual hold handlers
      click_recognizer_reset(&s_click_manager.recognizers[BUTTON_ID_BACK]);
      click_recognizer_reset(&s_click_manager.recognizers[BUTTON_ID_UP]);
      s_combo_back_up_timer = app_timer_register(QUICK_LAUNCH_HOLD_MS, prv_combo_back_up_timer_callback, NULL);
    }
  } else {
    if (s_combo_back_up_timer != NULL) {
      app_timer_cancel(s_combo_back_up_timer);
      s_combo_back_up_timer = NULL;
    }
  }
}

static void prv_quick_launch_handler(ClickRecognizerRef recognizer, void *data) {
  ButtonId button = click_recognizer_get_button_id(recognizer);

  if (s_combo_back_up_timer != NULL || 
      (s_buttons_pressed & COMBO_BACK_UP_BUTTONS) == COMBO_BACK_UP_BUTTONS) {
    return;
  }
  
  if (!quick_launch_is_enabled(button)) {
    return;
  }
  AppInstallId app_id = quick_launch_get_app(button);
  if (app_id == INSTALL_ID_INVALID) {
    app_id = app_install_get_id_for_uuid(&quick_launch_setup_get_app_info()->uuid);
  }
  s_buttons_pressed = BIT_CLEAR;  // Reset our own tracking
  prv_launch_app_via_button(&(AppLaunchEventConfig) {
    .id = app_id,
    .common.reason = APP_LAUNCH_QUICK_LAUNCH,
  }, recognizer);
}

static void prv_launch_up_down(ClickRecognizerRef recognizer, void *data) {
  ButtonId button = click_recognizer_get_button_id(recognizer);
  
  if ((s_buttons_pressed & COMBO_BACK_UP_BUTTONS) == COMBO_BACK_UP_BUTTONS) {
    return;
  }
  
  if (!quick_launch_single_click_is_enabled(button)) return;
  //check if quick launch app is not timeline
  if (quick_launch_single_click_get_app(button) != APP_ID_TIMELINE) {
    //launch other quick launch apps
    prv_launch_app_via_button(&(AppLaunchEventConfig) {
      .id = quick_launch_single_click_get_app(button),
      .common.reason = APP_LAUNCH_QUICK_LAUNCH,
    }, recognizer);
    return;
  }

  static TimelineArgs s_timeline_args;
  const bool is_up = (click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP);
  if (is_up) {
    PBL_LOG_DBG("Launching timeline in past mode.");
    s_timeline_args.direction = TimelineIterDirectionPast;
    analytics_inc(ANALYTICS_DEVICE_METRIC_TIMELINE_PAST_LAUNCH_COUNT, AnalyticsClient_System);
  } else {
    PBL_LOG_DBG("Launching timeline in future mode.");
    s_timeline_args.direction = TimelineIterDirectionFuture;
    analytics_inc(ANALYTICS_DEVICE_METRIC_TIMELINE_FUTURE_LAUNCH_COUNT, AnalyticsClient_System);
  }
  s_timeline_args.launch_into_pin = true;
  s_timeline_args.stay_in_list_view = true;
  timeline_peek_get_item_id(&s_timeline_args.pin_id);

  const CompositorTransition *animation = NULL;
  const bool is_future = (s_timeline_args.direction == TimelineIterDirectionFuture);
  const bool timeline_is_destination = true;
#if PBL_ROUND
  animation = compositor_dot_transition_timeline_get(is_future, timeline_is_destination);
#else
  const bool jump = (!uuid_is_invalid(&s_timeline_args.pin_id) && !timeline_peek_is_first_event());
  animation = jump ? compositor_peek_transition_timeline_get() :
                     compositor_slide_transition_timeline_get(is_future, timeline_is_destination,
                                                              timeline_peek_is_future_empty());
#endif
  prv_launch_app_via_button(&(AppLaunchEventConfig) {
    .id = APP_ID_TIMELINE,
    .common.args = &s_timeline_args,
    .common.transition = animation,
  }, recognizer);
}

static void prv_configure_click_handler(ButtonId button_id, ClickHandler single_click_handler) {
  ClickConfig *cfg = &s_click_manager.recognizers[button_id].config;
  cfg->long_click.delay_ms = QUICK_LAUNCH_HOLD_MS;
  cfg->long_click.handler = prv_quick_launch_handler;
  cfg->click.handler = single_click_handler;
}

static void prv_launch_launcher_app(ClickRecognizerRef recognizer, void *data) {
  static const LauncherMenuArgs s_launcher_args = { .reset_scroll = true };
  prv_launch_app_via_button(&(AppLaunchEventConfig) {
    .id = APP_ID_LAUNCHER_MENU,
    .common.args = &s_launcher_args,
  }, recognizer);
}

static void prv_dismiss_timeline_peek(ClickRecognizerRef recognizer, void *data) {
  if ((s_buttons_pressed & COMBO_BACK_UP_BUTTONS) == COMBO_BACK_UP_BUTTONS) {
    return;
  }
  timeline_peek_dismiss();
}

static void prv_watchface_configure_click_handlers(void) {
  prv_configure_click_handler(BUTTON_ID_UP, prv_launch_up_down);
  prv_configure_click_handler(BUTTON_ID_DOWN, prv_launch_up_down);
  prv_configure_click_handler(BUTTON_ID_SELECT, prv_launch_launcher_app);
  prv_configure_click_handler(BUTTON_ID_BACK, prv_dismiss_timeline_peek);
}

void watchface_init(void) {
  click_manager_init(&s_click_manager);
  prv_watchface_configure_click_handlers();
}

void watchface_handle_button_event(PebbleEvent *e) {
  if (prv_should_ignore_button_click()) {
    return;
  }
  switch (e->type) {
    case PEBBLE_BUTTON_DOWN_EVENT:
      s_buttons_pressed |= (BIT_SET << e->button.button_id);
      click_recognizer_handle_button_down(&s_click_manager.recognizers[e->button.button_id]);
      prv_check_combo_back_up();
      break;
    case PEBBLE_BUTTON_UP_EVENT:
      s_buttons_pressed &= ~(BIT_SET << e->button.button_id);
      prv_check_combo_back_up();
      click_recognizer_handle_button_up(&s_click_manager.recognizers[e->button.button_id]);
      break;
    default:
      PBL_CROAK("Invalid event type: %u", e->type);
      break;
  }
}

static void prv_watchface_launch_low_power(void) {
  PBL_LOG_DBG("Switching default watchface to low_power_mode watchface");
  app_manager_put_launch_app_event(&(AppLaunchEventConfig) {
    .id = APP_ID_LOW_POWER_FACE,
  });
}

void watchface_launch_default(const CompositorTransition *animation) {
  app_manager_put_launch_app_event(&(AppLaunchEventConfig) {
    .id = watchface_get_default_install_id(),
    .common.transition = animation,
  });
}

static void kernel_callback_watchface_launch(void* data) {
  watchface_launch_default(NULL);
}

void command_watch(void) {
  launcher_task_add_callback(kernel_callback_watchface_launch, NULL);
}

void watchface_start_low_power(void) {
  app_manager_set_minimum_run_level(ProcessAppRunLevelNormal);
  prv_watchface_launch_low_power();
}

void watchface_reset_click_manager(void) {
  click_manager_reset(&s_click_manager);
}
