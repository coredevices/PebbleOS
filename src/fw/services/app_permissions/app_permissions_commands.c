/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "console/prompt.h"
#include "pbl/services/app_permissions/app_permissions.h"
#include "pbl/services/blob_db/app_permissions_db.h"
#include "pbl/services/comm_session/session.h"
#include "process_management/app_install_manager.h"
#include "syscall/syscall.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static const char *s_permission_names[AppPermissionCount] = {
  [AppPermission_Microphone] = "mic",
};

static bool prv_parse_permission(const char *name, AppPermission *permission_out) {
  for (AppPermission p = 0; p < AppPermissionCount; p++) {
    if (strcmp(name, s_permission_names[p]) == 0) {
      *permission_out = p;
      return true;
    }
  }
  return false;
}

static bool prv_list_record(const Uuid *uuid, const AppPermissionsDBEntry *entry, void *context) {
  char uuid_str[UUID_STRING_BUFFER_LENGTH];
  uuid_to_string(uuid, uuid_str);
  char buffer[96];
  prompt_send_response_fmt(
      buffer, sizeof(buffer), "%s id=%" PRId32 " granted=0x%" PRIx32 " declared=0x%" PRIx32,
      uuid_str, app_install_get_id_for_uuid(uuid), entry->granted_mask, entry->declared_mask);
  return true;
}

void command_perm_list(void) {
  char buffer[64];
  prompt_send_response_fmt(
      buffer, sizeof(buffer), "phone support: %s",
      sys_system_pp_has_capability(CommSessionAppPermissionsSupport) ? "yes" : "no");
  app_permissions_db_each(prv_list_record, NULL);
  prompt_send_response("OK");
}

static void prv_set(const char *id_str, const char *perm_str, bool granted) {
  const AppInstallId id = atoi(id_str);
  Uuid uuid;
  if ((id == INSTALL_ID_INVALID) || !app_install_get_uuid_for_install_id(id, &uuid)) {
    prompt_send_response("No app with id");
    return;
  }
  AppPermission permission;
  if (!prv_parse_permission(perm_str, &permission)) {
    prompt_send_response("Unknown permission (try: mic)");
    return;
  }
  if (app_permissions_set_granted(&uuid, permission, granted) != S_SUCCESS) {
    prompt_send_response("Failed");
    return;
  }
  prompt_send_response("OK");
}

void command_perm_grant(const char *id_str, const char *perm_str) {
  prv_set(id_str, perm_str, true);
}

void command_perm_revoke(const char *id_str, const char *perm_str) {
  prv_set(id_str, perm_str, false);
}
