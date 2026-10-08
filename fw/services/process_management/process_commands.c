/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if defined(CONFIG_SHELL) && !defined(CONFIG_RECOVERY_FW)

#include <errno.h>
#include <inttypes.h>

#include <pbl/services/blob_db/app_db.h>
#include <pbl/shell/shell.h>

#include <process_management/app_install_manager.h>
#include <process_management/app_manager.h>

static int prv_parse_id(const struct pbl_shell *sh, const char *str, AppInstallId *id) {
  long val;
  if (pbl_shell_strtol(str, &val) != 0 || val == 0) {
    pbl_shell_error(sh, "invalid app number '%s'", str);
    return -EINVAL;
  }

  *id = (AppInstallId)val;
  return 0;
}

static int prv_cmd_app_remove(const struct pbl_shell *sh, size_t argc, char **argv) {
  AppInstallId id;
  int ret = prv_parse_id(sh, argv[1], &id);
  if (ret != 0) {
    return ret;
  }

  AppInstallEntry entry;
  if (!app_install_get_entry_for_install_id(id, &entry)) {
    pbl_shell_error(sh, "failed to get entry");
    return -ENOENT;
  }

  // should delete from blob db and fire off an event to AppInstallManager that does the rest
  app_db_delete((uint8_t *)&entry.uuid, sizeof(Uuid));
  pbl_shell_print(sh, "OK");
  return 0;
}

static bool prv_print_app_info(AppInstallEntry *entry, void *data) {
  const struct pbl_shell *sh = data;

  if (app_install_id_from_system(entry->install_id)) {
    return true;
  }

  char uuid_buffer[UUID_STRING_BUFFER_LENGTH];
  uuid_to_string(&entry->uuid, uuid_buffer);

  pbl_shell_print(sh, "%" PRIi32 ": %s %s", entry->install_id, entry->name, uuid_buffer);
  return true;
}

static int prv_cmd_app_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  app_install_enumerate_entries(prv_print_app_info, (void *)sh);
  return 0;
}

static int prv_cmd_app_launch(const struct pbl_shell *sh, size_t argc, char **argv) {
  AppInstallId id;
  int ret = prv_parse_id(sh, argv[1], &id);
  if (ret != 0) {
    return ret;
  }

  AppInstallEntry entry;
  if (!app_install_get_entry_for_install_id(id, &entry)) {
    pbl_shell_error(sh, "no app with id %" PRIi32, id);
    return -ENOENT;
  }

  app_manager_put_launch_app_event(&(AppLaunchEventConfig){.id = id});
  pbl_shell_print(sh, "OK");
  return 0;
}

static int prv_cmd_worker_launch(const struct pbl_shell *sh, size_t argc, char **argv) {
  AppInstallId id;
  int ret = prv_parse_id(sh, argv[1], &id);
  if (ret != 0) {
    return ret;
  }

  AppInstallEntry entry;
  if (!app_install_get_entry_for_install_id(id, &entry) || !app_install_entry_has_worker(&entry)) {
    pbl_shell_error(sh, "no worker with id %" PRIi32, id);
    return -ENOENT;
  }

  app_manager_put_launch_app_event(&(AppLaunchEventConfig){.id = id});
  pbl_shell_print(sh, "OK");
  return 0;
}

PBL_SHELL_SUBCMD_SET_CREATE(sub_app);
PBL_SHELL_CMD_REGISTER(app, sub_app, "Installed apps", NULL);
PBL_SHELL_SUBCMD_ADD(sub_app, list, NULL, "List installed apps", prv_cmd_app_list, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_app, launch, NULL, "Launch an app <id>", prv_cmd_app_launch, 2, 0);
PBL_SHELL_SUBCMD_ADD(sub_app, remove, NULL, "Remove an app <id>", prv_cmd_app_remove, 2, 0);

PBL_SHELL_SUBCMD_SET_CREATE(sub_worker);
PBL_SHELL_CMD_REGISTER(worker, sub_worker, "Background workers", NULL);
PBL_SHELL_SUBCMD_ADD(sub_worker, launch, NULL, "Launch the worker of an app <id>",
                     prv_cmd_worker_launch, 2, 0);

#endif
