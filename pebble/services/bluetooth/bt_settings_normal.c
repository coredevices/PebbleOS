/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/session.h>
#include <pbl/services/bluetooth/bt_settings.h>
#include <pbl/settings/settings.h>
#include <pbl/util/size.h>
#include <pbl/util/units.h>

#include <kernel/events.h>

PBL_LOG_MODULE_DECLARE(service_bluetooth, CONFIG_SERVICE_BLUETOOTH_LOG_LEVEL);

#define FILE_NAME "/bt_settings"
#define FILE_SIZE PBL_KIB(1)

#define LEGACY_FILE_NAME "/gap_bonding_db"
#define LEGACY_FILE_SIZE PBL_KIB(4)

static const char s_key_airplane[] = "airplane";
static const char s_key_unfaithful[] = "unfaithful";
static const char s_key_capabilities[] = "capabilities";

// The legacy keys include their terminator.
static const struct {
  const char *legacy_key;
  size_t legacy_key_len;
  const char *key;
} s_legacy_keys[] = {
  {"AIRPLANE_MODE", sizeof("AIRPLANE_MODE"), s_key_airplane},
  {"IS_UNFAITHFUL", sizeof("IS_UNFAITHFUL"), s_key_unfaithful},
  {"SYSTEM_CAPABILITIES", sizeof("SYSTEM_CAPABILITIES"), s_key_capabilities},
};

static PBL_MUTEX_DEFINE(s_mutex);
static struct pbl_pp_capabilities s_capabilities;

static int prv_get(const char *key, void *val, size_t val_len) {
  struct pbl_settings_file file;
  int ret;

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);

  ret = pbl_settings_open(&file, FILE_NAME, FILE_SIZE);
  if (ret != 0) {
    goto unlock;
  }

  if (pbl_settings_get_len(&file, key, strlen(key)) != val_len) {
    ret = -ENOENT;
  } else {
    ret = pbl_settings_get(&file, key, strlen(key), val, val_len);
  }

  pbl_settings_close(&file);

unlock:
  pbl_mutex_unlock(&s_mutex);

  return ret;
}

static void prv_set(const char *key, const void *val, size_t val_len) {
  struct pbl_settings_file file;
  int ret;

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);

  ret = pbl_settings_open(&file, FILE_NAME, FILE_SIZE);
  if (ret == 0) {
    ret = pbl_settings_set(&file, key, strlen(key), val, val_len);
    pbl_settings_close(&file);
  }

  pbl_mutex_unlock(&s_mutex);

  if (ret != 0) {
    PBL_LOG_ERR("Could not store %s (%d)", key, ret);
  }
}

static bool prv_get_bool(const char *key, bool default_value) {
  uint8_t value;

  if (prv_get(key, &value, sizeof(value)) != 0 || value > 1U) {
    return default_value;
  }

  return value != 0U;
}

// These settings were kept with the bondings, which the Bluetooth stack stores now.
static void prv_migrate(void) {
  struct pbl_settings_file legacy;
  struct pbl_fs_dirent entry;

  if (pbl_fs_stat(LEGACY_FILE_NAME, &entry) != 0) {
    return;
  }

  if (pbl_settings_open(&legacy, LEGACY_FILE_NAME, LEGACY_FILE_SIZE) == 0) {
    for (size_t i = 0; i < PBL_ARRAY_SIZE(s_legacy_keys); i++) {
      uint8_t val[sizeof(struct pbl_pp_capabilities)];
      size_t len;

      len = pbl_settings_get_len(&legacy, s_legacy_keys[i].legacy_key,
                                 s_legacy_keys[i].legacy_key_len);
      if (len == 0U || len > sizeof(val) ||
          pbl_settings_get(&legacy, s_legacy_keys[i].legacy_key, s_legacy_keys[i].legacy_key_len,
                           val, len) != 0) {
        continue;
      }

      prv_set(s_legacy_keys[i].key, val, len);
    }

    pbl_settings_close(&legacy);
  }

  (void)pbl_fs_unlink(LEGACY_FILE_NAME);
}

void bt_settings_init(void) {
  struct pbl_pp_capabilities capabilities = {};

  prv_migrate();

  (void)prv_get(s_key_capabilities, &capabilities, sizeof(capabilities));

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  s_capabilities = capabilities;
  pbl_mutex_unlock(&s_mutex);
}

bool bt_settings_get_airplane_mode(void) {
  return prv_get_bool(s_key_airplane, false);
}

void bt_settings_set_airplane_mode(bool enable) {
  const uint8_t value = enable;

  prv_set(s_key_airplane, &value, sizeof(value));
}

bool bt_settings_is_unfaithful(void) {
  return prv_get_bool(s_key_unfaithful, true);
}

void bt_settings_set_unfaithful(bool is_unfaithful) {
  const uint8_t value = is_unfaithful;

  PBL_LOG_INFO("Marking the watch as %s", is_unfaithful ? "unfaithful" : "faithful");
  prv_set(s_key_unfaithful, &value, sizeof(value));
}

void bt_settings_get_capabilities(struct pbl_pp_capabilities *capabilities_out) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  *capabilities_out = s_capabilities;
  pbl_mutex_unlock(&s_mutex);
}

void bt_settings_set_capabilities(const struct pbl_pp_capabilities *capabilities) {
  const struct pbl_pp_capabilities new_capabilities =
      capabilities != nullptr ? *capabilities : (struct pbl_pp_capabilities){};
  struct pbl_pp_capabilities diff;

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  diff.flags = s_capabilities.flags ^ new_capabilities.flags;
  s_capabilities = new_capabilities;
  pbl_mutex_unlock(&s_mutex);

  if (diff.flags == 0U) {
    return;
  }

  prv_set(s_key_capabilities, &new_capabilities, sizeof(new_capabilities));

  PebbleEvent event = {
    .type = PEBBLE_CAPABILITIES_CHANGED_EVENT,
    .capabilities.flags_diff = diff,
  };
  event_put(&event);
}

static void prv_session_cb(struct pbl_pp_session *session, enum pbl_pp_session_event event,
                           void *user_data) {
  if (event == PBL_PP_SESSION_CAPABILITIES && pbl_pp_session_is_system(session)) {
    const struct pbl_pp_capabilities capabilities = {
      .flags = pbl_pp_session_get_capabilities(session),
    };

    bt_settings_set_capabilities(&capabilities);
  }
}

PBL_PP_SESSION_LISTENER_DEFINE(bt_settings, prv_session_cb, nullptr);
