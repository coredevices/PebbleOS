/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if defined(CONFIG_SHELL) && !defined(CONFIG_RECOVERY_FW)
#include <pbl/shell/shell.h>

#include <inttypes.h>

#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/data_logging/dls_list.h"

static bool prv_list_cb(DataLoggingSession *session, void *data) {
  const struct pbl_shell *sh = data;

  pbl_shell_print(
      sh, "session_id : %" PRIu8 ", tag: %" PRIu32 ", bytes: %" PRIu32 ", write_offset: %" PRIu32,
      session->comm.session_id, session->tag, session->storage.num_bytes,
      session->storage.write_offset);
  return true;
}

static int prv_cmd_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  dls_list_for_each_session(prv_list_cb, (void *)sh);
  return 0;
}

static int prv_cmd_wipe(const struct pbl_shell *sh, size_t argc, char **argv) {
  dls_clear();
  return 0;
}

static int prv_cmd_send(const struct pbl_shell *sh, size_t argc, char **argv) {
  dls_send_all_sessions();
  return 0;
}

static const struct pbl_shell_cmd sub_dls[] = {
  PBL_SHELL_CMD(list, NULL, "List the sessions", prv_cmd_list),
  PBL_SHELL_CMD(wipe, NULL, "Erase all sessions", prv_cmd_wipe),
  PBL_SHELL_CMD(send, NULL, "Send all sessions to the phone", prv_cmd_send),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(dls, sub_dls, "Data logging", NULL);
#endif
