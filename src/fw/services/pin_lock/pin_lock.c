/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#include "services/pin_lock/pin_lock.h"

#include "drivers/rng.h"
#include "pbl/services/settings/settings_file.h"
#include "system/status_codes.h"
#include "util/attributes.h"
#include "util/sha2.h"
#include "util/units.h"

#include <string.h>

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

  // Load current config to preserve existing toggles/timeout
  PinLockConfig cfg;
  pin_lock_storage_load(&cfg);
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
