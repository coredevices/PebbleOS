/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/assert.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/endpoint.h>
#include <pbl/pp/send.h>
#include <pbl/services/audio_endpoint.h>
#include <pbl/services/audio_endpoint_private.h>
#include <pbl/util/circular_buffer.h>

#include <comm/bt_lock.h>

PBL_LOG_MODULE_DEFINE(service_audio_endpoint, CONFIG_SERVICE_AUDIO_ENDPOINT_LOG_LEVEL);

#define ACTIVE_MODE_TIMEOUT      (10000)
#define ACTIVE_MODE_START_BUFFER (100)

#define ACTIVE_MODE_PERIOD_MS    (ACTIVE_MODE_TIMEOUT - ACTIVE_MODE_START_BUFFER)
#define ACTIVE_MODE_PERIOD_TICKS (pbl_ms_to_ticks(ACTIVE_MODE_PERIOD_MS))

static_assert(ACTIVE_MODE_TIMEOUT > ACTIVE_MODE_START_BUFFER,
              "ACTIVE_MODE_TIMEOUT must be greater than ACTIVE_MODE_START_BUFFER");

typedef struct {
  AudioEndpointSessionId id;
  AudioEndpointStopTransferCallback stop_transfer;
  struct pbl_delayed_work active_mode_trigger;
  //! Cleared before canceling the trigger, which re-arms itself.
  bool active_mode_on;
} AudioEndpointSession;

static AudioEndpointSessionId s_session_id = AUDIO_ENDPOINT_SESSION_INVALID_ID;
static void prv_active_mode_timer_cb(struct pbl_work *work);

static AudioEndpointSession s_session = {
  .active_mode_trigger = PBL_DELAYED_WORK_INITIALIZER(prv_active_mode_timer_cb),
};
static uint32_t s_dropped_frames;

static void prv_session_deinit(bool call_stop_handler) {
  bt_lock();
  if (call_stop_handler && s_session.stop_transfer) {
    s_session.stop_transfer(s_session.id);
  }

  if (s_session.active_mode_on) {
    s_session.active_mode_on = false;
    pbl_delayed_work_cancel(&s_session.active_mode_trigger);
    struct pbl_pp_session *comm_session = pbl_pp_session_get_system();
    pbl_pp_session_set_responsiveness(comm_session, PBL_BT_CONSUMER_PP_AUDIO_ENDPOINT,
                                      PBL_BT_RESPONSE_TIME_MAX, 0, nullptr);
  }

  s_session.id = AUDIO_ENDPOINT_SESSION_INVALID_ID;
  s_session.stop_transfer = nullptr;
  bt_unlock();

  if (s_dropped_frames > 0) {
    PBL_LOG_WRN("Dropped %" PRIu32 " frames during audio transfer", s_dropped_frames);
  }
}

void audio_endpoint_protocol_msg_callback(struct pbl_pp_session *session, const uint8_t *data,
                                          size_t size) {
  MsgId msg_id = data[0];
  if (size >= sizeof(StopTransferMsg) && msg_id == MsgIdStopTransfer) {
    StopTransferMsg *msg = (StopTransferMsg *)data;

    if (msg->session_id == s_session.id) {
      prv_session_deinit(true /* call_stop_handler */);
    } else {
      PBL_LOG_WRN("Received mismatching session id: %u vs %u", msg->session_id, s_session.id);
    }
  }
}

PBL_PP_ENDPOINT_DEFINE(PBL_PP_ENDPOINT_AUDIO, PBL_PP_ACCESS_PRIVATE,
                       audio_endpoint_protocol_msg_callback);

static void prv_start_active_mode(void *data) {
  struct pbl_pp_session *comm_session = pbl_pp_session_get_system();
  pbl_pp_session_set_responsiveness(
      comm_session, PBL_BT_CONSUMER_PP_AUDIO_ENDPOINT, PBL_BT_RESPONSE_TIME_MIN,
      PBL_BT_MIN_LATENCY_MODE_TIMEOUT_AUDIO_SECS, nullptr /* granted_handler */);
}

//! Never block on bt_lock from the high priority queue (pbl_pp_session_set_responsiveness
//! takes it); defer to SysWorkq.
static void prv_active_mode_timer_cb(struct pbl_work *work) {
  pbl_sys_work_call(prv_start_active_mode, nullptr);

  // pbl_sys_work_call() may wait, so the session may have stopped or restarted by now.
  if (s_session.active_mode_on && !pbl_delayed_work_is_pending(&s_session.active_mode_trigger)) {
    pbl_delayed_work_schedule_next(&s_session.active_mode_trigger, ACTIVE_MODE_PERIOD_TICKS);
  }
}

AudioEndpointSessionId audio_endpoint_setup_transfer(
    AudioEndpointStopTransferCallback stop_transfer) {
  if (s_session.id != AUDIO_ENDPOINT_SESSION_INVALID_ID) {
    return AUDIO_ENDPOINT_SESSION_INVALID_ID;
  }

  bt_lock();

  s_session.id = ++s_session_id;
  s_session.stop_transfer = stop_transfer;
  s_session.active_mode_on = true;
  s_dropped_frames = 0;

  // restart active mode before it expires, this way it will never be off during the transfer
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_session.active_mode_trigger,
                              PBL_MSEC(ACTIVE_MODE_PERIOD_MS));

  bt_unlock();

  prv_start_active_mode(nullptr);

  return s_session.id;
}

void audio_endpoint_add_frame(AudioEndpointSessionId session_id, uint8_t *frame,
                              uint8_t frame_size) {
  PBL_ASSERTN(session_id != AUDIO_ENDPOINT_SESSION_INVALID_ID);

  if (s_session.id != session_id) {
    return;
  }

  struct pbl_pp_session *comm_session = pbl_pp_session_get_system();
  struct pbl_pp_send_buffer *sb = pbl_pp_send_buffer_begin_bulk(
      comm_session, PBL_PP_ENDPOINT_AUDIO, sizeof(DataTransferMsg) + frame_size + 1,
      0 /* timeout_ms, never block */);
  if (!sb) {
    s_dropped_frames++;
    PBL_LOG_DBG("Dropping a frame...");
    return;
  }

  uint8_t header[sizeof(DataTransferMsg) + sizeof(uint8_t) /* frame_size */];
  DataTransferMsg *msg = (DataTransferMsg *)header;
  *msg = (const DataTransferMsg){
    .msg_id = MsgIdDataTransfer,
    .session_id = session_id,
    .frame_count = 1,
  };
  msg->frames[0] = frame_size;

  pbl_pp_send_buffer_write(sb, header, sizeof(header));
  pbl_pp_send_buffer_write(sb, frame, frame_size);
  pbl_pp_send_buffer_end(sb);
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

  StopTransferMsg msg = (const StopTransferMsg){
    .msg_id = MsgIdStopTransfer,
    .session_id = session_id,
  };

  prv_session_deinit(false /* call_stop_handler */);

  pbl_pp_send(pbl_pp_session_get_system(), PBL_PP_ENDPOINT_AUDIO, (const uint8_t *)&msg,
              sizeof(msg), PBL_PP_DEFAULT_TIMEOUT_MS);
}
