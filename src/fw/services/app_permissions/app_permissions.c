/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/app_permissions/app_permissions.h"

#include "applib/event_service_client.h"
#include "kernel/events.h"
#include "pbl/services/blob_db/api.h"
#include "pbl/services/blob_db/app_permissions_db.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include "process_management/pebble_process_md.h"
#include <pbl/logging/logging.h>

#ifdef CONFIG_SERVICE_MIC_CAPTURE
#include "pbl/services/mic_capture/mic_capture_service.h"
#endif

PBL_LOG_MODULE_DEFINE(service_app_permissions, CONFIG_SERVICE_APP_PERMISSIONS_LOG_LEVEL);

static bool prv_md_declares(const PebbleProcessMd *md, AppPermission permission) {
  switch (permission) {
    case AppPermission_Microphone:
      // Watchfaces run unattended; they never get the microphone
      return md->uses_microphone && (md->process_type != ProcessTypeWatchface);
    case AppPermissionCount:
      break;
  }
  return false;
}

AppPermissionState app_permissions_get_state_for_app(const Uuid *uuid, bool declared,
                                                     AppPermission permission) {
  if (!uuid || (permission >= AppPermissionCount) || !declared) {
    return AppPermissionStateNotDeclared;
  }

  AppPermissionsDBEntry entry;
  if (app_permissions_db_get(uuid, &entry) != S_SUCCESS) {
#ifdef CONFIG_SHELL_SDK
    // No phone in the loop: let sideloaded apps use what they declare.
    return AppPermissionStateGranted;
#else
    return AppPermissionStateDenied;
#endif
  }

  return (entry.granted_mask & APP_PERMISSION_BIT(permission)) ? AppPermissionStateGranted
                                                               : AppPermissionStateDenied;
}

AppPermissionState app_permissions_get_state_for_current_app(AppPermission permission) {
  const PebbleProcessMd *md = app_manager_get_current_app_md();
  if (!md || (permission >= AppPermissionCount)) {
    return AppPermissionStateNotDeclared;
  }
  // Apps built into the firmware are trusted with everything.
  if (!md->is_unprivileged || app_install_id_from_system(app_manager_get_current_app_id())) {
    return AppPermissionStateGranted;
  }
  return app_permissions_get_state_for_app(&md->uuid, prv_md_declares(md, permission), permission);
}

bool app_permissions_is_granted_for_current_app(AppPermission permission) {
  return (app_permissions_get_state_for_current_app(permission) == AppPermissionStateGranted);
}

status_t app_permissions_set_granted(const Uuid *uuid, AppPermission permission, bool granted) {
  if (!uuid || (permission >= AppPermissionCount)) {
    return E_INVALID_ARGUMENT;
  }
  AppPermissionsDBEntry entry;
  if (app_permissions_db_get(uuid, &entry) != S_SUCCESS) {
    entry = (AppPermissionsDBEntry){.version = APP_PERMISSIONS_DB_ENTRY_VERSION};
  }
  const AppPermissionMask bit = APP_PERMISSION_BIT(permission);
  entry.declared_mask |= bit;
  if (granted) {
    entry.granted_mask |= bit;
  } else {
    entry.granted_mask &= ~bit;
  }
  return app_permissions_db_set(uuid, &entry);
}

static void prv_notify_current_app(void) {
  for (AppPermission permission = 0; permission < AppPermissionCount; permission++) {
    const AppPermissionState state = app_permissions_get_state_for_current_app(permission);
    if (state == AppPermissionStateNotDeclared) {
      continue;
    }
    PBL_LOG_DBG("Permission %u for current app is now %u", permission, state);
    PebbleEvent event = {
      .type = PEBBLE_APP_PERMISSION_EVENT,
      .app_permission = {
        .permission = permission,
        .state = state,
      },
    };
    event_put(&event);
  }
#ifdef CONFIG_SERVICE_MIC_CAPTURE
  mic_capture_service_handle_permission_changed();
#endif
}

static void prv_blob_db_event_handler(PebbleEvent *event, void *context) {
  const PebbleBlobDBEvent *blob_db_event = &event->blob_db;
  if (blob_db_event->db_id != BlobDBIdAppPermissions) {
    return;
  }

  const PebbleProcessMd *md = app_manager_get_current_app_md();
  if (!md) {
    return;
  }

  const bool affects_current_app = (blob_db_event->type == BlobDBEventTypeFlush) ||
                                   ((blob_db_event->key_len == UUID_SIZE) &&
                                    uuid_equal((const Uuid *)blob_db_event->key, &md->uuid));
  if (affects_current_app) {
    prv_notify_current_app();
  }
}

void app_permissions_init(void) {
  static EventServiceInfo s_blob_db_event_info = {
    .type = PEBBLE_BLOBDB_EVENT,
    .handler = prv_blob_db_event_handler,
  };
  event_service_client_subscribe(&s_blob_db_event_info);
}
