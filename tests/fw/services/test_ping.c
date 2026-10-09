/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/comm_session/session.h>

#include <applib/ui/dialogs/simple_dialog.h>
#include <clar.h>
#include <kernel/event_loop.h>
#include <kernel/ui/modals/modal_manager.h>
#include <stubs_logging.h>
#include <stubs_passert.h>

extern void ping_protocol_msg_callback(CommSession *session, const uint8_t *data, size_t length);

#define MAX_CALLBACKS 8

static CallbackEventCallback s_callbacks[MAX_CALLBACKS];
static int s_num_callbacks;
static int s_num_pongs;
static int s_num_dialogs;
static SimpleDialog s_dialog;

void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  cl_assert(s_num_callbacks < MAX_CALLBACKS);
  s_callbacks[s_num_callbacks++] = callback;
}

void system_task_add_callback(CallbackEventCallback callback, void *data) {
}

bool comm_session_send_data(CommSession *session, uint16_t endpoint_id, const uint8_t *data,
                            size_t length, uint32_t timeout_ms) {
  s_num_pongs++;
  return true;
}

CommSession *comm_session_get_system_session(void) {
  return nullptr;
}

bool battery_is_usb_connected(void) {
  return false;
}

bool accel_is_idle(void) {
  return false;
}

time_t rtc_get_time(void) {
  return 0;
}

SimpleDialog *simple_dialog_create(const char *dialog_name) {
  s_num_dialogs++;
  s_dialog = (SimpleDialog){};
  return &s_dialog;
}

Dialog *simple_dialog_get_dialog(SimpleDialog *simple_dialog) {
  return &simple_dialog->dialog;
}

void simple_dialog_push(SimpleDialog *simple_dialog, WindowStack *window_stack) {
}

void dialog_set_callbacks(Dialog *dialog, const DialogCallbacks *callbacks,
                          void *callback_context) {
  dialog->callbacks = *callbacks;
  dialog->callback_context = callback_context;
}

void dialog_set_background_color(Dialog *dialog, GColor background_color) {
}

void dialog_set_text_color(Dialog *dialog, GColor text_color) {
}

void dialog_set_text(Dialog *dialog, const char *text) {
}

WindowStack *modal_manager_get_window_stack(ModalPriority priority) {
  return nullptr;
}

static void prv_run_callbacks(void) {
  for (int i = 0; i < s_num_callbacks; i++) {
    s_callbacks[i](nullptr);
  }
  s_num_callbacks = 0;
}

static void prv_dismiss_dialog(void) {
  s_dialog.dialog.callbacks.unload(s_dialog.dialog.callback_context);
}

static void prv_receive_ping(uint8_t cookie) {
  const uint8_t ping[] = {0x00, 0x00, 0x00, 0x00, cookie, 0x00};
  ping_protocol_msg_callback(nullptr, ping, sizeof(ping));
}

void test_ping__initialize(void) {
  s_num_callbacks = 0;
  s_num_pongs = 0;
  s_num_dialogs = 0;
}

void test_ping__cleanup(void) {
  prv_run_callbacks();
  if (s_num_dialogs) {
    prv_dismiss_dialog();
  }
}

void test_ping__burst_shows_one_dialog(void) {
  for (int i = 0; i < 100; i++) {
    prv_receive_ping(i);
  }
  prv_run_callbacks();

  cl_assert_equal_i(s_num_pongs, 100);
  cl_assert_equal_i(s_num_dialogs, 1);
}

void test_ping__dialog_shown_again_once_dismissed(void) {
  prv_receive_ping(0);
  prv_run_callbacks();
  prv_dismiss_dialog();

  prv_receive_ping(1);
  prv_run_callbacks();

  cl_assert_equal_i(s_num_pongs, 2);
  cl_assert_equal_i(s_num_dialogs, 2);
}
