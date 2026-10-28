/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/display.h>
#include <pbl/shell/shell.h>
#include <pbl/util/misc.h>

static const char *const s_formats[] = {
  [PBL_DISPLAY_FORMAT_MONO] = "mono",
  [PBL_DISPLAY_FORMAT_ARGB2222] = "argb2222",
};

static int prv_cmd_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_display_class, dev) {
    const struct pbl_display *disp = container_of(dev, const struct pbl_display, dev);

    pbl_shell_print(sh, "%s: %ux%u %s%s%s", dev->name, disp->width, disp->height,
                    s_formats[disp->format], disp->rotated ? " mounted upside down" : "",
                    pbl_device_is_ready(dev) ? "" : " (not ready)");
  }

  return 0;
}

static int prv_report(const struct pbl_shell *sh, int ret) {
  if (ret != 0) {
    pbl_shell_error(sh, "failed (%d)", ret);
  }

  return ret;
}

static int prv_cmd_on(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_report(sh, pbl_display_enable(DISPLAY));
}

static int prv_cmd_off(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_report(sh, pbl_display_disable(DISPLAY));
}

static int prv_cmd_clear(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_report(sh, pbl_display_clear(DISPLAY));
}

static const struct pbl_shell_cmd sub_display[] = {
  PBL_SHELL_CMD(list, nullptr, "List the displays", prv_cmd_list),
  PBL_SHELL_CMD(on, nullptr, "Turn the panel on", prv_cmd_on),
  PBL_SHELL_CMD(off, nullptr, "Turn the panel off", prv_cmd_off),
  PBL_SHELL_CMD(clear, nullptr, "Clear the image held by the panel", prv_cmd_clear),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(display, sub_display, "Display", nullptr);
