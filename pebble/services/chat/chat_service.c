/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/malloc.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/endpoint.h>
#include <pbl/pp/protocol.h>
#include <pbl/pp/send.h>
#include <pbl/pp/session.h>
#include <pbl/services/chat/chat_service.h>

#include <kernel/pebble_tasks.h>
#include <process_management/process_manager.h>

#define LAUNCH_HINT_SIZE 64

static ChatServiceHandler s_handler;
static char s_launch_hint[LAUNCH_HINT_SIZE];
static ChatServiceHintHandler s_hint_handler;

typedef struct {
  size_t len;
  uint8_t dict[];
} Packet;

static void prv_deliver(void *data) {
  Packet *packet = data;
  // Read again on the app task: the app may have exited since the packet was queued
  if (s_handler) {
    s_handler(packet->dict, packet->len);
  }
  pbl_free(packet);
}

static void prv_handle_packet(struct pbl_pp_session *session, const uint8_t *data, size_t length) {
  if (length < 1 || data[0] != CHAT_SERVICE_VERSION) {
    PBL_LOG_WRN("Chat: dropping version %u", length ? data[0] : 0);
    return;
  }
  if (!s_handler) {
    return; // Chat app not running; it reads what it needs from the chat db when it starts
  }
  Packet *packet = pbl_malloc(sizeof(Packet) + length - 1);
  if (!packet) {
    return;
  }
  packet->len = length - 1;
  memcpy(packet->dict, data + 1, length - 1);
  // ponytail: a packet queued just as the app exits is never freed; a per-app queue drained on
  // exit if it shows up in heap stats.
  process_manager_send_callback_event_to_process(PebbleTask_App, prv_deliver, packet);
}

#ifndef CONFIG_RECOVERY_FW
PBL_PP_ENDPOINT_DEFINE(PBL_PP_ENDPOINT_CHAT, PBL_PP_ACCESS_PRIVATE, prv_handle_packet);
#endif

void chat_service_set_handler(ChatServiceHandler handler) {
  s_handler = handler;
}

bool chat_service_send(const uint8_t *dict, size_t len) {
  struct pbl_pp_session *session = pbl_pp_session_get_system();
  if (!session) {
    return false;
  }
  struct pbl_pp_send_buffer *sb = pbl_pp_send_buffer_begin(session, PBL_PP_ENDPOINT_CHAT, len + 1,
                                                           PBL_PP_DEFAULT_TIMEOUT_MS);
  if (!sb) {
    return false;
  }
  const uint8_t version = CHAT_SERVICE_VERSION;
  pbl_pp_send_buffer_write(sb, &version, 1);
  pbl_pp_send_buffer_write(sb, dict, len);
  pbl_pp_send_buffer_end(sb);
  return true;
}

static void prv_deliver_hint(void *data) {
  if (s_hint_handler) {
    s_hint_handler();
  }
}

void chat_service_set_launch_hint(const char *title) {
  strncpy(s_launch_hint, title ? title : "", sizeof(s_launch_hint) - 1);
  if (s_hint_handler) {  // the app is running: it reads the hint now, not at its next launch
    process_manager_send_callback_event_to_process(PebbleTask_App, prv_deliver_hint, nullptr);
  }
}

void chat_service_set_hint_handler(ChatServiceHintHandler handler) {
  s_hint_handler = handler;
}

bool chat_service_take_launch_hint(char *out, size_t out_size) {
  const bool has = s_launch_hint[0] != '\0';
  strncpy(out, s_launch_hint, out_size - 1);
  out[out_size - 1] = '\0';
  s_launch_hint[0] = '\0';
  return has;
}
