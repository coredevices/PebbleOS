/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/mutex.h>
#include <pbl/services/app_session.h>
#include <pbl/settings/settings.h>
#include <pbl/util/units.h>

#include <kernel/events.h>
#include <process_management/app_install_types.h>
#include <process_management/app_manager.h>

#define APP_SESSION_CAPABILITIES_CACHE_FILENAME "/app_comm"

#define APP_SESSION_CAPABILITIES_CACHE_FILE_MAX_USED_SPACE (PBL_KIB(2))

static PBL_MUTEX_DEFINE(s_mutex);

static int prv_open(struct pbl_settings_file *settings_file) {
  return pbl_settings_open(settings_file, APP_SESSION_CAPABILITIES_CACHE_FILENAME,
                           APP_SESSION_CAPABILITIES_CACHE_FILE_MAX_USED_SPACE);
}

static int prv_open_locked(struct pbl_settings_file *settings_file) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  int ret = prv_open(settings_file);
  if (ret < 0) {
    pbl_mutex_unlock(&s_mutex);
  }
  return ret;
}

static void prv_close_and_unlock(struct pbl_settings_file *settings_file) {
  pbl_settings_close(settings_file);
  pbl_mutex_unlock(&s_mutex);
}

bool app_session_has_capability(enum pbl_pp_capability capability) {
  struct pbl_pp_session *app_session = app_session_get_current();

  const Uuid app_uuid = app_manager_get_current_app_md()->uuid;

  struct pbl_settings_file settings_file;
  bool file_open = prv_open_locked(&settings_file) == 0;

  uint64_t cached_capabilities = 0;
  if (file_open) {
    pbl_settings_get(&settings_file, &app_uuid, sizeof(app_uuid), &cached_capabilities,
                     sizeof(cached_capabilities));
  }

  uint64_t new_capabilities = cached_capabilities;
  if (app_session) {
    // Connected, grab fresh capabilities data:
    new_capabilities = pbl_pp_session_get_capabilities(app_session);

    if (file_open && (new_capabilities != cached_capabilities)) {
      pbl_settings_set(&settings_file, &app_uuid, sizeof(app_uuid), &new_capabilities,
                       sizeof(new_capabilities));
    }
  }

  if (file_open) {
    prv_close_and_unlock(&settings_file);
  }

  return ((new_capabilities & capability) != 0);
}

static void prv_rewrite_cb(struct pbl_settings_file *old_file, struct pbl_settings_file *new_file,
                           struct pbl_settings_record *info, void *context) {
  if (!info->val_len) {
    return; // Cache for this app has been deleted, don't rewrite it
  }
  Uuid key;
  uint64_t val;
  pbl_settings_read_key(old_file, &key, sizeof(key));
  pbl_settings_read_val(old_file, &val, sizeof(val));
  pbl_settings_set(new_file, &key, sizeof(key), &val, sizeof(val));
}

void app_session_capabilities_evict(const Uuid *app_uuid) {
  struct pbl_settings_file settings_file;
  if (prv_open_locked(&settings_file) == 0) {
    pbl_settings_delete(&settings_file, app_uuid, sizeof(*app_uuid));
    prv_close_and_unlock(&settings_file);
  }
}

// The capabilities of the system session are kept with those of the apps, under a UUID no app
// has.
static const Uuid s_system_key = UUID_INVALID_INIT;
static struct pbl_pp_capabilities s_system_capabilities;

void app_session_get_system_capabilities(struct pbl_pp_capabilities *capabilities_out) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  *capabilities_out = s_system_capabilities;
  pbl_mutex_unlock(&s_mutex);
}

static void prv_set_system_capabilities(const struct pbl_pp_capabilities *capabilities) {
  struct pbl_settings_file settings_file;
  struct pbl_pp_capabilities diff;

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  diff.flags = s_system_capabilities.flags ^ capabilities->flags;
  s_system_capabilities = *capabilities;
  if (diff.flags != 0U && prv_open(&settings_file) == 0) {
    pbl_settings_set(&settings_file, &s_system_key, sizeof(s_system_key), capabilities,
                     sizeof(*capabilities));
    pbl_settings_close(&settings_file);
  }
  pbl_mutex_unlock(&s_mutex);

  if (diff.flags == 0U) {
    return;
  }

  PebbleEvent event = {
    .type = PEBBLE_CAPABILITIES_CHANGED_EVENT,
    .capabilities.flags_diff = diff,
  };
  event_put(&event);
}

static void prv_session_cb(struct pbl_pp_session *session, enum pbl_pp_session_event event,
                           void *user_data) {
  if (event == PBL_PP_SESSION_CAPABILITIES) {
    const struct pbl_pp_capabilities capabilities = {
      .flags = pbl_pp_session_get_capabilities(session),
    };

    prv_set_system_capabilities(&capabilities);
  }
}

PBL_PP_SESSION_LISTENER_DEFINE(app_session_capabilities, prv_session_cb, nullptr);

void app_session_capabilities_init(void) {
  struct pbl_settings_file settings_file;
  if (prv_open_locked(&settings_file) == 0) {
    pbl_settings_rewrite(&settings_file, prv_rewrite_cb, nullptr);
    pbl_settings_get(&settings_file, &s_system_key, sizeof(s_system_key), &s_system_capabilities,
                     sizeof(s_system_capabilities));
    prv_close_and_unlock(&settings_file);
  }
}
