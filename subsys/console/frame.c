/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "console_private.h"

#include <stdint.h>
#include <string.h>

#include <pbl/console/console.h>
#include <pbl/crc/crc.h>
#include <pbl/kernel/work.h>
#include <pbl/pp/transports/console.h>
#include <pbl/util/base64.h>

#define ESC 0x1b

#define SEQ_LEN     1U
#define CRC_LEN     sizeof(uint32_t)
#define RAW_MAX     (SEQ_LEN + PBL_CONSOLE_FRAME_MTU + CRC_LEN)
#define B64_MAX     ((RAW_MAX + 2U) / 3U * 4U)
#define ACK_RAW_LEN (SEQ_LEN + 1U + CRC_LEN)
#define ACK_B64_LEN 8U

enum rx_state {
  RX_TEXT,
  RX_ESC,
  RX_CHANNEL,
  RX_FRAME,
  RX_FRAME_END,
  RX_ACK,
  RX_ACK_END,
};

// One frame at a time: the host waits for the acknowledgment before sending the next.
static char s_rx_buf[1U + B64_MAX];
static size_t s_rx_len;
static enum rx_state s_rx_state;
static bool s_rx_dropping;
static volatile bool s_rx_pending;
static int s_last_seq = -1;

// Acknowledgments arrive at any time, also while s_rx_buf is waiting to be handled. One arriving
// while another is handled fails its CRC, and the host answers the repeated frame.
static char s_ack_buf[ACK_B64_LEN];
static size_t s_ack_len;

// Under the console lock.
static uint8_t s_tx_raw[RAW_MAX];
static char s_tx_b64[B64_MAX + 1U];

static uint32_t prv_crc(char channel, const void *data, size_t len) {
  return pbl_crc32(pbl_crc32(0, &channel, 1), data, len);
}

void pbl_console_frame_send(char channel, uint8_t seq, const void *data, size_t len) {
  pbl_console_lock();

  s_tx_raw[0] = seq;
  memcpy(&s_tx_raw[SEQ_LEN], data, len);
  const size_t body_len = SEQ_LEN + len;
  const uint32_t crc = prv_crc(channel, s_tx_raw, body_len);
  memcpy(&s_tx_raw[body_len], &crc, CRC_LEN);
  const size_t b64_len =
      pbl_base64_encode(s_tx_b64, sizeof(s_tx_b64), s_tx_raw, body_len + CRC_LEN);

  pbl_console_putchar(ESC);
  pbl_console_putchar('_');
  pbl_console_putchar(channel);
  for (size_t i = 0; i < b64_len; i++) {
    pbl_console_putchar(s_tx_b64[i]);
  }
  pbl_console_putchar(ESC);
  pbl_console_putchar('\\');
  pbl_console_flush();

  pbl_console_unlock();
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
    [[maybe_unused]] const void *payload = &s_rx_buf[1 + SEQ_LEN];
    [[maybe_unused]] const size_t payload_len = (size_t)raw_len - SEQ_LEN - CRC_LEN;

#ifdef CONFIG_CONSOLE_IMAGING
    if (channel == CONSOLE_FRAME_CHANNEL_IMAGING) {
      pbl_console__imaging_handle_frame(payload, payload_len);
    }
#endif
#ifdef CONFIG_PP_TRANSPORT_CONSOLE
    if (channel == PBL_CONSOLE_CHANNEL_PP) {
      pbl_pp_console_receive(payload, payload_len);
    }
#endif
    s_last_seq = (seq == 0U) ? -1 : seq;
  }

  // Before acknowledging, as the host's next frame follows right away.
  s_rx_pending = false;
  pbl_console_frame_send(CONSOLE_FRAME_CHANNEL_ACK, 0U, &seq, sizeof(seq));
}

static void prv_process_ack(void *unused) {
  uint8_t raw[ACK_RAW_LEN];
  [[maybe_unused]] const int raw_len = pbl_base64_decode(raw, sizeof(raw), s_ack_buf, ACK_B64_LEN);

#ifdef CONFIG_PP_TRANSPORT_CONSOLE
  if (raw_len == (int)ACK_RAW_LEN &&
      prv_crc(CONSOLE_FRAME_CHANNEL_ACK, raw, ACK_RAW_LEN) == PBL_CRC32_RESIDUE) {
    pbl_pp_console_ack(raw[SEQ_LEN]);
  }
#endif
}

static bool prv_is_frame_char(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' ||
         c == '/' || c == '=';
}

void pbl_console__frame_input_from_isr(char c, void (*text_input)(char c)) {
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
        s_rx_state = RX_CHANNEL;
      } else {
        text_input(ESC);
        if (c != ESC) {
          text_input(c);
          s_rx_state = RX_TEXT;
        }
      }
      break;
    case RX_CHANNEL:
      if (c == CONSOLE_FRAME_CHANNEL_ACK) {
        s_rx_state = RX_ACK;
        s_ack_len = 0U;
        break;
      }
      s_rx_state = RX_FRAME;
      // The buffer still holds the frame waiting to be handled.
      s_rx_dropping = s_rx_pending;
      if (!s_rx_dropping) {
        s_rx_len = 0U;
      }
      [[fallthrough]];
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
        if (!pbl_sys_work_call_from_isr(prv_process_frame, nullptr)) {
          s_rx_pending = false;
        }
      }
      break;
    case RX_ACK:
      if (c == ESC) {
        s_rx_state = RX_ACK_END;
      } else if (!prv_is_frame_char(c) || s_ack_len >= sizeof(s_ack_buf)) {
        s_rx_state = RX_TEXT;
      } else {
        s_ack_buf[s_ack_len++] = c;
      }
      break;
    case RX_ACK_END:
      s_rx_state = RX_TEXT;
      if (c == '\\' && s_ack_len == ACK_B64_LEN) {
        pbl_sys_work_call_high_from_isr(prv_process_ack, nullptr);
      }
      break;
  }
}
