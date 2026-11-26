/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/analytics/analytics.h>
#include <pbl/bluetooth/init.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/services/bluetooth/advertising.h>
#include <pbl/services/bluetooth/battery_level.h>
#include <pbl/services/bluetooth/bluetooth_ctl.h>
#include <pbl/services/bluetooth/bonding.h>
#include <pbl/services/bluetooth/bt_settings.h>
#include <pbl/services/bluetooth/dis.h>
#include <pbl/services/bluetooth/gateway.h>
#include <pbl/services/bluetooth/link.h>
#include <pbl/services/bluetooth/local_id.h>
#include <pbl/services/bluetooth/pairability.h>

#include <kernel/events.h>

PBL_LOG_MODULE_DEFINE(service_bluetooth, CONFIG_SERVICE_BLUETOOTH_LOG_LEVEL);

static bool s_comm_initialized = false;
static bool s_comm_airplane_mode_on = false;
static bool s_comm_enabled = false;
static bool s_comm_is_running = false;
static bool s_comm_state_change_eval_is_scheduled;
static BtCtlModeOverride s_comm_override = BtCtlModeOverrideNone;
static PBL_MUTEX_DEFINE(s_comm_state_change_mutex);

bool bt_ctl_is_airplane_mode_on(void) {
  return s_comm_airplane_mode_on;
}

bool bt_ctl_is_bluetooth_active(void) {
  if (s_comm_enabled) {
    if (s_comm_override == BtCtlModeOverrideRun) {
      return true;
    } else if (s_comm_override == BtCtlModeOverrideNone && !s_comm_airplane_mode_on) {
      return true;
    }
  }
  return false;
}

bool bt_ctl_is_bluetooth_running(void) {
  return s_comm_is_running;
}

static void prv_put_disconnection_event(void) {
  PebbleEvent event = (PebbleEvent){
    .type = PEBBLE_BT_CONNECTION_EVENT,
    .bluetooth.connection = {
      .state = PebbleBluetoothConnectionEventStateDisconnected,
    }
  };
  PBL_LOG_DBG("New BT Conn change event, We are now disconnected");
  event_put(&event);
}

static void prv_comm_start(void) {
  if (s_comm_is_running) {
    return;
  }
  struct pbl_bt_sm_key root_keys[PBL_BT_SM_ROOT_KEY_TYPE_NUM];

  // Without root keys the stack would run with an identity anyone can resolve.
  if (bt_bonding_get_root_keys(root_keys) == 0) {
    s_comm_is_running = (pbl_bt_start(&root_keys[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY]) == 0);
  }

  if (s_comm_is_running) {
    bt_link_init();
    bt_advertising_init();
    bt_gateway_init();
    bt_local_id_handle_start();
#ifdef CONFIG_BT_SERVICE_BAS
    bt_battery_level_start();
#endif
    bt_pairability_init();
  } else {
    PBL_LOG_ERR("Bluetooth failed to start");
  }
}

static void prv_comm_stop(void) {
  if (!s_comm_is_running) {
    return;
  }
#ifdef CONFIG_BT_SERVICE_BAS
  bt_battery_level_stop();
#endif
  bt_link_deinit();
  bt_advertising_deinit();

  // Should be the last thing to happen that touches the Bluetooth controller directly
  pbl_bt_stop();
  s_comm_is_running = false;

  // This is a legacy event used to update the Settings app.
  prv_put_disconnection_event();
}

static void prv_send_state_change_event(void) {
  PBL_LOG_DBG("----> Sending a BT state event");
  PebbleEvent event = {
    .type = PEBBLE_BT_STATE_EVENT,
    .bluetooth = {
      .state = {
        .airplane = s_comm_airplane_mode_on,
        .enabled = s_comm_enabled,
        .override = s_comm_override,
      },
    },
  };
  event_put(&event);
  if (s_comm_airplane_mode_on) {
    PBL_ANALYTICS_TIMER_STOP(connectivity_connected_time_ms);
    PBL_ANALYTICS_TIMER_STOP(connectivity_expected_time_ms);
  } else {
    PBL_ANALYTICS_TIMER_START(connectivity_expected_time_ms);
  }
}

static void prv_comm_state_change(void *context) {
  pbl_mutex_lock(&s_comm_state_change_mutex, PBL_FOREVER);
  s_comm_state_change_eval_is_scheduled = false;
  bool is_active_mode = bt_ctl_is_bluetooth_active();
  if (is_active_mode != s_comm_is_running) {
    if (is_active_mode) {
      prv_comm_start();
    } else {
      prv_comm_stop();
    }
    // Only send event if state changed successfully:
    if (is_active_mode == s_comm_is_running) {
      prv_send_state_change_event();
    }
  }

  pbl_mutex_unlock(&s_comm_state_change_mutex);
}

void bt_ctl_set_enabled(bool enabled) {
  if (!s_comm_initialized) {
    PBL_LOG_ERR("Error: Bluetooth isn't initialized yet");
    return;
  }
  pbl_mutex_lock(&s_comm_state_change_mutex, PBL_FOREVER);
  s_comm_enabled = enabled;
  pbl_mutex_unlock(&s_comm_state_change_mutex);
  prv_comm_state_change(nullptr);
}

void bt_ctl_set_override_mode(BtCtlModeOverride override) {
  if (!s_comm_initialized) {
    PBL_LOG_ERR("Error: Bluetooth isn't initialized yet");
    return;
  }
  pbl_mutex_lock(&s_comm_state_change_mutex, PBL_FOREVER);
  s_comm_override = override;
  pbl_mutex_unlock(&s_comm_state_change_mutex);
  prv_comm_state_change(nullptr);
}

void bt_ctl_set_airplane_mode_async(bool enabled) {
  if (!s_comm_initialized) {
    PBL_LOG_ERR("Error: Bluetooth isn't initialized yet");
    return;
  }
  pbl_mutex_lock(&s_comm_state_change_mutex, PBL_FOREVER);
#ifdef CONFIG_SETTINGS
  bt_settings_set_airplane_mode(enabled);
#endif
  s_comm_airplane_mode_on = enabled;
  bool should_schedule_eval = false;
  if (!s_comm_state_change_eval_is_scheduled) {
    should_schedule_eval = true;
    s_comm_state_change_eval_is_scheduled = true;
  }
  pbl_mutex_unlock(&s_comm_state_change_mutex);
  if (should_schedule_eval) {
    pbl_sys_work_call(prv_comm_state_change, nullptr);
  }
}

void bt_ctl_init(void) {
  bt_dis_init();
#ifdef CONFIG_SETTINGS
  s_comm_airplane_mode_on = bt_settings_get_airplane_mode();
#endif
  s_comm_initialized = true;
}

static void prv_bt_ctl_reset_bluetooth_callback(void *context) {
  PBL_LOG_DBG("Resetting Bluetooth");
  pbl_mutex_lock(&s_comm_state_change_mutex, PBL_FOREVER);

  bool was_already_running = s_comm_is_running;

  prv_comm_stop();
  prv_comm_start();

  // It's possible a reset was triggered because the stack failed to boot up
  // correctly in which case we have never generated an event about the stack
  // booting up. Don't bother sending events if we are just returning the stack
  // to the state it is already in
  if (!was_already_running && s_comm_is_running) {
    prv_send_state_change_event();
  }
  pbl_mutex_unlock(&s_comm_state_change_mutex);
}

void bt_ctl_reset_bluetooth(void) {
  if (bt_ctl_is_bluetooth_active()) {
    pbl_sys_work_call(prv_bt_ctl_reset_bluetooth_callback, nullptr);
  } else {
    PBL_LOG_DBG("Bluetooth is disabled, reset aborted");
  }
}

#ifdef CONFIG_SHELL
#include <errno.h>

#include <pbl/shell/shell.h>

static int prv_cmd_airplane(const struct pbl_shell *sh, size_t argc, char **argv) {
  BtCtlModeOverride override;

  if (strcmp(argv[1], "on") == 0) {
    override = BtCtlModeOverrideStop;
  } else if (strcmp(argv[1], "off") == 0) {
    override = BtCtlModeOverrideNone;
    // Also leave the airplane mode set from the UI, which persists across reboots.
    bt_ctl_set_airplane_mode_async(false);
  } else {
    pbl_shell_error(sh, "invalid mode '%s'", argv[1]);
    return -EINVAL;
  }

  bt_ctl_set_override_mode(override);
  pbl_shell_print(sh, "%s airplane mode", bt_ctl_is_bluetooth_active() ? "Left" : "Entered");
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, airplane, nullptr, "Airplane mode <on|off>", prv_cmd_airplane, 2, 0);
#endif
