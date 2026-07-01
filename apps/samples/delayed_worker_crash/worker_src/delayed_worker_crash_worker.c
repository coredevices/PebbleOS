/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble_worker.h>

#define WORKER_CRASH_DELAY_MS 5000

static void worker_timer_callback(void *data) {
  // Free -1 to crash the worker
  free((void *) -1);
}

static void worker_init(void) {
  app_timer_register(WORKER_CRASH_DELAY_MS, worker_timer_callback, NULL);
}

int main(void) {
  worker_init();
  worker_event_loop();
}
