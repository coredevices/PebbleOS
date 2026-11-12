/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/input/input.h>
#include <pbl/kernel/msgq.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>

#define LISTEN_S  30
#define QUEUE_LEN 16

/* Input callbacks may run in an interrupt: they queue, the main thread logs. */
static PBL_MSGQ_DEFINE(s_events, sizeof(struct pbl_input_event), QUEUE_LEN);

static void prv_input_cb(const struct pbl_input_event *evt, void *user_data) {
  (void)pbl_msgq_put(&s_events, evt, PBL_NO_WAIT);
}

PBL_INPUT_CALLBACK_DEFINE(prv_input_cb, nullptr);

static void prv_log(const struct pbl_input_event *evt) {
  switch (evt->type) {
    case PBL_INPUT_EV_KEY:
      PBL_LOG_ALWAYS("input: key %d %s", evt->code, evt->value != 0 ? "pressed" : "released");
      break;
    case PBL_INPUT_EV_ABS:
      PBL_LOG_ALWAYS("input: axis %d = %d%s", evt->code, (int)evt->value,
                     evt->sync ? ", sync" : "");
      break;
    case PBL_INPUT_EV_GES:
      PBL_LOG_ALWAYS("input: gesture %d", evt->code);
      break;
    default:
      PBL_LOG_ALWAYS("input: type %d code %d value %d", evt->type, evt->code, (int)evt->value);
      break;
  }
}

int main(void) {
  uint64_t end = pbl_uptime_ms() + LISTEN_S * 1000U;
  struct pbl_input_event evt;
  int count = 0;

  PBL_LOG_ALWAYS("input: logging events for %d s", LISTEN_S);

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
      PBL_LOG_ERR("input: no event (%d)", ret);
      return ret;
    }

    prv_log(&evt);
    count++;
  }

  PBL_LOG_ALWAYS("input: %d events", count);
  PBL_LOG_ALWAYS("Sample done");

  return 0;
}
