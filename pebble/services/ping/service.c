/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/drivers/battery.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/endpoint.h>
#include <pbl/pp/send.h>
#include <pbl/pp/session.h>
#include <pbl/services/accel_manager.h>
#include <pbl/util/byteorder.h>

#include <applib/ui/dialogs/dialog.h>
#include <applib/ui/dialogs/simple_dialog.h>
#include <kernel/event_loop.h>
#include <kernel/ui/modals/modal_manager.h>

PBL_LOG_MODULE_DEFINE(service_ping, CONFIG_SERVICE_PING_LOG_LEVEL);

#define PING_MIN_PERIOD_SECS (60 * 60) // 1 hour

static time_t s_last_send_time;
static bool s_is_ping_kernel_bg_callback_scheduled;
static bool s_is_dialog_pushed;

// ---------------------------------------------------------------------------------------------------------
// Ping Pong structures
typedef struct PBL_PACKED {
  uint8_t cmd;
  uint32_t cookie;
} PingMsgHeader;

typedef struct PBL_PACKED {
  PingMsgHeader hdr;
} PingMsgV1;

typedef struct PBL_PACKED {
  PingMsgHeader hdr;
  uint8_t idle; // Optional
} PingMsgV2;

typedef struct PBL_PACKED {
  PingMsgHeader hdr;
} PongMsg;

static void prv_send_ping_kernel_bg_cb(void *unused) {
  struct pbl_pp_session *system_session = pbl_pp_session_get_system();
  if (system_session) {
    // Are we idle?
    bool idle = (battery_is_usb_connected() || accel_is_idle());

    PingMsgV2 ping_msg =
        (PingMsgV2){.hdr = {.cmd = 0, .cookie = pbl_cpu_to_be32(42)}, .idle = idle};
    bool success =
        0 == pbl_pp_send(system_session, PBL_PP_ENDPOINT_PING, (const uint8_t *)&ping_msg,
                         sizeof(ping_msg), PBL_PP_DEFAULT_TIMEOUT_MS);
    if (success) {
      s_last_send_time = pbl_realtime();
    }
    PBL_LOG_DBG("Sent ping idle=%d, success=%d", (int)idle, (int)success);
  }

  s_is_ping_kernel_bg_callback_scheduled = false;
}

// ------------------------------------------------------------------------------------------------------------
// If a ping is due to be sent, send it.
// bt_lock() is held by the caller
void ping_send_if_due(void) {
  if (s_is_ping_kernel_bg_callback_scheduled) {
    return;
  }

  // Only send if we haven't sent within the last PING_MIN_PERIOD_SECS
  time_t current_time = pbl_realtime();
  if (current_time < s_last_send_time + PING_MIN_PERIOD_SECS) {
    return;
  }

  // Offload to SysWorkq, because pbl_pp_send() may block.
  pbl_sys_work_call(prv_send_ping_kernel_bg_cb, nullptr);
  s_is_ping_kernel_bg_callback_scheduled = true;
}

static void prv_dialog_unload(void *context) {
  s_is_dialog_pushed = false;
}

static void prv_push_window(void *data) {
  SimpleDialog *s_dialog = simple_dialog_create("Ping");
  Dialog *dialog = simple_dialog_get_dialog(s_dialog);

  dialog_set_callbacks(dialog, &(DialogCallbacks){.unload = prv_dialog_unload}, nullptr);

  dialog_set_background_color(dialog, GColorCobaltBlue);
  dialog_set_text_color(dialog, GColorWhite);
  dialog_set_text(dialog, "Ping");

  WindowStack *stack = modal_manager_get_window_stack(ModalPriorityGeneric);
  simple_dialog_push(s_dialog, stack);
}

void ping_protocol_msg_callback(struct pbl_pp_session *session, const uint8_t *data,
                                size_t length) {
  PingMsgV1 *ping = (PingMsgV1 *)data;
  switch (ping->hdr.cmd) {
    case 0: {
      if (length != sizeof(PingMsgV1) &&
          length != sizeof(PingMsgV2) /* idle boolean is optional */) {
        PBL_LOG_ERR("Invalid Ping, l=%zu", length);
        return;
      }

      // Ping message
      uint32_t cookie = pbl_be32_to_cpu(ping->hdr.cookie);
      PBL_LOG_DBG("Ping c=%" PRIu32 "", cookie);
      if (!s_is_dialog_pushed) {
        s_is_dialog_pushed = true;
        launcher_task_add_callback(prv_push_window, nullptr);
      }

      // Send the pong response
      PongMsg pong = {.hdr = {.cmd = 1, .cookie = pbl_cpu_to_be32(cookie)}};
      pbl_pp_send(session, PBL_PP_ENDPOINT_PING, (uint8_t *)&pong, sizeof(pong),
                  PBL_PP_DEFAULT_TIMEOUT_MS);
      break;
    }

    case 1:
      if (length != sizeof(PongMsg)) {
        PBL_LOG_ERR("Invalid Pong, l=%zu", length);
        return;
      }

      PongMsg *pong = (PongMsg *)data;
      PBL_LOG_DBG("Pong c=%" PRIu32, pbl_be32_to_cpu(pong->hdr.cookie));
      break;

    default:
      PBL_LOG_ERR("Invalid message received. First byte is %u", ping->hdr.cmd);
      break;
  }
}

#ifndef CONFIG_RECOVERY_FW
PBL_PP_ENDPOINT_DEFINE(PBL_PP_ENDPOINT_PING, PBL_PP_ACCESS_ANY, ping_protocol_msg_callback);
#endif

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>

static int prv_cmd_ping(const struct pbl_shell *sh, size_t argc, char **argv) {
  s_last_send_time = 0;
  ping_send_if_due();
  return 0;
}

PBL_SHELL_CMD_REGISTER(ping, nullptr, "Send a ping to the phone", prv_cmd_ping);
#endif
