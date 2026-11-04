/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "console_frame.h"

#include <stdint.h>
#include <string.h>

#include <pbl/crc/crc.h>
#include <pbl/services/system_task.h>
#include <pbl/util/base64.h>

#include <console/dbgserial.h>
#include <console/serial_console.h>

#define ESC 0x1b

#define SEQ_LEN 1U
#define CRC_LEN sizeof(uint32_t)
#define RAW_MAX (SEQ_LEN + CONSOLE_FRAME_MTU + CRC_LEN)
#define B64_MAX ((RAW_MAX + 2U) / 3U * 4U)

enum rx_state {
  RX_TEXT,
  RX_ESC,
  RX_FRAME,
  RX_FRAME_END,
};

// One frame at a time: the host waits for the acknowledgment before sending the next.
static char s_rx_buf[1U + B64_MAX];
static size_t s_rx_len;
static enum rx_state s_rx_state;
static bool s_rx_dropping;
static volatile bool s_rx_pending;
static int s_last_seq = -1;

// Under the console lock.
static uint8_t s_tx_raw[RAW_MAX];
static char s_tx_b64[B64_MAX + 1U];

static uint32_t prv_crc(char channel, const void *data, size_t len) {
  return pbl_crc32(pbl_crc32(0, &channel, 1), data, len);
}

static void prv_put(char c) {
  dbgserial_putchar_lazy(c);
}

void console_frame_send(char channel, const void *data, size_t len) {
  serial_console_lock();

  s_tx_raw[0] = 0U;
  memcpy(&s_tx_raw[SEQ_LEN], data, len);
  const size_t body_len = SEQ_LEN + len;
  const uint32_t crc = prv_crc(channel, s_tx_raw, body_len);
  memcpy(&s_tx_raw[body_len], &crc, CRC_LEN);
  const size_t b64_len =
      pbl_base64_encode(s_tx_b64, sizeof(s_tx_b64), s_tx_raw, body_len + CRC_LEN);

  prv_put(ESC);
  prv_put('_');
  prv_put(channel);
  for (size_t i = 0; i < b64_len; i++) {
    prv_put(s_tx_b64[i]);
  }
  prv_put(ESC);
  prv_put('\\');
  dbgserial_flush();

  serial_console_unlock();
}

static void prv_process_frame(void *unused) {
  const char channel = s_rx_buf[0];
  const int raw_len =
      pbl_base64_decode((uint8_t *)&s_rx_buf[1], B64_MAX, &s_rx_buf[1], s_rx_len - 1U);

  if (raw_len < (int)(SEQ_LEN + CRC_LEN) ||
      prv_crc(channel, &s_rx_buf[1], (size_t)raw_len) != PBL_CRC32_RESIDUE) {
    s_rx_pending = false;
    return;
  }

  const uint8_t seq = (uint8_t)s_rx_buf[1];
  if (seq == 0U || seq != s_last_seq) {
    const void *payload = &s_rx_buf[1 + SEQ_LEN];
    const size_t payload_len = (size_t)raw_len - SEQ_LEN - CRC_LEN;

    if (channel == CONSOLE_FRAME_CHANNEL_PP) {
      console_pp_handle_frame(payload, payload_len);
    } else if (channel == CONSOLE_FRAME_CHANNEL_IMAGING) {
      console_imaging_handle_frame(payload, payload_len);
    }
    s_last_seq = (seq == 0U) ? -1 : seq;
  }

  // Before acknowledging, as the host's next frame follows right away.
  s_rx_pending = false;
  console_frame_send(CONSOLE_FRAME_CHANNEL_ACK, &seq, sizeof(seq));
}

static bool prv_is_frame_char(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' ||
         c == '/' || c == '=';
}

void console_frame_input_from_isr(char c, void (*text_input)(char c)) {
  switch (s_rx_state) {
    case RX_TEXT:
      if (c == ESC) {
        s_rx_state = RX_ESC;
      } else {
        text_input(c);
      }
      break;
    case RX_ESC:
      if (c == '_') {
        s_rx_state = RX_FRAME;
        // The buffer still holds the frame waiting to be handled.
        s_rx_dropping = s_rx_pending;
        if (!s_rx_dropping) {
          s_rx_len = 0U;
        }
      } else {
        text_input(ESC);
        if (c != ESC) {
          text_input(c);
          s_rx_state = RX_TEXT;
        }
      }
      break;
    case RX_FRAME:
      if (c == ESC) {
        s_rx_state = RX_FRAME_END;
      } else if (!prv_is_frame_char(c) || (!s_rx_dropping && s_rx_len >= sizeof(s_rx_buf))) {
        // Not a frame after all, or a damaged one: back to text.
        s_rx_state = RX_TEXT;
      } else if (!s_rx_dropping) {
        s_rx_buf[s_rx_len++] = c;
      }
      break;
    case RX_FRAME_END:
      s_rx_state = RX_TEXT;
      if (c == '\\' && !s_rx_dropping && s_rx_len > 1U) {
        s_rx_pending = true;
        if (!system_task_add_callback_from_isr(prv_process_frame, nullptr)) {
          s_rx_pending = false;
        }
      }
      break;
  }
}
