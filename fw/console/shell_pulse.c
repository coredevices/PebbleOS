/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/rtc.h>
#include <pbl/kernel/compiler.h>
#include <pbl/logging/logging.h>
#include <pbl/shell/backend.h>
#include <pbl/shell/shell.h>

#include <console/pulse_internal.h>
#include <console/pulse_protocol_impl.h>

#define PROMPT_RESP_ACK     (100)
#define PROMPT_RESP_DONE    (101)
#define PROMPT_RESP_MESSAGE (102)

#define LINE_BUFF_SIZE 128

typedef struct PBL_PACKED PromptResponseContents {
  uint8_t message_type;
  uint64_t time_ms;
  char message[];
} PromptResponseContents;

static char s_line[LINE_BUFF_SIZE];
static size_t s_line_len;

static void prv_send(int message_type, const char *msg, size_t len) {
#ifdef CONFIG_PULSE_EVERYWHERE
  PromptResponseContents *contents = pulse_reliable_send_begin(PULSE2_RELIABLE_PROMPT_PROTOCOL);
  if (contents == NULL) {
    return;
  }
#else
  PromptResponseContents *contents = pulse_best_effort_send_begin(PULSE_PROTOCOL_PROMPT);
#endif
  time_t time_s;
  uint16_t time_ms;

  rtc_get_time_ms(&time_s, &time_ms);
  contents->message_type = message_type;
  contents->time_ms = (uint64_t)time_s * 1000 + time_ms;
  if (len > 0) {
    memcpy(contents->message, msg, len);
  }

#ifdef CONFIG_PULSE_EVERYWHERE
  pulse_reliable_send(contents, sizeof(*contents) + len);
#else
  pulse_best_effort_send(contents, sizeof(*contents) + len);
#endif
}

static void prv_flush(void) {
  prv_send(PROMPT_RESP_MESSAGE, s_line, s_line_len);
  s_line_len = 0;
}

static void prv_write(const struct pbl_shell *sh, const char *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (data[i] == '\r') {
      continue;
    }
    if (data[i] == '\n') {
      prv_flush();
      continue;
    }
    if (s_line_len == sizeof(s_line)) {
      prv_flush();
    }
    s_line[s_line_len++] = data[i];
  }
}

static void prv_done(const struct pbl_shell *sh, int ret) {
  if (s_line_len > 0) {
    prv_flush();
  }
  prv_send(PROMPT_RESP_DONE, NULL, 0);
}

static const struct pbl_shell_backend_api s_api = {
  .write = prv_write,
  .done = prv_done,
};

PBL_SHELL_DEFINE(shell_pulse, NULL, &s_api, NULL);

#ifdef CONFIG_PULSE_EVERYWHERE

void pulse2_prompt_packet_handler(void *packet, size_t length) {
  int ret = pbl_shell_execute_line(&shell_pulse, packet, length);
  if (ret < 0) {
    PBL_LOG_WRN("Dropping shell command (%d)", ret);
  }
}

#else

typedef struct PBL_PACKED PromptCommand {
  uint8_t cookie;
  char command[];
} PromptCommand;

static uint16_t s_latest_cookie = UINT16_MAX;

void pulse_prompt_handler(void *packet, size_t length) {
  PromptCommand *command = packet;

  if (s_latest_cookie == command->cookie) {
    prv_send(pbl_shell_is_busy(&shell_pulse) ? PROMPT_RESP_ACK : PROMPT_RESP_DONE, NULL, 0);
    return;
  }

  prv_send(PROMPT_RESP_ACK, NULL, 0);
  s_latest_cookie = command->cookie;

  int ret = pbl_shell_execute_line(&shell_pulse, command->command, length - sizeof(*command));
  if (ret < 0) {
    PBL_LOG_WRN("Dropping shell command (%d)", ret);
    prv_send(PROMPT_RESP_DONE, NULL, 0);
  }
}

void pulse_prompt_link_state_handler(PulseLinkState link_state) {
  if (link_state == PulseLinkState_Open) {
    s_latest_cookie = UINT16_MAX;
  }
}

#endif
