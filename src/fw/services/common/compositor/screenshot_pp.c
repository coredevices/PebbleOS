/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "screenshot_pp.h"

#include "compositor.h"

#include "applib/graphics/framebuffer.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "services/common/comm_session/session_send_buffer.h"
#include "services/common/system_task.h"
#include "system/logging.h"
#include "util/attributes.h"
#include "util/net.h"

static const uint16_t SCREENSHOT_ENDPOINT_ID = 8000;
static bool s_screenshot_in_progress = false;

typedef enum {
  SCREENSHOT_OK = 0,
  SCREENSHOT_MALFORMED_COMMAND = 1,
  SCREENSHOT_OOM_ERROR = 2,
  SCREENSHOT_ALREADY_IN_PROGRESS = 3,
} ScreenshotResponse;

typedef struct FrameBufferState {
  FrameBuffer *fb;
  uint32_t row;
  uint32_t col;
  uint32_t width;
  uint32_t height;
} FrameBufferState;

typedef struct ScreenshotState {
  CommSession *session;
  FrameBufferState framebuffer;
  bool sent_header;
} ScreenshotState;
static ScreenshotState s_screenshot_state;

typedef struct PACKED {
  uint8_t  response_code;
  uint32_t version;
  uint32_t width;
  uint32_t height;
} ScreenshotHeader;

typedef struct ScreenshotErrorResponseData {
  CommSession *session;
  ScreenshotHeader header;
} ScreenshotErrorResponseData;

static void prv_send_error_response(CommSession *session, uint8_t response) {
  ScreenshotErrorResponseData error_response = (ScreenshotErrorResponseData) {
    .session = session,
    .header = {
      .response_code = response,
      .version       = htonl(1),
      .width         = htonl(0),
      .height        = htonl(0),
    },
  };

  comm_session_send_data(
      error_response.session, SCREENSHOT_ENDPOINT_ID,
      (const uint8_t *) &error_response.header, sizeof(error_response.header),
      COMM_SESSION_DEFAULT_TIMEOUT);
}

static void prv_finish(ScreenshotState *state) {
  comm_session_set_responsiveness(state->session, BtConsumerPpScreenshot, ResponseTimeMax, 0);
  compositor_unfreeze();
  s_screenshot_in_progress = false;
}

static void prv_request_fast_connection(CommSession *session) {
  comm_session_set_responsiveness(session, BtConsumerPpScreenshot, ResponseTimeMin,
                                  MIN_LATENCY_MODE_TIMEOUT_SCREENSHOT_SECS);
}

static uint32_t prv_framebuffer_next_chunk(FrameBufferState *restrict state,
                                           uint32_t max_chunk_bytes, uint8_t *output_buffer) {
  const uint32_t bytes_per_row = SCREEN_COLOR_DEPTH_BITS * DISP_COLS / 8;
  const uint32_t cols_per_byte = DISP_COLS / bytes_per_row;

  uint32_t remaining_chunk_bytes = max_chunk_bytes;
  uint8_t *output_buffer_with_offset = output_buffer;

  while (remaining_chunk_bytes > 0 && state->row < state->height) {
    const uint8_t *restrict framebuffer_row_data = (uint8_t *)framebuffer_get_line(state->fb,
                                                                                   state->row);
    const uint16_t framebuffer_current_column = state->col / cols_per_byte;
    uint16_t remaining_framebuffer_row_bytes = bytes_per_row - framebuffer_current_column;
    const bool framebuffer_row_is_larger_than_chunk =
      (remaining_framebuffer_row_bytes > remaining_chunk_bytes);
    if (framebuffer_row_is_larger_than_chunk) {
      remaining_framebuffer_row_bytes = remaining_chunk_bytes;
    }

#ifdef PLATFORM_SPALDING
    const GBitmapDataRowInfoInternal *row_infos = g_gbitmap_spalding_data_row_infos;
    const size_t framebuffer_row_min_pixel = row_infos[state->row].min_x;
    const size_t framebuffer_row_max_pixel = row_infos[state->row].max_x;
    for (uint32_t i = 0; i < remaining_framebuffer_row_bytes; i++) {
      const uint32_t i_with_offset = i + framebuffer_current_column;
      if (WITHIN(i_with_offset, framebuffer_row_min_pixel, framebuffer_row_max_pixel)) {
        output_buffer_with_offset[i] = framebuffer_row_data[i_with_offset];
      } else {
        output_buffer_with_offset[i] = GColorClear.argb;
      }
    }
#else
    memcpy(output_buffer_with_offset, framebuffer_row_data + framebuffer_current_column,
           remaining_framebuffer_row_bytes);
#endif
    if (framebuffer_row_is_larger_than_chunk) {
      state->col = remaining_chunk_bytes * cols_per_byte;
    } else {
      state->col = 0;
      state->row++;
    }

    remaining_chunk_bytes -= remaining_framebuffer_row_bytes;
    output_buffer_with_offset += remaining_framebuffer_row_bytes;
  }

  return max_chunk_bytes - remaining_chunk_bytes;
}

void screenshot_send_next_chunk(void* raw_state) {
  ScreenshotState* state = (ScreenshotState*)raw_state;

  CommSession *session = state->session;
  uint32_t max_buf_len = comm_session_send_buffer_get_max_payload_length(session);
  uint32_t session_len = 0;
  if (!state->sent_header) {
    max_buf_len -= sizeof(ScreenshotHeader);
    session_len += sizeof(ScreenshotHeader);
  }

  void *buffer = kernel_zalloc(max_buf_len);
  if (!buffer) {
    PBL_LOG(LOG_LEVEL_WARNING, "Screenshot aborted, OOM.");
    prv_send_error_response(session, SCREENSHOT_OOM_ERROR);
    prv_finish(state);
  }
  uint32_t len = prv_framebuffer_next_chunk(&state->framebuffer, max_buf_len, buffer);
  session_len += len;

  if (len == 0) {
    kernel_free(buffer);
    prv_finish(state);
    return;
  }

  SendBuffer *sb;
  if (max_buf_len == 0 /* disconnected */ ||
      !(sb = comm_session_send_buffer_begin_write(session, SCREENSHOT_ENDPOINT_ID,
                                                  session_len, COMM_SESSION_DEFAULT_TIMEOUT))) {
    PBL_LOG(LOG_LEVEL_WARNING, "Terminating screenshot send early: %"PRIu32, max_buf_len);
    prv_finish(state);
    return;
  }

  if (state->sent_header) {
    comm_session_send_buffer_write(sb, buffer, len);
  } else {
    const ScreenshotHeader header = (const ScreenshotHeader) {
      .response_code = SCREENSHOT_OK,
#if SCREEN_COLOR_DEPTH_BITS == 1
      .version       = htonl(1),
#elif SCREEN_COLOR_DEPTH_BITS == 8
      .version       = htonl(2),
#else
#warning "Need SCREEN_COLOR_DEPTH_BITS for screenshot version."
#endif
      .width         = htonl(state->framebuffer.width),
      .height        = htonl(state->framebuffer.height),
    };
    comm_session_send_buffer_write(sb, (const uint8_t *) &header, sizeof(header));
    // Fill the rest of this packet with image data.
    comm_session_send_buffer_write(sb, buffer, len);

    state->sent_header = true;
  }
  comm_session_send_buffer_end_write(sb);

  kernel_free(buffer);

  prv_request_fast_connection(session);

  system_task_add_callback(screenshot_send_next_chunk, state);
}

void screenshot_protocol_msg_callback(CommSession *session, const uint8_t* msg_data, unsigned int msg_len) {
  uint8_t sub_command = msg_data[0];
  if (sub_command != 0x00) {
    PBL_LOG(LOG_LEVEL_ERROR, "first byte can't be %u", sub_command);
    prv_send_error_response(session, SCREENSHOT_MALFORMED_COMMAND);
    return;
  }

  if (s_screenshot_in_progress) {
    PBL_LOG(LOG_LEVEL_ERROR, "Screenshot already in progress.");
    // Use a low timeout, if we are already in screenshot_send_next_chunk with the send buffer locked, then this
    // would block for a long time, causing the comm_protocol_dispatch_message()'s 150ms max timeout to trip.
    prv_send_error_response(session, SCREENSHOT_ALREADY_IN_PROGRESS);
    return;
  }
  s_screenshot_in_progress = true;

  prv_request_fast_connection(session);

  compositor_freeze();

  s_screenshot_state = (ScreenshotState) {
    .session = session,
    .framebuffer =  (FrameBufferState) {
      .fb = compositor_get_framebuffer(),
      .row = 0,
      .col = 0,
      .width = DISP_COLS,
      .height = DISP_ROWS,
    },
    .sent_header = false,
  };

  screenshot_send_next_chunk(&s_screenshot_state);
}
