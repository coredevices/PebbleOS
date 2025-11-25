/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/normal/audio_endpoint.h"
#include "services/normal/audio_endpoint_private.h"

#include "util/circular_buffer.h"
#include "util/list.h"

#include "stubs_bt_lock.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "fake_session.h"
#include "fake_system_task.h"
#include "fake_new_timer.h"

extern void audio_endpoint_protocol_msg_callback(CommSession *session,
                                                 const uint8_t* data, size_t size);

static AudioEndpointSessionId s_session_id;

static uint8_t s_test_frame[] = {
  0x01, 0x02, 0x03, 0x04,
};

static CommSession *s_session;

static void prv_test_data_transfer_msg(uint16_t endpoint_id, const uint8_t* data,
    unsigned int length) {
  DataTransferMsg *msg = (DataTransferMsg *)data;

  cl_assert(msg->msg_id == MsgIdDataTransfer);
  cl_assert(msg->session_id == s_session_id);

  for (unsigned int i = 0; i < msg->frame_count; i += (sizeof(msg->frame_count) +
      sizeof(s_test_frame))) {
    cl_assert(msg->frames[i] == sizeof(s_test_frame));

    for (unsigned int j = 0; j < sizeof(s_test_frame); j++) {
      cl_assert(msg->frames[i + sizeof(msg->frame_count) + j] == s_test_frame[j]);
    }
  }
}

static void prv_test_stop_transfer_msg(uint16_t endpoint_id, const uint8_t* data,
    unsigned int length) {
  StopTransferMsg *msg = (StopTransferMsg *)data;

  cl_assert(msg->msg_id == MsgIdStopTransfer);
  cl_assert(msg->session_id == s_session_id);
}

static void prv_test_stop_transfer_callback(AudioEndpointSessionId session_id) {
  cl_assert(session_id == s_session_id);
}

Transport *s_transport;

void test_audio_endpoint__initialize(void) {
  fake_comm_session_init();
  s_transport = fake_transport_create(TransportDestinationSystem, NULL, NULL);
  s_session = fake_transport_set_connected(s_transport, true);

  s_session_id = audio_endpoint_setup_transfer(prv_test_stop_transfer_callback);
  cl_assert(s_session_id != AUDIO_ENDPOINT_SESSION_INVALID_ID);
}


void test_audio_endpoint__session_control(void) {
  // Test that it is not possible to start another transfer session if one is already on-going:
  AudioEndpointSessionId session_id = audio_endpoint_setup_transfer(NULL);
  cl_assert(session_id == AUDIO_ENDPOINT_SESSION_INVALID_ID);

  audio_endpoint_stop_transfer(s_session_id);
  fake_transport_set_sent_cb(s_transport, prv_test_stop_transfer_msg);
  fake_comm_session_process_send_next();
}

void test_audio_endpoint__buffer_overflow(void) {
  // add a huge number of frames (1 kB) to the buffer to cause it to overflow
  for (unsigned int i = 0; i < 1024 / (sizeof(DataTransferMsg) + sizeof(s_test_frame)); i++) {
    audio_endpoint_add_frame(s_session_id, s_test_frame, sizeof(s_test_frame));
  }
  fake_transport_set_sent_cb(s_transport, prv_test_data_transfer_msg);
  fake_comm_session_process_send_next();

  audio_endpoint_stop_transfer(s_session_id);
  fake_transport_set_sent_cb(s_transport, prv_test_stop_transfer_msg);
  fake_comm_session_process_send_next();
}

void test_audio_endpoint__remote_stop_transfer(void) {
  StopTransferMsg *msg = (StopTransferMsg *)malloc(sizeof(StopTransferMsg));
  msg->msg_id = MsgIdStopTransfer;
  msg->session_id = s_session_id;
  audio_endpoint_protocol_msg_callback(s_session, (uint8_t *)msg, sizeof(StopTransferMsg));
  fake_system_task_callbacks_invoke_pending();
  free(msg);
}

void test_audio_endpoint__cleanup(void) {
  fake_comm_session_cleanup();
  fake_system_task_callbacks_cleanup();
  s_transport = NULL;

  fake_pbl_malloc_check_net_allocs();
  fake_pbl_malloc_clear_tracking();
}
