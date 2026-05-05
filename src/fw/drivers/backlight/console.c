/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>

#include "console/prompt.h"
#include "drivers/backlight.h"

void command_backlight_ctl(const char *arg) {
  const int bright_percent = atoi(arg);
  if (bright_percent < 0 || bright_percent > 100) {
    prompt_send_response("Invalid Brightness");
    return;
  }

  backlight_set_brightness(bright_percent);
  prompt_send_response("OK");
}

#ifdef CONFIG_BACKLIGHT_HAS_COLOR
void command_backlight_set_color(const char *color) {
  uint32_t color_val = strtol(color, NULL, 16);

  backlight_set_color(color_val);
}
#endif