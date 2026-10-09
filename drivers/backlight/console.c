/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL
#include <errno.h>
#include <stdlib.h>

#include <pbl/drivers/backlight.h>
#include <pbl/shell/shell.h>

static int prv_cmd_level(const struct pbl_shell *sh, size_t argc, char **argv) {
  long bright_percent;

  if (pbl_shell_strtol(argv[1], &bright_percent) != 0 || bright_percent < 0 ||
      bright_percent > 100) {
    pbl_shell_error(sh, "invalid brightness '%s'", argv[1]);
    return -EINVAL;
  }

  backlight_set_brightness((uint8_t)bright_percent);
  return 0;
}

#ifdef CONFIG_BACKLIGHT_HAS_COLOR
static int prv_cmd_color(const struct pbl_shell *sh, size_t argc, char **argv) {
  char *end;
  unsigned long color_val = strtoul(argv[1], &end, 16);

  if (*argv[1] == '\0' || *end != '\0') {
    pbl_shell_error(sh, "invalid color '%s'", argv[1]);
    return -EINVAL;
  }

  backlight_set_color((uint32_t)color_val);
  return 0;
}
#endif

PBL_SHELL_SUBCMD_SET_CREATE(sub_backlight);
PBL_SHELL_CMD_REGISTER(backlight, sub_backlight, "Backlight control", nullptr);

PBL_SHELL_SUBCMD_ADD(sub_backlight, level, nullptr, "Set the brightness <0-100>", prv_cmd_level, 2,
                     0);
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
PBL_SHELL_SUBCMD_ADD(sub_backlight, color, nullptr, "Set the color <rrggbb>", prv_cmd_color, 2, 0);
#endif
#endif
