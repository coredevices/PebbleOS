/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/touch.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/time.h>

#define LISTEN_S 10
#define POLL_MS  20

static const char *prv_yes_no(bool value) {
  return value ? "yes" : "no";
}

int main(void) {
  const struct pbl_touch_props *props = pbl_touch_get_props(TOUCH);
  struct pbl_touch_sample prev = {};
  uint64_t end;
  int touches = 0;
  int ret;

  /* The coordinates come mapped to the display: the properties say how the panel is mounted. */
  printf("touch: %dx%d, inverted x: %s, inverted y: %s, swapped: %s\n", props->size_x,
         props->size_y, prv_yes_no(props->inverted_x), prv_yes_no(props->inverted_y),
         prv_yes_no(props->swapped_x_y));

  /* Enabling counts users: the controller stays powered until the last one disables it. */
  ret = pbl_touch_enable(TOUCH);
  if (ret != 0) {
    fprintf(stderr, "touch: enable failed (%d)\n", ret);
    return ret;
  }

  printf("touch: listening for %d s\n", LISTEN_S);

  /*
   * Touches also go to the input subsystem, as events: polling the last sample is enough to
   * follow a finger here.
   */
  end = pbl_uptime_ms() + LISTEN_S * 1000U;
  while (pbl_uptime_ms() < end) {
    struct pbl_touch_sample now;

    pbl_touch_get_last(TOUCH, &now);
    if (now.down && !prev.down) {
      printf("touch: down at %d,%d\n", now.x, now.y);
      touches++;
    } else if (!now.down && prev.down) {
      printf("touch: up at %d,%d\n", prev.x, prev.y);
    }
    prev = now;

    pbl_sleep_ms(POLL_MS);
  }

  printf("touch: %d touches\n", touches);

  ret = pbl_touch_disable(TOUCH);
  if (ret != 0) {
    fprintf(stderr, "touch: disable failed (%d)\n", ret);
    return ret;
  }

  printf("touch: enabled after disable: %s\n", prv_yes_no(pbl_touch_is_enabled(TOUCH)));
  printf("Sample done\n");

  return 0;
}
