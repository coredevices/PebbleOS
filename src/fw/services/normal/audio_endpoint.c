/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "audio_endpoint.h"
#include "audio_endpoint_private.h"

#include "comm/bt_lock.h"
#include "kernel/pbl_malloc.h"
#include "services/common/comm_session/session_send_buffer.h"
#include "services/common/new_timer/new_timer.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/circular_buffer.h"

#define AUDIO_ENDPOINT (10000)

#define ACTIVE_MODE_TIMEOUT      (10000)
#define ACTIVE_MODE_START_BUFFER (100)

//! Size of the frame queue buffer. Needs to hold ~1.2s of encoded Speex frames
//! to absorb the BLE connection parameter transition from ResponseTimeMax to
//! ResponseTimeMin. At 50 frames/sec and ~200 bytes/frame, that's ~12KB.
#define FRAME_QUEUE_BUFFER_SIZE (16 * 1024)

_Static_assert(ACTIVE_MODE_TIMEOUT > ACTIVE_MODE_START_BUFFER,
  "ACTIVE_MODE_TIMEOUT must be greater than ACTIVE_MODE_START_BUFFER");

typedef struct {
  AudioEndpointSessionId id;
  AudioEndpointStopTransferCallback stop_transfer;
  TimerID active_mode_trigger;
  CircularBuffer frame_queue;
  uint8_t *frame_queue_storage;
} AudioEndpointSession;

static AudioEndpointSessionId s_session_id = AUDIO_ENDPOINT_SESSION_INVALID_ID;
static AudioEndpointSession s_session;
static uint32_t s_dropped_frames;

static void prv_session_deinit(bool call_stop_handler) {
  bt_lock();
  if (call_stop_handler && s_session.stop_transfer) {
    s_session.stop_transfer(s_session.id);
  }

  if (s_session.active_mode_trigger != TIMER_INVALID_ID) {
    new_timer_delete(s_session.active_mode_trigger);
    s_session.active_mode_trigger = TIMER_INVALID_ID;
    CommSession *comm_session = comm_session_get_system_session();
    comm_session_set_responsiveness(
        comm_session, BtConsumerPpAudioEndpoint, ResponseTimeMax, 0);
  }

  if (s_session.frame_queue_storage) {
    kernel_free(s_session.frame_queue_storage);
    s_session.frame_queue_storage = NULL;
  }

  s_session.id = AUDIO_ENDPOINT_SESSION_INVALID_ID;
  s_session.stop_transfer = NULL;
  bt_unlock();

  if (s_dropped_frames > 0) {
    PBL_LOG_INFO("Dropped %"PRIu32" frames during audio transfer", s_dropped_frames);
  }
}

void audio_endpoint_protocol_msg_callback(CommSession *session, const uint8_t* data, size_t size) {
  MsgId msg_id = data[0];
  if (size >= sizeof(StopTransferMsg) && msg_id == MsgIdStopTransfer) {
    StopTransferMsg *msg = (StopTransferMsg *)data;

    if (msg->session_id == s_session.id) {
      prv_session_deinit(true /* call_stop_handler */);
    } else {
      PBL_LOG_WRN("Received mismatching session id: %u vs %u",
              msg->session_id, s_session.id);
    }
  }
}

static bool prv_try_send_frame(CommSession *comm_session, AudioEndpointSessionId session_id,
                               const uint8_t *frame, uint8_t frame_size) {
  SendBuffer *sb = comm_session_send_buffer_begin_write(comm_session, AUDIO_ENDPOINT,
                                                        sizeof(DataTransferMsg) + frame_size + 1,
                                                        0 /* timeout_ms, never block */);
  if (!sb) {
    return false;
  }

  uint8_t header[sizeof(DataTransferMsg) + sizeof(uint8_t) /* frame_size */];
  DataTransferMsg *msg = (DataTransferMsg *)header;
  *msg = (const DataTransferMsg) {
    .msg_id = MsgIdDataTransfer,
    .session_id = session_id,
    .frame_count = 1,
  };
  msg->frames[0] = frame_size;

  comm_session_send_buffer_write(sb, header, sizeof(header));
  comm_session_send_buffer_write(sb, frame, frame_size);
  comm_session_send_buffer_end_write(sb);
  return true;
}

//! Try to drain queued frames into the BLE send buffer.
static void prv_drain_frame_queue(CommSession *comm_session, AudioEndpointSessionId session_id) {
  CircularBuffer *queue = &s_session.frame_queue;

  while (circular_buffer_get_read_space_remaining(queue) > 0) {
    // Read the frame size prefix
    const uint8_t *size_ptr;
    uint16_t readable;
    if (!circular_buffer_read(queue, sizeof(uint8_t), &size_ptr, &readable)) {
      break;
    }
    uint8_t frame_size = *size_ptr;

    // Peek the full entry (size + frame) to verify it's all available
    uint16_t entry_size = sizeof(uint8_t) + frame_size;
    if (circular_buffer_get_read_space_remaining(queue) < entry_size) {
      break;
    }

    // Copy the frame data out (it may wrap in the circular buffer)
    uint8_t frame_buf[UINT8_MAX];
    circular_buffer_copy_offset(queue, sizeof(uint8_t), frame_buf, frame_size);

    if (!prv_try_send_frame(comm_session, session_id, frame_buf, frame_size)) {
      // Send buffer full, stop draining
      break;
    }

    // Successfully sent, consume the entry
    circular_buffer_consume(queue, entry_size);
  }
}

static void prv_start_active_mode(void *data) {
  CommSession *comm_session = comm_session_get_system_session();
  comm_session_set_responsiveness_ext(comm_session, BtConsumerPpAudioEndpoint, ResponseTimeMin,
                                      MIN_LATENCY_MODE_TIMEOUT_AUDIO_SECS,
                                      NULL /* granted_handler */);
}

AudioEndpointSessionId audio_endpoint_setup_transfer(AudioEndpointStopTransferCallback stop_transfer) {

  if (s_session.id != AUDIO_ENDPOINT_SESSION_INVALID_ID) {
    return AUDIO_ENDPOINT_SESSION_INVALID_ID;
  }

  bt_lock();

  s_session.id = ++s_session_id;
  s_session.stop_transfer = stop_transfer;
  s_session.active_mode_trigger = new_timer_create();
  s_session.frame_queue_storage = kernel_malloc(FRAME_QUEUE_BUFFER_SIZE);
  PBL_ASSERTN(s_session.frame_queue_storage);
  circular_buffer_init(&s_session.frame_queue, s_session.frame_queue_storage,
                       FRAME_QUEUE_BUFFER_SIZE);
  s_dropped_frames = 0;

  // restart active mode before it expires, this way it will never be off during the transfer
  new_timer_start(s_session.active_mode_trigger, ACTIVE_MODE_TIMEOUT - ACTIVE_MODE_START_BUFFER,
      prv_start_active_mode, NULL, TIMER_START_FLAG_REPEATING);

  bt_unlock();

  prv_start_active_mode(NULL);

  return s_session.id;
}

void audio_endpoint_add_frame(AudioEndpointSessionId session_id, uint8_t *frame,
    uint8_t frame_size) {
  PBL_ASSERTN(session_id != AUDIO_ENDPOINT_SESSION_INVALID_ID);

  if (s_session.id != session_id) {
    return;
  }

  CommSession *comm_session = comm_session_get_system_session();

  // Try to drain any previously queued frames first
  prv_drain_frame_queue(comm_session, session_id);

  // If the queue still has data, we must queue the new frame to preserve ordering
  bool queued = circular_buffer_get_read_space_remaining(&s_session.frame_queue) > 0;
  if (!queued) {
    // Queue is empty, try sending directly
    if (prv_try_send_frame(comm_session, session_id, frame, frame_size)) {
      return;
    }
  }

  // Send buffer full (or queue not empty), buffer the frame for later
  uint8_t entry_header = frame_size;
  if (!circular_buffer_write(&s_session.frame_queue, &entry_header, sizeof(entry_header)) ||
      !circular_buffer_write(&s_session.frame_queue, frame, frame_size)) {
    s_dropped_frames++;
    PBL_LOG_DBG("Dropping a frame...");
  }
}

void audio_endpoint_cancel_transfer(AudioEndpointSessionId session_id) {
  PBL_ASSERTN(session_id != AUDIO_ENDPOINT_SESSION_INVALID_ID);

  if (s_session.id != session_id) {
    return;
  }

  prv_session_deinit(false /* call_stop_handler */);
}

void audio_endpoint_stop_transfer(AudioEndpointSessionId session_id) {
  PBL_ASSERTN(session_id != AUDIO_ENDPOINT_SESSION_INVALID_ID);

  if (s_session.id != session_id) {
    return;
  }

  // Flush any remaining queued frames before stopping
  CommSession *comm_session = comm_session_get_system_session();
  prv_drain_frame_queue(comm_session, session_id);

  StopTransferMsg msg = (const StopTransferMsg) {
    .msg_id = MsgIdStopTransfer,
    .session_id = session_id,
  };

  prv_session_deinit(false /* call_stop_handler */);

  comm_session_send_data(comm_session_get_system_session(), AUDIO_ENDPOINT, (const uint8_t *) &msg,
                         sizeof(msg), COMM_SESSION_DEFAULT_TIMEOUT);
}
