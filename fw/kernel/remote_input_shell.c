/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL

#include <errno.h>

#include <pbl/shell/shell.h>

#include <kernel/remote_input.h>

static int prv_press(const struct pbl_shell *sh, const char *button_str, const char *presses_str,
                     const char *hold_ms_str, const char *gap_ms_str) {
  unsigned long button;
  unsigned long presses = 1;
  unsigned long hold_ms = 0;
  unsigned long gap_ms = 0;

  if (pbl_shell_strtoul(button_str, &button) != 0 || button >= NUM_BUTTONS) {
    pbl_shell_error(sh, "invalid button '%s'", button_str);
    return -EINVAL;
  }

  if (presses_str != NULL && pbl_shell_strtoul(presses_str, &presses) != 0) {
    pbl_shell_error(sh, "invalid count '%s'", presses_str);
    return -EINVAL;
  }

  if (hold_ms_str != NULL && pbl_shell_strtoul(hold_ms_str, &hold_ms) != 0) {
    pbl_shell_error(sh, "invalid hold time '%s'", hold_ms_str);
    return -EINVAL;
  }

  if (gap_ms_str != NULL && pbl_shell_strtoul(gap_ms_str, &gap_ms) != 0) {
    pbl_shell_error(sh, "invalid delay '%s'", gap_ms_str);
    return -EINVAL;
  }

  switch (remote_input_button_press((ButtonId)button, presses, hold_ms, gap_ms)) {
    case RemoteInputResult_Ok:
      return 0;
    case RemoteInputResult_Busy:
      pbl_shell_error(sh, "busy");
      return -EBUSY;
    case RemoteInputResult_Invalid:
    default:
      pbl_shell_error(sh, "invalid request");
      return -EINVAL;
  }
}

static int prv_cmd_click(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_press(sh, argv[1], NULL, NULL, NULL);
}

static int prv_cmd_hold(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_press(sh, argv[1], NULL, argv[2], NULL);
}

static int prv_cmd_multi(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_press(sh, argv[1], argv[2], argv[3], argv[4]);
}

PBL_SHELL_SUBCMD_SET_CREATE(sub_button);
PBL_SHELL_CMD_REGISTER(button, sub_button, "Buttons", NULL);

PBL_SHELL_SUBCMD_ADD(sub_button, click, NULL, "Click a button <btn>", prv_cmd_click, 2, 0);
PBL_SHELL_SUBCMD_ADD(sub_button, hold, NULL, "Press a button for a while <btn> <hold_ms>",
                     prv_cmd_hold, 3, 0);
PBL_SHELL_SUBCMD_ADD(sub_button, multi, NULL,
                     "Press a button repeatedly <btn> <count> <hold_ms> <delay_ms>", prv_cmd_multi,
                     5, 0);

#endif
