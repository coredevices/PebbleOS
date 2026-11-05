/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/touch.h>
#include <pbl/shell/shell.h>

static bool s_shell_enabled;

static int prv_cmd_info(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_touch_props *props = &TOUCH->props;

  pbl_shell_print(sh, "device: %s (%s)", TOUCH->dev.name,
                  pbl_device_is_ready(&TOUCH->dev) ? "ready" : "not ready");
  pbl_shell_print(sh, "size: %ux%u", props->size_x, props->size_y);
  pbl_shell_print(sh, "inverted: x=%d y=%d, swapped: %d", props->inverted_x, props->inverted_y,
                  props->swapped_x_y);
  pbl_shell_print(sh, "users: %u", TOUCH->state->users);

  return 0;
}

static int prv_cmd_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_touch_sample sample;

  if (!pbl_device_is_ready(&TOUCH->dev)) {
    pbl_shell_error(sh, "not ready");
    return -ENODEV;
  }

  pbl_touch_get_last(TOUCH, &sample);
  pbl_shell_print(sh, "%s x=%d y=%d", sample.down ? "down" : "up", sample.x, sample.y);

  return 0;
}

static int prv_cmd_enable(const struct pbl_shell *sh, size_t argc, char **argv) {
  int ret;

  if (s_shell_enabled) {
    return 0;
  }

  ret = pbl_touch_enable(TOUCH);
  if (ret != 0) {
    pbl_shell_error(sh, "enable failed (%d)", ret);
    return ret;
  }

  s_shell_enabled = true;

  return 0;
}

static int prv_cmd_disable(const struct pbl_shell *sh, size_t argc, char **argv) {
  int ret;

  if (!s_shell_enabled) {
    return 0;
  }

  s_shell_enabled = false;

  ret = pbl_touch_disable(TOUCH);
  if (ret != 0) {
    pbl_shell_error(sh, "disable failed (%d)", ret);
  }

  return ret;
}

PBL_SHELL_SUBCMD_SET_CREATE(sub_touch);
PBL_SHELL_CMD_REGISTER(touch, sub_touch, "Touchscreen", nullptr);

PBL_SHELL_SUBCMD_ADD(sub_touch, info, nullptr, "Show the touchscreen properties and users",
                     prv_cmd_info, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_touch, read, nullptr, "Show the last sample", prv_cmd_read, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_touch, enable, nullptr, "Hold the touchscreen powered up", prv_cmd_enable,
                     0, 0);
PBL_SHELL_SUBCMD_ADD(sub_touch, disable, nullptr, "Release the hold taken by enable",
                     prv_cmd_disable, 0, 0);
