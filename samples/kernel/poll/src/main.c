/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/msgq.h>
#include <pbl/kernel/poll.h>
#include <pbl/kernel/thread.h>
#include <pbl/logging/logging.h>

struct button_event {
  uint8_t button;
  bool pressed;
};

struct accel_sample {
  int16_t x;
  int16_t y;
  int16_t z;
};

/* One queue per source, each with its own message type and depth. */
static PBL_MSGQ_DEFINE(s_buttons, sizeof(struct button_event), 4);
static PBL_MSGQ_DEFINE(s_accel, sizeof(struct accel_sample), 8);
static PBL_POLL_GROUP_DEFINE(s_events);

PBL_THREAD_STACK_DEFINE(s_source_stack, 1024);
static struct pbl_thread s_source;

static void prv_source(void *arg) {
  const struct button_event press = {.button = 1, .pressed = true};
  const struct button_event release = {.button = 1, .pressed = false};
  const struct accel_sample sample = {.x = 10, .y = -20, .z = 1000};

  pbl_msgq_put(&s_buttons, &press, PBL_FOREVER);
  pbl_sleep_ms(10);
  pbl_msgq_put(&s_accel, &sample, PBL_FOREVER);
  pbl_sleep_ms(10);
  pbl_msgq_put(&s_buttons, &release, PBL_FOREVER);
}

static void prv_handle(struct pbl_msgq *q) {
  /* The wait leaves the message in its queue. */
  if (q == &s_buttons) {
    struct button_event event;

    if (pbl_msgq_get(q, &event, PBL_NO_WAIT) == 0) {
      PBL_LOG_INFO("loop: button %u %s", event.button, event.pressed ? "pressed" : "released");
    }
  } else if (q == &s_accel) {
    struct accel_sample sample;

    if (pbl_msgq_get(q, &sample, PBL_NO_WAIT) == 0) {
      PBL_LOG_INFO("loop: accel %d %d %d", sample.x, sample.y, sample.z);
    }
  }
}

int main(void) {
  const struct pbl_thread_attr attr = {
    .name = "source",
    .entry = prv_source,
    .prio = CONFIG_KERNEL_MAIN_THREAD_PRIO - 1,
    .privileged = true,
    .stack = s_source_stack,
    .stack_size = sizeof(s_source_stack),
  };

  /* Queues join before the first wait, and for good. */
  pbl_poll_group_add(&s_events, &s_buttons);
  pbl_poll_group_add(&s_events, &s_accel);

  pbl_thread_create(&s_source, &attr);

  for (;;) {
    struct pbl_msgq *q = pbl_poll_group_wait(&s_events, PBL_MSEC(100));
    if (q == NULL) {
      PBL_LOG_INFO("loop: nothing for 100 ms");
      break;
    }

    prv_handle(q);
  }

  PBL_LOG_INFO("Sample done");

  return 0;
}
