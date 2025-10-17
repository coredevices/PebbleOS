/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "event_loop.h"
#include "events.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "applib/app_launch_reason.h"
#include "applib/battery_state_service.h"
#include "applib/connection_service.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/animation_private.h"
#include "applib/ui/app_window_click_glue.h"
#include "applib/ui/ui.h"
#include "applib/ui/window.h"
#include "applib/ui/window_private.h"
#include "comm/ble/kernel_le_client/kernel_le_client.h"
#include "console/serial_console.h"
#include "console/prompt.h"
#include "drivers/backlight.h"
#include "drivers/battery.h"
#include "drivers/button.h"
#include "drivers/task_watchdog.h"
#include "kernel/core_dump.h"
#include "kernel/kernel_applib_state.h"
#include "kernel/low_power.h"
#include "kernel/panic.h"
#include "kernel/pbl_malloc.h"
#include "kernel/ui/kernel_ui.h"
#include "kernel/ui/modals/modal_manager.h"
#include "kernel/util/factory_reset.h"
#include "mcu/fpu.h"
#include "pebble_errors.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include "process_management/app_run_state.h"
#include "process_management/process_manager.h"
#include "process_management/worker_manager.h"
#include "resource/resource_ids.auto.h"
#include "services/common/analytics/analytics.h"
#include "services/common/battery/battery_state.h"
#include "services/common/battery/battery_monitor.h"
#include "services/common/compositor/compositor.h"
#include "services/common/cron.h"
#include "services/common/debounced_connection_service.h"
#include "services/common/ecompass.h"
#include "services/common/event_service.h"
#include "services/common/evented_timer.h"
#include "services/common/firmware_update.h"
#include "services/common/i18n/i18n.h"
#include "services/common/light.h"
#include "services/common/new_timer/new_timer.h"
#include "services/common/put_bytes/put_bytes.h"
#include "services/common/status_led.h"
#include "services/common/system_task.h"
#include "services/common/vibe_pattern.h"
#include "services/normal/accessory/accessory_manager.h"
#include "services/normal/alarms/alarm.h"
#include "services/normal/app_fetch_endpoint.h"
#include "services/normal/blob_db/api.h"
#include "services/normal/notifications/do_not_disturb.h"
#include "services/normal/stationary.h"
#include "services/normal/timeline/reminders.h"
#include "services/normal/wakeup.h"
#include "services/runlevel.h"
#include "shell/normal/app_idle_timeout.h"
#include "shell/normal/watchface.h"
#include "shell/prefs.h"
#include "shell/shell_event_loop.h"
#include "shell/system_app_state_machine.h"
#include "system/bootbits.h"
#include "system/logging.h"
#include "system/passert.h"
#include "system/reset.h"
#include "system/testinfra.h"
#include "util/bitset.h"
#include "util/struct.h"
#include "system/version.h"

#include <bluetooth/reconnect.h>

#include "FreeRTOS.h"
#include "task.h"

static const uint32_t FORCE_QUIT_HOLD_MS = 1500;
static int s_back_hold_timer = TIMER_INVALID_ID;

static const uint32_t BACK_QUICKPRESS_INTERVAL_TICKS = 300;
static const int BACK_QUICKPRESS_COREDUMP_PRESSES = 10;
static RtcTicks s_back_quickpress_last = 0;
static int s_back_quickpress_count = 0;

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  PebbleEvent event = {
    .type = PEBBLE_CALLBACK_EVENT,
    .callback = {
      .callback = callback,
      .data = data,
    },
  };
  event_put(&event);
}

bool launcher_task_is_current_task(void) {
  return (pebble_task_get_current() == PebbleTask_KernelMain);
}

//! Return true if event could cause pop-up
//! Used in getting started and during firmware update
static bool launcher_is_popup_event(PebbleEvent* e) {
  switch (e->type) {
    case PEBBLE_SYS_NOTIFICATION_EVENT:
    case PEBBLE_ALARM_CLOCK_EVENT:
    case PEBBLE_BATTERY_CONNECTION_EVENT:
    case PEBBLE_BATTERY_STATE_CHANGE_EVENT:
      return true;
    default:
      return false;
  }
}

static int s_block_popup_count = 0;

void launcher_block_popups(bool block) {
  if (block) {
    s_block_popup_count++;
  } else {
    PBL_ASSERTN(s_block_popup_count > 0);
    s_block_popup_count--;
  }
}

bool launcher_popups_are_blocked(void) {
  return s_block_popup_count > 0;
}

// FIRM-425: sometimes, if the system goes out to lunch for a long time when
// loading a watchface as a result of pressing 'back', the
// FORCE_QUIT_HOLD_MS timer will trigger.  Check once we wake up and process
// the launcher_force_quit_app event to see if the back button release event
// got processed between the NewTimer firing and KernelMain waking up to do the kill.
static bool s_force_quit_was_cancelled = false;

void launcher_cancel_force_quit(void) {
  s_force_quit_was_cancelled = true;
  new_timer_stop(s_back_hold_timer);
}

// Export the count of how many times this has happened to Memfault.
uint32_t metric_firm_425_back_button_long_presses_cancelled = 0;

static void launcher_force_quit_app(void *data) {
  if (s_force_quit_was_cancelled) {
    // If you see this in logs after FIRM-556 (general system sluggishness)
    // is fixed, please file a bug!
    PBL_LOG(LOG_LEVEL_ERROR, "NewTimer event fired for force quit, but the back button was released before we went to deal with it!  Wow, we must have been really slow!  Please file a bug!");
    metric_firm_425_back_button_long_presses_cancelled++;
    return;
  }

  if (low_power_is_active() || factory_reset_ongoing()) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Forcekill disabled due to low-power or factory-reset");
    return;
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "Force killing app.");
  app_manager_force_quit_to_launcher();
}

static void back_button_force_quit_handler(void *data) {
  // Happens on NewTimer thread -- the launcher task may still be "behind"
  // on servicing events, so do check again later once *this* event gets
  // serviced to see if this is still relevant!
  launcher_task_add_callback(launcher_force_quit_app, NULL);
}

static void launcher_handle_button_event(PebbleEvent* e) {
  ButtonId button_id = e->button.button_id;
  const bool watchface_running = app_manager_is_watchface_running();

  // trigger the backlight on any button down event
  if (e->type == PEBBLE_BUTTON_DOWN_EVENT) {
    analytics_inc(ANALYTICS_DEVICE_METRIC_BUTTON_PRESSED_COUNT, AnalyticsClient_System);

    if (button_id == BUTTON_ID_BACK && !watchface_running &&
        process_metadata_get_run_level(
            app_manager_get_current_app_md()) == ProcessAppRunLevelNormal) {
      // Start timer for force-quitting app
      s_force_quit_was_cancelled = false;
      bool success = new_timer_start(s_back_hold_timer, FORCE_QUIT_HOLD_MS, back_button_force_quit_handler, NULL,
                                     0 /*flags*/);
      PBL_ASSERTN(success);
    }
    
#ifndef SHELL_SDK
    // 10 quick-presses of the back button triggers a manual coredump, if
    // that feature is enabled in system settings.
    if (button_id == BUTTON_ID_BACK) {
      RtcTicks now = rtc_get_ticks();
      if ((now - s_back_quickpress_last) > BACK_QUICKPRESS_INTERVAL_TICKS) {
        s_back_quickpress_count = 0;
      }
      s_back_quickpress_last = now;
      s_back_quickpress_count++;
      if (s_back_quickpress_count >= BACK_QUICKPRESS_COREDUMP_PRESSES && shell_prefs_can_coredump_on_request()) {
        PBL_LOG(LOG_LEVEL_INFO, "triggering core dump because you asked for it!");
        core_dump_reset(true /* is_forced */);
      }
    }
#endif // !SHELL_SDK

    light_button_pressed();
  } else if (e->type == PEBBLE_BUTTON_UP_EVENT) {
    if (button_id == BUTTON_ID_BACK) {
      launcher_cancel_force_quit();
    }
    light_button_released();
  }

  app_idle_timeout_refresh();

  if (compositor_is_animating()) {
    // mask the app task if we're already animating
    e->task_mask |= 1 << PebbleTask_App;
    return;
  }

  const bool is_modal_focused = (modal_manager_get_enabled() &&
                                 !(modal_manager_get_properties() & ModalProperty_Unfocused));
  if (is_modal_focused) {
    // mask the app task if a modal is on top
    e->task_mask |= 1 << PebbleTask_App;
    modal_manager_handle_button_event(e);
    return;
  }

  if (watchface_running) {
    watchface_handle_button_event(e);
    // suppress the button event from the app task
    e->task_mask |= 1 << PebbleTask_App;
  }
}

// This function should handle very basic events (Button clicks, app launching, battery events,
// crashes, etc.
static NOINLINE void prv_minimal_event_handler(PebbleEvent* e) {
  switch (e->type) {
    case PEBBLE_BUTTON_DOWN_EVENT:
    case PEBBLE_BUTTON_UP_EVENT:
      launcher_handle_button_event(e);
      return;

    case PEBBLE_BATTERY_CONNECTION_EVENT: {
      const bool is_connected = e->battery_connection.is_connected;
      battery_state_handle_connection_event(is_connected);
      if (is_connected) {
        light_enable_interaction();
      } else {
        // Chances are the Pebble of our dear customer has been charging away
        // from the phone and is disconnected because of that. Try reconnecting
        // immediately upon disconnecting the charger:
        bt_driver_reconnect_reset_interval();
        bt_driver_reconnect_try_now(false /*ignore_paused*/);
      }
#if STATIONARY_MODE
      stationary_handle_battery_connection_change_event();
#endif
      return;
    }

    case PEBBLE_BATTERY_STATE_CHANGE_EVENT:
      battery_monitor_handle_state_change_event(e->battery_state.new_state);
#if CAPABILITY_HAS_MAGNETOMETER
      ecompass_handle_battery_state_change_event(e->battery_state.new_state);
#endif
      return;

    case PEBBLE_RENDER_READY_EVENT:
      compositor_app_render_ready();
      return;

    case PEBBLE_ACCEL_SHAKE_EVENT:
      analytics_inc(ANALYTICS_DEVICE_METRIC_ACCEL_SHAKE_COUNT, AnalyticsClient_System);
      if (backlight_is_motion_enabled()) {
        light_enable_interaction();
      }
      return;

    case PEBBLE_PANIC_EVENT:
      launcher_panic(e->panic.error_code);
      break;

    case PEBBLE_APP_LAUNCH_EVENT:
      if (!app_install_is_app_running(e->launch_app.id)) {
        process_manager_launch_process(&(ProcessLaunchConfig) {
          .id = e->launch_app.id,
          .common = NULL_SAFE_FIELD_ACCESS(e->launch_app.data, common, (LaunchConfigCommon) {}),
        });
      }
      return;

    case PEBBLE_WORKER_LAUNCH_EVENT:
      if (!app_install_is_worker_running(e->launch_app.id)) {
        process_manager_launch_process(&(ProcessLaunchConfig) {
          .id = e->launch_app.id,
          .common = NULL_SAFE_FIELD_ACCESS(e->launch_app.data, common, (LaunchConfigCommon) {}),
          .worker = true,
        });
      }
      return;

    case PEBBLE_CALLBACK_EVENT:
      e->callback.callback(e->callback.data);
      return;

    case PEBBLE_PROCESS_KILL_EVENT:
      process_manager_close_process(e->kill.task, e->kill.gracefully);
      return;

    case PEBBLE_SUBSCRIPTION_EVENT:
      // App button events depend on this, so this needs to be in the minimal event handler.
      event_service_handle_subscription(&e->subscription);
      return;

    default:
      PBL_LOG_VERBOSE("Received an unhandled event (%u)", e->type);
      return;
  }
}

static NOINLINE void prv_handle_app_fetch_request_event(PebbleEvent *e) {
  AppInstallEntry entry;
  PBL_ASSERTN(app_install_get_entry_for_install_id(e->app_fetch_request.id, &entry));
  bool has_worker = app_install_entry_has_worker(&entry);
  app_fetch_binaries(&entry.uuid, e->app_fetch_request.id, has_worker);
}

static NOINLINE void prv_extended_event_handler(PebbleEvent* e) {
  switch (e->type) {
    case PEBBLE_APP_OUTBOX_MSG_EVENT:
      e->app_outbox_msg.callback(e->app_outbox_msg.data);
      return;

    case PEBBLE_APP_FETCH_REQUEST_EVENT:
      prv_handle_app_fetch_request_event(e);
      return;

    case PEBBLE_PUT_BYTES_EVENT:
      // TODO: inform the other things interested in put_bytes (apps?)
      firmware_update_pb_event_handler(&e->put_bytes);
#ifndef RECOVERY_FW
      app_fetch_put_bytes_event_handler(&e->put_bytes);
#endif
      return;

    case PEBBLE_SYSTEM_MESSAGE_EVENT:
      firmware_update_event_handler(&e->firmware_update);
      return;

    case PEBBLE_ECOMPASS_SERVICE_EVENT:
#if CAPABILITY_HAS_MAGNETOMETER
      ecompass_service_handle();
#endif
      return;

    case PEBBLE_SET_TIME_EVENT:
    {
#ifndef RECOVERY_FW
      PebbleSetTimeEvent *set_time_info = &e->set_time_info;

      // The phone and watch time may be out of sync by a second or two (since
      // we don't account for the time it takes for the request to change the
      // time to propagate to the watch). Thus only update our alarm time if
      // the timezone has changed or a 'substantial' time has passed, or DST
      // state has changed.
      if (set_time_info->gmt_offset_delta != 0 ||
          set_time_info->dst_changed ||
          ABS(set_time_info->utc_time_delta) > 15) {
        alarm_handle_clock_change();
        wakeup_handle_clock_change();
        cron_service_handle_clock_change(set_time_info);
      }

      // TODO: evaluate if these need to change on every time update
      do_not_disturb_handle_clock_change();
      reminders_update_timer();
#endif
      return;
    }
    case PEBBLE_BLE_SCAN_EVENT:
    case PEBBLE_BLE_CONNECTION_EVENT:
    case PEBBLE_BLE_GATT_CLIENT_EVENT:
      kernel_le_client_handle_event(e);
      return;

    case PEBBLE_COMM_SESSION_EVENT: {
      PebbleCommSessionEvent *comm_session_event = &e->bluetooth.comm_session_event;
      debounced_connection_service_handle_event(comm_session_event);
      put_bytes_handle_comm_session_event(comm_session_event);
#ifndef RECOVERY_FW
      if (comm_session_event->is_system) {
        // tell the phone which app is running
        const Uuid *running_uuid = &app_manager_get_current_app_md()->uuid;
        if (running_uuid != NULL) {
          app_run_state_send_update(running_uuid, RUNNING);
        }
      }
#endif
      return;
    }

    default:
      return;
  }
}

//! Tasks that have to be done in between each event.
static void event_loop_upkeep(void) {
  modal_manager_event_loop_upkeep();
}

// NOTE: Marking this as NOINLINE saves us 150+ bytes on the KernelMain stack
static void NOINLINE prv_handle_event(PebbleEvent *e) {
  prv_minimal_event_handler(e);

  // FIXME: This logic is pretty wacky, but I'm going to leave it as is to refactor later out of
  // fear of breaking something. This should mimic the exact same behaviour as before but
  // flattened.
  if (s_block_popup_count > 0) {
    // A service has requested that the launcher block any events that may cause
    // pop-ups
    if (launcher_is_popup_event(e)) {
      return;
    }
  }

  if (!launcher_panic_get_current_error()) {
    prv_extended_event_handler(e);
  }

  shell_event_loop_handle_event(e);
}

static NOINLINE void prv_launcher_main_loop_init(void) {
  s_back_hold_timer = new_timer_create();

  process_manager_init();
  app_manager_init();
  worker_manager_init();
  vibes_init();
  battery_monitor_init();
  evented_timer_init();
#if CAPABILITY_HAS_MAGNETOMETER
  ecompass_service_init();
#endif
  tick_timer_service_init();
  debounced_connection_service_init();
  event_service_system_init();
#if CAPABILITY_HAS_ACCESSORY_CONNECTOR
  accessory_manager_init();
#endif

  modal_manager_init();

  shell_event_loop_init();

#if STATIONARY_MODE
  stationary_init();
#endif

  task_watchdog_bit_set(PebbleTask_KernelMain);

  // if we are in launcher panic, don't turn on any extra services.
  const RunLevel run_level = launcher_panic_get_current_error() ? RunLevel_BareMinimum
                                                                : RunLevel_Normal;
  services_set_runlevel(run_level);

  // emulate a button press-and-release to turn on/off the backlight
  light_button_pressed();
  light_button_released();

#ifndef RECOVERY_FW
  i18n_set_resource(RESOURCE_ID_STRINGS);
#endif
  app_manager_start_first_app();

#ifndef RECOVERY_FW
  // Launch the default worker. If any of the buttons are down, or we hit 2 strikes already,
  // skip this. This insures that we don't enter PRF for a bad worker.
  if (launcher_panic_get_current_error()) {
    PBL_LOG(LOG_LEVEL_INFO, "Not launching worker because launcher panic");
  } else if (button_get_state_bits() != 0) {
    PBL_LOG(LOG_LEVEL_INFO, "Not launching worker because button held");
  } else if (boot_bit_test(BOOT_BIT_FW_START_FAIL_STRIKE_TWO)) {
    PBL_LOG(LOG_LEVEL_INFO, "Not launching worker because of 2 strikes");
  } else {
    process_manager_launch_process(&(ProcessLaunchConfig) {
      .id = worker_manager_get_default_install_id(),
      .worker = true,
    });
  }
#endif

  notify_system_ready_for_communication();
  serial_console_enable_prompt();
}

void launcher_main_loop(void) {
  PBL_LOG(LOG_LEVEL_ALWAYS, "Starting Launcher");

  prv_launcher_main_loop_init();

  while (1) {
    task_watchdog_bit_set(PebbleTask_KernelMain);

    // We make this PebbleEvent static to save stack space
    static PebbleEvent e;
    if (event_take_timeout(&e, 1000)) {
      const PebbleTaskBitset kernel_main_task_bit = (1 << PebbleTask_KernelMain);
      const bool is_not_masked_out_from_kernel_main = !(e.task_mask & kernel_main_task_bit);
      if (is_not_masked_out_from_kernel_main) {
        prv_handle_event(&e);
      }

      event_service_handle_event(&e);

      event_cleanup(&e);

      mcu_fpu_cleanup();
      event_loop_upkeep();
    }
  }

  __builtin_unreachable();
}
