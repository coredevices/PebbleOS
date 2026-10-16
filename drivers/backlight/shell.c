/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>

#include <pbl/drivers/backlight.h>
#include <pbl/shell/shell.h>

static int prv_cmd_level(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long brightness;
  int ret;

  if (pbl_shell_strtoul(argv[1], &brightness) != 0 || brightness > PBL_BACKLIGHT_BRIGHTNESS_MAX) {
    pbl_shell_error(sh, "invalid brightness '%s'", argv[1]);
    return -EINVAL;
  }

  ret = pbl_backlight_set_brightness(BACKLIGHT, (uint8_t)brightness);
  if (ret != 0) {
    pbl_shell_error(sh, "set failed (%d)", ret);
  }

  return ret;
}

#ifdef CONFIG_BACKLIGHT_HAS_COLOR
static int prv_cmd_color(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long color;
  char *end;
  int ret;

  if (argc == 1) {
    pbl_shell_print(sh, "%06" PRIx32, pbl_backlight_get_color(BACKLIGHT));
    return 0;
  }

  color = strtoul(argv[1], &end, 16);
  if (*argv[1] == '\0' || *end != '\0' || color > 0xFFFFFFUL) {
    pbl_shell_error(sh, "invalid color '%s'", argv[1]);
    return -EINVAL;
  }

  ret = pbl_backlight_set_color(BACKLIGHT, (uint32_t)color);
  if (ret != 0) {
    pbl_shell_error(sh, "set failed (%d)", ret);
  }

  return ret;
}
#endif

PBL_SHELL_SUBCMD_SET_CREATE(sub_backlight);
PBL_SHELL_CMD_REGISTER(backlight, sub_backlight, "Backlight control", nullptr);

PBL_SHELL_SUBCMD_ADD(sub_backlight, level, nullptr, "Set the brightness <0-100>", prv_cmd_level, 2,
                     0);
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
PBL_SHELL_SUBCMD_ADD(sub_backlight, color, nullptr, "Get or set the color [rrggbb]", prv_cmd_color,
                     1, 1);
#endif
