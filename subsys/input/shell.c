/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include <pbl/input/input.h>
#include <pbl/logging/logging.h>
#include <pbl/shell/shell.h>
#include <pbl/util/size.h>

PBL_LOG_MODULE_DEFINE(input, CONFIG_INPUT_LOG_LEVEL);

static const char *const s_type_names[] = {
  [PBL_INPUT_EV_KEY] = "key",
  [PBL_INPUT_EV_ABS] = "abs",
  [PBL_INPUT_EV_GES] = "ges",
};

static bool s_dump;

static void prv_dump_cb(const struct pbl_input_event *evt, void *user_data) {
  if (!s_dump) {
    return;
  }

  PBL_LOG_INFO("%s code %" PRIu16 " value %" PRId32 " sync %d", s_type_names[evt->type], evt->code,
               evt->value, evt->sync);
}

PBL_INPUT_CALLBACK_DEFINE(prv_dump_cb, NULL);

static int prv_cmd_dump(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (strcmp(argv[1], "on") == 0) {
    s_dump = true;
  } else if (strcmp(argv[1], "off") == 0) {
    s_dump = false;
  } else {
    pbl_shell_error(sh, "invalid state '%s'", argv[1]);
    return -EINVAL;
  }

  return 0;
}

static int prv_cmd_report(const struct pbl_shell *sh, size_t argc, char **argv) {
  enum pbl_input_type type;
  long code;
  long value;
  long sync = 1;

  for (type = 0; type < ARRAY_LENGTH(s_type_names); type++) {
    if (strcmp(argv[1], s_type_names[type]) == 0) {
      break;
    }
  }
  if (type == ARRAY_LENGTH(s_type_names)) {
    pbl_shell_error(sh, "invalid type '%s'", argv[1]);
    return -EINVAL;
  }

  if (pbl_shell_strtol(argv[2], &code) != 0 || code < 0 || code > UINT16_MAX) {
    pbl_shell_error(sh, "invalid code '%s'", argv[2]);
    return -EINVAL;
  }

  if (pbl_shell_strtol(argv[3], &value) != 0) {
    pbl_shell_error(sh, "invalid value '%s'", argv[3]);
    return -EINVAL;
  }

  if (argc > 4 && (pbl_shell_strtol(argv[4], &sync) != 0 || (sync != 0 && sync != 1))) {
    pbl_shell_error(sh, "invalid sync '%s'", argv[4]);
    return -EINVAL;
  }

  pbl_input_report(type, (uint16_t)code, (int32_t)value, sync == 1);
  return 0;
}

static const struct pbl_shell_cmd sub_input[] = {
  PBL_SHELL_CMD_ARG(dump, NULL, "Log every event <on|off>", prv_cmd_dump, 2, 0),
  PBL_SHELL_CMD_ARG(report, NULL, "Report an event <key|abs|ges> <code> <value> [sync=1]",
                    prv_cmd_report, 4, 1),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(input, sub_input, "Input events", NULL);
