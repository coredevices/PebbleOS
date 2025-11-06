/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/qr_code.h"
#include "applib/ui/window.h"
#include "applib/ui/window_private.h"
#include "applib/ui/text_layer.h"
#include "kernel/pbl_malloc.h"
#include "mfg/mfg_serials.h"
#include "process_state/app_state/app_state.h"
#include "process_management/pebble_process_md.h"
#include "util/bitset.h"
#include "util/size.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
  Window window;

  QRCode qr_code;
  TextLayer serial;

  char serial_buffer[MFG_SERIAL_NUMBER_SIZE + 1];
} AppData;

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);

  // Get the serial number
  const char *serial_number = mfg_get_serial_number();
  strncpy(data->serial_buffer, serial_number, MFG_SERIAL_NUMBER_SIZE);
  data->serial_buffer[MFG_SERIAL_NUMBER_SIZE] = '\0';

  QRCode* qr_code = &data->qr_code;
  qr_code_init_with_parameters(qr_code,
                               &GRect(10, 10, window->layer.bounds.size.w - 20,
                                      window->layer.bounds.size.h - 30),
                               data->serial_buffer, strlen(data->serial_buffer), QRCodeECCMedium,
                               GColorBlack, GColorWhite);
  layer_add_child(&window->layer, &qr_code->layer);

  TextLayer* serial = &data->serial;
  text_layer_init_with_parameters(serial,
                                  &GRect(0, window->layer.bounds.size.h - 20,
                                         window->layer.bounds.size.w, 20),
                                  data->serial_buffer, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                                  GColorBlack, GColorWhite, GTextAlignmentCenter,
                                  GTextOverflowModeTrailingEllipsis);
  layer_add_child(&window->layer, &serial->layer);

  app_window_stack_push(window, true /* Animated */);
}

static void s_main(void) {
  prv_handle_init();

  app_event_loop();
}

const PebbleProcessMd* mfg_serial_qr_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: 4f8a2d3e-1c5b-4a9f-8e7d-6c3b2a1f0e9d
    .common.uuid = { 0x4f, 0x8a, 0x2d, 0x3e, 0x1c, 0x5b, 0x4a, 0x9f,
                     0x8e, 0x7d, 0x6c, 0x3b, 0x2a, 0x1f, 0x0e, 0x9d },
    .name = "MfgSerialQR",
  };
  return (const PebbleProcessMd*) &s_app_info;
}
