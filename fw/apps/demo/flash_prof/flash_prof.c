/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board/partitions.h>
#include <pbl/drivers/flash.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>

#include <applib/app.h>
#include <applib/ui/app_window_stack.h>
#include <applib/ui/number_window.h>
#include <applib/ui/window_stack.h>
#include <process_management/pebble_process_md.h>

static NumberWindow number_window;

static uint32_t timed_read_bytes(uint32_t num_bytes) {
  uint8_t *buffer = pbl_malloc_check(num_bytes);
  uint64_t start_ms = pbl_realtime_ms();
  pbl_flash_partition_read(PART_FILESYSTEM, 0U, buffer, num_bytes);
  uint64_t stop_ms = pbl_realtime_ms();
  pbl_free(buffer);
  return (uint32_t)(stop_ms - start_ms);
}

static void do_timed_read(NumberWindow *nw, void *data) {
  uint32_t num_bytes = nw->value;
  uint32_t predicted_time = num_bytes * 8 / 16000;
  uint32_t time = timed_read_bytes(num_bytes);
  PBL_LOG_DBG("time to read %lu bytes: predicted %lu, actual %lu", num_bytes, predicted_time, time);
  window_stack_remove(&number_window.window, false);
  app_window_stack_push(&number_window.window, true);
}

#define NUM_BYTES 1000

static void handle_init(void) {
  number_window_init(&number_window, "Num Writes",
                     (NumberWindowCallbacks){
                       .selected = (NumberWindowCallback)do_timed_read,
                     },
                     nullptr);
  number_window_set_min(&number_window, 1000);
  number_window_set_max(&number_window, 1000000);
  number_window_set_step_size(&number_window, 1000);
  app_window_stack_push((Window *)&number_window, true);
}

static void handle_deinit(void) {
}

static void s_main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}

const PebbleProcessMd *flash_prof_get_app_info() {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    .name = "Flash Prof"
  };

  return (const PebbleProcessMd *)&s_app_info;
}
