/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#include "services/pin_lock/pin_lock.h"

#include "applib/event_service_client.h"
#include "drivers/rng.h"
#include "kernel/events.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/settings/settings_file.h"
#include "syscall/syscall_internal.h"
#include "system/status_codes.h"
#include "util/attributes.h"
#include "util/sha2.h"
#include "util/units.h"
#ifdef CONFIG_HRM
#include "kernel/event_loop.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include "process_management/app_install_types.h"
#endif

#include <string.h>

#ifdef CONFIG_HRM
// How often to poll the HRM while watching for wrist removal. The optical
// sensor is opt-in here (it costs battery), so keep the cadence modest.
#define PIN_LOCK_WRIST_OFF_INTERVAL_S 5
#endif

#define PIN_LOCK_FILE_NAME "pinlock"
#define PIN_LOCK_FILE_LEN  KiBYTES(1)

// String keys for each record in the settings file
static const char PIN_KEY_CONFIG[] = "config";
static const char PIN_KEY_SALT[]   = "salt";
static const char PIN_KEY_HASH[]   = "hash";

// On-disk config record — only toggles, timeout, and length; never hash/salt
typedef struct PACKED {
  uint8_t  enabled;
  uint8_t  trigger_boot;
  uint8_t  trigger_timeout;
  uint8_t  trigger_bt_disconnect;
  uint16_t timeout_s;
  uint8_t  hide_notifications;
  uint8_t  hide_timeline;
  uint8_t  pin_len;
  uint8_t  mask_digits;
  uint8_t  haptic;
#ifdef CONFIG_HRM
  uint8_t  trigger_wrist_off;
#endif
} PinLockStored;

void pin_lock_compute_hash(const uint8_t *salt, const uint8_t *digits, uint8_t len,
                           uint8_t out_hash[PIN_LOCK_HASH_LEN]) {
  SHA256Context ctx;
  sha256_init(&ctx);
  sha256_update(&ctx, salt, PIN_LOCK_SALT_LEN);
  sha256_update(&ctx, digits, len);
  sha256_final(&ctx, out_hash);
}

bool pin_lock_hash_equal(const uint8_t *a, const uint8_t *b) {
  uint8_t diff = 0;
  for (unsigned i = 0; i < PIN_LOCK_HASH_LEN; i++) {
    diff |= (uint8_t)(a[i] ^ b[i]);
  }
  return diff == 0;
}

bool pin_lock_storage_load(PinLockConfig *out) {
  memset(out, 0, sizeof(*out));
  SettingsFile f;
  if (settings_file_open(&f, PIN_LOCK_FILE_NAME, PIN_LOCK_FILE_LEN) != S_SUCCESS) {
    return false;
  }
  PinLockStored stored = {};
  status_t st = settings_file_get(&f, PIN_KEY_CONFIG, sizeof(PIN_KEY_CONFIG),
                                  &stored, sizeof(stored));
  settings_file_close(&f);
  if (st != S_SUCCESS) {
    return true; // file opened fine, record just absent — return zeroed defaults
  }
  out->enabled               = stored.enabled;
  out->trigger_boot          = stored.trigger_boot;
  out->trigger_timeout       = stored.trigger_timeout;
  out->trigger_bt_disconnect = stored.trigger_bt_disconnect;
  out->timeout_s             = stored.timeout_s;
  out->hide_notifications    = stored.hide_notifications;
  out->hide_timeline         = stored.hide_timeline;
  out->pin_len               = stored.pin_len;
  out->mask_digits           = stored.mask_digits;
  out->haptic                = stored.haptic;
#ifdef CONFIG_HRM
  out->trigger_wrist_off     = stored.trigger_wrist_off;
#endif
  return true;
}

void pin_lock_storage_save_config(const PinLockConfig *config) {
  SettingsFile f;
  if (settings_file_open(&f, PIN_LOCK_FILE_NAME, PIN_LOCK_FILE_LEN) != S_SUCCESS) {
    return;
  }
  PinLockStored stored = {
    .enabled               = config->enabled,
    .trigger_boot          = config->trigger_boot,
    .trigger_timeout       = config->trigger_timeout,
    .trigger_bt_disconnect = config->trigger_bt_disconnect,
    .timeout_s             = config->timeout_s,
    .hide_notifications    = config->hide_notifications,
    .hide_timeline         = config->hide_timeline,
    .pin_len               = config->pin_len,
    .mask_digits           = config->mask_digits,
    .haptic                = config->haptic,
#ifdef CONFIG_HRM
    .trigger_wrist_off     = config->trigger_wrist_off,
#endif
  };
  settings_file_set(&f, PIN_KEY_CONFIG, sizeof(PIN_KEY_CONFIG), &stored, sizeof(stored));
  settings_file_close(&f);
}

void pin_lock_storage_set_pin(const uint8_t *digits, uint8_t len) {
  // Generate a fresh random salt (4 × uint32 → 16 bytes)
  uint8_t salt[PIN_LOCK_SALT_LEN];
  for (unsigned i = 0; i < PIN_LOCK_SALT_LEN / sizeof(uint32_t); i++) {
    uint32_t r = 0xDEADBEEFu; // weak fallback if rng fails
    rng_rand(&r);
    memcpy(salt + i * sizeof(uint32_t), &r, sizeof(uint32_t));
  }

  uint8_t hash[PIN_LOCK_HASH_LEN];
  pin_lock_compute_hash(salt, digits, len, hash);

  // Load current config to preserve existing toggles/timeout.
  // On a fresh enable (no prior record) seed mask_digits=true; existing
  // records already carry their stored value.
  PinLockConfig cfg;
  pin_lock_storage_load(&cfg);
  if (!cfg.enabled) {
    cfg.mask_digits = true;
    cfg.haptic = true;
  }
  cfg.enabled = true;
  cfg.pin_len = len;

  SettingsFile f;
  if (settings_file_open(&f, PIN_LOCK_FILE_NAME, PIN_LOCK_FILE_LEN) != S_SUCCESS) {
    return;
  }
  PinLockStored stored = {
    .enabled               = cfg.enabled,
    .trigger_boot          = cfg.trigger_boot,
    .trigger_timeout       = cfg.trigger_timeout,
    .trigger_bt_disconnect = cfg.trigger_bt_disconnect,
    .timeout_s             = cfg.timeout_s,
    .hide_notifications    = cfg.hide_notifications,
    .hide_timeline         = cfg.hide_timeline,
    .pin_len               = cfg.pin_len,
    .mask_digits           = cfg.mask_digits,
    .haptic                = cfg.haptic,
  };
  settings_file_set(&f, PIN_KEY_CONFIG, sizeof(PIN_KEY_CONFIG), &stored, sizeof(stored));
  settings_file_set(&f, PIN_KEY_SALT, sizeof(PIN_KEY_SALT), salt, sizeof(salt));
  settings_file_set(&f, PIN_KEY_HASH, sizeof(PIN_KEY_HASH), hash, sizeof(hash));
  settings_file_close(&f);
}

bool pin_lock_storage_verify_pin(const uint8_t *digits, uint8_t len) {
  SettingsFile f;
  if (settings_file_open(&f, PIN_LOCK_FILE_NAME, PIN_LOCK_FILE_LEN) != S_SUCCESS) {
    return false;
  }

  uint8_t salt[PIN_LOCK_SALT_LEN];
  uint8_t stored_hash[PIN_LOCK_HASH_LEN];
  status_t s1 = settings_file_get(&f, PIN_KEY_SALT, sizeof(PIN_KEY_SALT),
                                   salt, sizeof(salt));
  status_t s2 = settings_file_get(&f, PIN_KEY_HASH, sizeof(PIN_KEY_HASH),
                                   stored_hash, sizeof(stored_hash));
  settings_file_close(&f);

  if (s1 != S_SUCCESS || s2 != S_SUCCESS) {
    return false;
  }

  uint8_t computed[PIN_LOCK_HASH_LEN];
  pin_lock_compute_hash(salt, digits, len, computed);
  return pin_lock_hash_equal(computed, stored_hash);
}

void pin_lock_storage_clear(void) {
  SettingsFile f;
  if (settings_file_open(&f, PIN_LOCK_FILE_NAME, PIN_LOCK_FILE_LEN) != S_SUCCESS) {
    return;
  }
  // Wipe the PIN material
  settings_file_delete(&f, PIN_KEY_SALT, sizeof(PIN_KEY_SALT));
  settings_file_delete(&f, PIN_KEY_HASH, sizeof(PIN_KEY_HASH));
  // Update config: set enabled=false, preserve other toggles
  PinLockStored stored = {};
  settings_file_get(&f, PIN_KEY_CONFIG, sizeof(PIN_KEY_CONFIG), &stored, sizeof(stored));
  stored.enabled = false;
  settings_file_set(&f, PIN_KEY_CONFIG, sizeof(PIN_KEY_CONFIG), &stored, sizeof(stored));
  settings_file_close(&f);
}

// Runtime state
static struct {
  PinLockConfig config;
  bool locked;
} s_pin_lock;

static TimerID s_idle_timer = TIMER_INVALID_ID;
static EventServiceInfo s_button_sub;
static EventServiceInfo s_conn_sub;

static void prv_idle_timer_cb(void *unused) {
  pin_lock_handle_inactivity_timeout();
}

static void prv_button_event_handler(PebbleEvent *e, void *context) {
  pin_lock_handle_activity();
}

static void prv_conn_event_handler(PebbleEvent *e, void *context) {
  // Lock as soon as the system (Pebble app) phone session closes. We use the
  // raw comm-session event, not the debounced one: the debounced service hides
  // disconnects for 25 s, which is too long to leave a privacy lock open.
  if (e->bluetooth.comm_session_event.is_system &&
      !e->bluetooth.comm_session_event.is_open) {
    pin_lock_handle_bt_disconnected();
  }
}

void pin_lock_handle_activity(void) {
  // A zero timeout would re-arm a 0 ms timer that fires before the user can
  // navigate, re-locking instantly after every unlock. Treat it as no
  // inactivity auto-lock.
  if (!s_pin_lock.config.enabled || !s_pin_lock.config.trigger_timeout ||
      s_pin_lock.config.timeout_s == 0 || s_pin_lock.locked) {
    return;
  }
  if (s_idle_timer == TIMER_INVALID_ID) {
    s_idle_timer = new_timer_create();
  }
  new_timer_start(s_idle_timer, (uint32_t)s_pin_lock.config.timeout_s * 1000,
                  prv_idle_timer_cb, NULL, 0);
}

void pin_lock_handle_inactivity_timeout(void) {
  if (s_pin_lock.config.enabled && s_pin_lock.config.trigger_timeout) {
    s_pin_lock.locked = true;
  }
}

void pin_lock_handle_bt_disconnected(void) {
  if (s_pin_lock.config.enabled && s_pin_lock.config.trigger_bt_disconnect) {
    s_pin_lock.locked = true;
  }
}

#ifdef CONFIG_HRM
static HRMSessionRef s_hrm_session = HRM_INVALID_SESSION_REF;
static EventServiceInfo s_hrm_sub;
static bool s_hrm_on_wrist;  //!< last sampled wear state (worn vs off-wrist)

// For a KernelMain subscriber the HRM manager delivers updates as a
// PEBBLE_HRM_EVENT on this task's queue (the subscribe callback is only used by
// queue-less KernelBG clients), so we listen via the event service.
//
// Edge-triggered: lock only on the worn -> off-wrist transition. Locking on every
// off-wrist sample would re-lock within one poll after each unlock (and trap the
// user permanently) whenever the watch reads off-wrist.
static void prv_hrm_event_handler(PebbleEvent *e, void *context) {
  const PebbleHRMEvent *hrm = &e->hrm;
  if (hrm->event_type != HRMEvent_BPM) {
    return;
  }
  if (hrm->bpm.quality != HRMQuality_OffWrist) {
    s_hrm_on_wrist = true;
    return;
  }
  if (s_hrm_on_wrist) {
    s_hrm_on_wrist = false;
    if (s_pin_lock.config.enabled && s_pin_lock.config.trigger_wrist_off) {
      s_pin_lock.locked = true;
    }
  }
}

// Enable the HRM only while the wrist-off trigger is active; the optical sensor
// draws power, so it must not run otherwise. The callback is NULL on purpose:
// KernelMain clients receive updates via PEBBLE_HRM_EVENT (see handler above).
//
// Must run on KernelMain: hrm_manager binds the subscription's delivery queue to
// the calling task. Our PEBBLE_HRM_EVENT handler lives on KernelMain, so creating
// the subscription anywhere else (e.g. the Settings app via the reload syscall)
// would queue events to that task instead and leak an app-scoped subscription.
static void prv_sync_wrist_off_subscription(void *unused) {
  // Off-wrist detection drives the optical HR sensor, so it must respect both
  // Health master tracking and the heart-rate pref: without them the sensor
  // either must not run (user disabled Health) or never produces readings.
  const bool want = s_pin_lock.config.enabled && s_pin_lock.config.trigger_wrist_off &&
                    activity_prefs_tracking_is_enabled() &&
                    activity_prefs_heart_rate_is_enabled();
  if (want && s_hrm_session == HRM_INVALID_SESSION_REF) {
    // Start from "off": require a confirmed worn sample before a removal can lock,
    // so enabling the trigger while off-wrist doesn't lock on the first sample.
    s_hrm_on_wrist = false;
    s_hrm_session = hrm_manager_subscribe_with_callback(
        INSTALL_ID_INVALID, PIN_LOCK_WRIST_OFF_INTERVAL_S, 0 /* expire_s */,
        HRMFeature_BPM, NULL, NULL);
  } else if (!want && s_hrm_session != HRM_INVALID_SESSION_REF) {
    sys_hrm_manager_unsubscribe(s_hrm_session);
    s_hrm_session = HRM_INVALID_SESSION_REF;
  }
}
#endif // CONFIG_HRM

void pin_lock_reload_config(void) {
  pin_lock_storage_load(&s_pin_lock.config);
#ifdef CONFIG_HRM
  // The reload syscall runs on the caller's task (the Settings app); bounce the
  // HRM (un)subscription onto KernelMain so its event queue matches our handler.
  if (launcher_task_is_current_task()) {
    prv_sync_wrist_off_subscription(NULL);
  } else {
    launcher_task_add_callback(prv_sync_wrist_off_subscription, NULL);
  }
#endif
}

void pin_lock_init(void) {
  pin_lock_reload_config();
  s_pin_lock.locked = s_pin_lock.config.enabled && s_pin_lock.config.trigger_boot;

  s_button_sub = (EventServiceInfo){
    .type = PEBBLE_BUTTON_DOWN_EVENT,
    .handler = prv_button_event_handler,
  };
  event_service_client_subscribe(&s_button_sub);

  s_conn_sub = (EventServiceInfo){
    .type = PEBBLE_COMM_SESSION_EVENT,
    .handler = prv_conn_event_handler,
  };
  event_service_client_subscribe(&s_conn_sub);

#ifdef CONFIG_HRM
  s_hrm_sub = (EventServiceInfo){
    .type = PEBBLE_HRM_EVENT,
    .handler = prv_hrm_event_handler,
  };
  event_service_client_subscribe(&s_hrm_sub);
#endif

  pin_lock_handle_activity();
}

bool pin_lock_is_locked(void) {
  return s_pin_lock.locked;
}

void pin_lock_lock_now(void) {
  if (s_pin_lock.config.enabled) {
    s_pin_lock.locked = true;
  }
}

void pin_lock_mark_unlocked(void) {
  s_pin_lock.locked = false;
}

// Syscalls: the Settings app runs in a separate (unprivileged) context and does
// not share the kernel-side runtime state. It persists config/PIN to flash
// directly, then calls these to apply the change to the live kernel state that
// the watchface gate and triggers read.
DEFINE_SYSCALL(void, sys_pin_lock_reload_config, void) {
  pin_lock_reload_config();
}

DEFINE_SYSCALL(void, sys_pin_lock_lock_now, void) {
  pin_lock_lock_now();
}

bool pin_lock_should_hide_notifications(void) {
  return s_pin_lock.locked && s_pin_lock.config.hide_notifications;
}

bool pin_lock_should_hide_timeline(void) {
  return s_pin_lock.locked && s_pin_lock.config.hide_timeline;
}

bool pin_lock_should_mask_digits(void) {
  // A fresh/disabled config defaults to masked.
  return s_pin_lock.config.enabled ? s_pin_lock.config.mask_digits : true;
}

bool pin_lock_should_haptic(void) {
  // A fresh/disabled config defaults to haptics on.
  return s_pin_lock.config.enabled ? s_pin_lock.config.haptic : true;
}

uint8_t pin_lock_get_pin_len(void) {
  return s_pin_lock.config.pin_len ? s_pin_lock.config.pin_len : PIN_LOCK_MIN_LEN;
}
