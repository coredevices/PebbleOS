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

// Guards the starts and stops of the stack, and what decides them.
static PBL_MUTEX_DEFINE(s_lock);
static bool s_initialized;
static bool s_enabled;
static bool s_airplane_mode;
// Airplane mode from the shell, which is not persisted.
static bool s_forced_off;
static bool s_running;
static bool s_update_scheduled;

bool bt_ctl_is_airplane_mode_on(void) {
  return s_airplane_mode;
}

bool bt_ctl_is_bluetooth_active(void) {
  return s_enabled && !s_airplane_mode && !s_forced_off;
}

bool bt_ctl_is_bluetooth_running(void) {
  return s_running;
}

static void prv_start(void) {
  struct pbl_bt_sm_key root_keys[PBL_BT_SM_ROOT_KEY_TYPE_NUM];

  if (s_running) {
    return;
  }

  // Without root keys the stack would run with an identity anyone can resolve.
  if (bt_bonding_get_root_keys(root_keys) == 0) {
    s_running = (pbl_bt_start(&root_keys[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY]) == 0);
  }

  if (!s_running) {
    PBL_LOG_ERR("Bluetooth failed to start");
    return;
  }

  bt_link_init();
  bt_advertising_init();
  bt_gateway_init();
  bt_local_id_handle_start();
#ifdef CONFIG_BT_SERVICE_BAS
  bt_battery_level_start();
#endif
  bt_pairability_init();
}

static void prv_stop(void) {
  if (!s_running) {
    return;
  }

#ifdef CONFIG_BT_SERVICE_BAS
  bt_battery_level_stop();
#endif
  bt_link_deinit();
  bt_advertising_deinit();

  // The last thing to happen that touches the controller.
  pbl_bt_stop();
  s_running = false;
}

static void prv_put_state_event(void) {
  PebbleEvent event = {
    .type = PEBBLE_BT_STATE_EVENT,
    .bluetooth = {
      .state = {
        .airplane = s_airplane_mode,
        .enabled = s_enabled,
      },
    },
  };

  event_put(&event);
  if (s_airplane_mode) {
    PBL_ANALYTICS_TIMER_STOP(connectivity_connected_time_ms);
    PBL_ANALYTICS_TIMER_STOP(connectivity_expected_time_ms);
  } else {
    PBL_ANALYTICS_TIMER_START(connectivity_expected_time_ms);
  }
}

// Brings the stack to where it is wanted.
static void prv_update(void *context) {
  bool active;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_update_scheduled = false;
  active = bt_ctl_is_bluetooth_active();
  if (active != s_running) {
    if (active) {
      prv_start();
    } else {
      prv_stop();
    }

    if (active == s_running) {
      prv_put_state_event();
    }
  }
  pbl_mutex_unlock(&s_lock);
}

void bt_ctl_set_enabled(bool enabled) {
  if (!s_initialized) {
    PBL_LOG_ERR("Bluetooth is not initialized yet");
    return;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_enabled = enabled;
  pbl_mutex_unlock(&s_lock);

  prv_update(nullptr);
}

void bt_ctl_set_airplane_mode_async(bool enabled) {
  bool schedule;

  if (!s_initialized) {
    PBL_LOG_ERR("Bluetooth is not initialized yet");
    return;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
#ifdef CONFIG_SETTINGS
  bt_settings_set_airplane_mode(enabled);
#endif
  s_airplane_mode = enabled;
  schedule = !s_update_scheduled;
  s_update_scheduled = true;
  pbl_mutex_unlock(&s_lock);

  if (schedule) {
    pbl_sys_work_call(prv_update, nullptr);
  }
}

void bt_ctl_init(void) {
  bt_dis_init();
#ifdef CONFIG_SETTINGS
  s_airplane_mode = bt_settings_get_airplane_mode();
#endif
  s_initialized = true;
}

static void prv_reset(void *context) {
  bool was_running;

  PBL_LOG_DBG("Resetting Bluetooth");

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  was_running = s_running;
  prv_stop();
  prv_start();
  // A stack that had failed to start was never reported as running.
  if (!was_running && s_running) {
    prv_put_state_event();
  }
  pbl_mutex_unlock(&s_lock);
}

void bt_ctl_reset_bluetooth(void) {
  if (bt_ctl_is_bluetooth_active()) {
    pbl_sys_work_call(prv_reset, nullptr);
  } else {
    PBL_LOG_DBG("Bluetooth is disabled, reset aborted");
  }
}

#ifdef CONFIG_SHELL
#include <errno.h>

#include <pbl/shell/shell.h>

static int prv_cmd_airplane(const struct pbl_shell *sh, size_t argc, char **argv) {
  bool on;

  if (strcmp(argv[1], "on") == 0) {
    on = true;
  } else if (strcmp(argv[1], "off") == 0) {
    on = false;
    // Also leave the airplane mode set from the UI, which persists across reboots.
    bt_ctl_set_airplane_mode_async(false);
  } else {
    pbl_shell_error(sh, "invalid mode '%s'", argv[1]);
    return -EINVAL;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_forced_off = on;
  pbl_mutex_unlock(&s_lock);
  prv_update(nullptr);

  pbl_shell_print(sh, "%s airplane mode", bt_ctl_is_bluetooth_active() ? "Left" : "Entered");
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, airplane, nullptr, "Airplane mode <on|off>", prv_cmd_airplane, 2, 0);
#endif
