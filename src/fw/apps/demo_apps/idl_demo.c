/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "idl_demo.h"

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"
#include "system/hexdump.h"

#include "pb_decode.h"
#include "pb_encode.h"
#include "nanopb/simple.pb.h"
#include "nanopb/measurements.pb.h"

static void prv_init(void) {
  SimpleMessage msg = {
    .lucky_number = 42,
  };
  uint8_t *buffer = app_malloc(30);
  pb_ostream_t s = pb_ostream_from_buffer(buffer, 30);
  pb_encode(&s, SimpleMessage_fields, &msg);
  PBL_LOG(LOG_LEVEL_DEBUG, "Encoded message, size: %u bytes", s.bytes_written);
  PBL_HEXDUMP(LOG_LEVEL_DEBUG, buffer, s.bytes_written);
  app_state_set_user_data(buffer);
}

static void prv_deinit(void) {
  SimpleMessage msg = {0};
  uint8_t *buffer = app_state_get_user_data();
  pb_istream_t s = pb_istream_from_buffer(buffer, 30);
  pb_decode(&s, SimpleMessage_fields, &msg);
  PBL_LOG(LOG_LEVEL_DEBUG, "The lucky number is %"PRId32, msg.lucky_number);
}

static void prv_app_main(void) {
  prv_init();
  Window *window = window_create();
  app_window_stack_push(window, false /*animated*/);
  app_event_loop();
  prv_deinit();
}

const PebbleProcessMd *idl_demo_get_app_info() {
  static const PebbleProcessMdSystem s_app_data = {
    .common = {
      .main_func = prv_app_main,
      // UUID: 101a32d95-1234-46d4-1234-854cc62f97f9
      .uuid = {0x99, 0xa3, 0x2d, 0x95, 0x12, 0x34, 0x46, 0xd4,
               0x12, 0x34, 0x85, 0x4c, 0xc6, 0x2f, 0x97, 0xf9},
    },
    .name = "IDL Demo",
  };
  return (const PebbleProcessMd *)&s_app_data;
}
