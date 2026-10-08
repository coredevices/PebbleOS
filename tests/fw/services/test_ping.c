/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/ui/dialogs/simple_dialog.h"
#include "kernel/event_loop.h"
#include "kernel/ui/modals/modal_manager.h"
#include "pbl/services/comm_session/session.h"
#include "pbl/services/system_task.h"

#include "stubs_battery.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_rtc.h"

extern void ping_protocol_msg_callback(CommSession *session, const uint8_t *data, size_t length);

static SimpleDialog s_dialog;
static DialogCallbacks s_dialog_callbacks;
static void *s_dialog_callback_context;
static int s_num_dialogs_pushed;
static int s_num_pongs_sent;

bool accel_is_idle(void) {
  return false;
}

CommSession *comm_session_get_system_session(void) {
  return NULL;
}

bool comm_session_send_data(CommSession *session, uint16_t endpoint_id, const uint8_t *data,
                            size_t length, uint32_t timeout_ms) {
  s_num_pongs_sent++;
  return true;
}

bool system_task_add_callback(SystemTaskEventCallback cb, void *data) {
  return true;
}

void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  callback(data);
}

WindowStack *modal_manager_get_window_stack(ModalPriority priority) {
  return NULL;
}

SimpleDialog *simple_dialog_create(const char *dialog_name) {
  s_dialog = (SimpleDialog){};
  return &s_dialog;
}

Dialog *simple_dialog_get_dialog(SimpleDialog *simple_dialog) {
  return &simple_dialog->dialog;
}

void simple_dialog_push(SimpleDialog *simple_dialog, WindowStack *window_stack) {
  s_num_dialogs_pushed++;
}

void dialog_set_callbacks(Dialog *dialog, const DialogCallbacks *callbacks,
                          void *callback_context) {
  s_dialog_callbacks = *callbacks;
  s_dialog_callback_context = callback_context;
}

void dialog_set_background_color(Dialog *dialog, GColor background_color) {
}

void dialog_set_text_color(Dialog *dialog, GColor text_color) {
}

void dialog_set_text(Dialog *dialog, const char *text) {
}

static void prv_receive_ping(uint32_t cookie) {
  const uint8_t ping[] = {0, cookie >> 24, cookie >> 16, cookie >> 8, cookie, 0};
  ping_protocol_msg_callback(NULL, ping, sizeof(ping));
}

static void prv_dismiss_dialog(void) {
  cl_assert(s_dialog_callbacks.unload);
  s_dialog_callbacks.unload(s_dialog_callback_context);
}

void test_ping__initialize(void) {
  s_dialog_callbacks = (DialogCallbacks){};
  s_dialog_callback_context = NULL;
  s_num_dialogs_pushed = 0;
  s_num_pongs_sent = 0;
}

void test_ping__cleanup(void) {
  if (s_dialog_callbacks.unload) {
    prv_dismiss_dialog();
  }
}

void test_ping__pings_show_one_dialog(void) {
  for (uint32_t cookie = 0; cookie < 100; cookie++) {
    prv_receive_ping(cookie);
  }
  cl_assert_equal_i(s_num_pongs_sent, 100);
  cl_assert_equal_i(s_num_dialogs_pushed, 1);
}

void test_ping__ping_after_dismissal_shows_dialog(void) {
  prv_receive_ping(1);
  prv_dismiss_dialog();
  prv_receive_ping(2);
  cl_assert_equal_i(s_num_pongs_sent, 2);
  cl_assert_equal_i(s_num_dialogs_pushed, 2);
}
