/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/input/input.h>
#include <pbl/kernel/msgq.h>
#include <pbl/kernel/time.h>

#define LISTEN_S  30
#define QUEUE_LEN 16

/* Input callbacks may run in an interrupt: they queue, the main thread prints. */
static PBL_MSGQ_DEFINE(s_events, sizeof(struct pbl_input_event), QUEUE_LEN);

static void prv_input_cb(const struct pbl_input_event *evt, void *user_data) {
  (void)pbl_msgq_put(&s_events, evt, PBL_NO_WAIT);
}

PBL_INPUT_CALLBACK_DEFINE(prv_input_cb, nullptr);

static void prv_print(const struct pbl_input_event *evt) {
  switch (evt->type) {
    case PBL_INPUT_EV_KEY:
      printf("input: key %d %s\n", evt->code, evt->value != 0 ? "pressed" : "released");
      break;
    case PBL_INPUT_EV_ABS:
      printf("input: axis %d = %d%s\n", evt->code, (int)evt->value, evt->sync ? ", sync" : "");
      break;
    case PBL_INPUT_EV_GES:
      printf("input: gesture %d\n", evt->code);
      break;
    default:
      printf("input: type %d code %d value %d\n", evt->type, evt->code, (int)evt->value);
      break;
  }
}

int main(void) {
  uint64_t end = pbl_uptime_ms() + LISTEN_S * 1000U;
  struct pbl_input_event evt;
  int count = 0;

  printf("input: printing events for %d s\n", LISTEN_S);

  while (true) {
    uint64_t now = pbl_uptime_ms();
    int ret;

    if (now >= end) {
      break;
    }

    ret = pbl_msgq_get(&s_events, &evt, PBL_MSEC((uint32_t)(end - now)));
    if (ret == -EAGAIN) {
      break;
    } else if (ret != 0) {
      fprintf(stderr, "input: no event (%d)\n", ret);
      return ret;
    }

    prv_print(&evt);
    count++;
  }

  printf("input: %d events\n", count);
  printf("Sample done\n");

  return 0;
}
