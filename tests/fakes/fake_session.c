/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_session.h"

#include "comm/bt_lock.h"

#include "kernel/pbl_malloc.h"
#include "system/logging.h"
#include "services/common/comm_session/protocol.h"
#include "services/common/comm_session/session_send_buffer.h"
#include "services/common/system_task.h"
#include "util/circular_buffer.h"
#include "system/hexdump.h"

#include "clar_asserts.h"

#include "util/list.h"

#include <string.h>

extern void fake_system_task_callbacks_invoke_pending(void);

typedef struct CommSession {
  ListNode node;
  Transport *transport;
  const TransportImplementation *transport_imp;
  bool is_send_next_call_pending;
  TransportDestination destination;
  uint8_t *temp_write_buffer;
  uint16_t endpoint_id;
  uint16_t bytes_written;
  uint16_t max_out_payload_length;
  CircularBuffer send_buffer;
  uint8_t storage[1024];
} CommSession;

static CommSession *s_session_head;

static int s_session_close_call_count;
static int s_session_open_call_count;

bool comm_session_is_valid(const CommSession *session) {
  return list_contains((ListNode *) s_session_head, &session->node);
}

static bool prv_find_session_is_system_filter(ListNode *found_node, void *data) {
  const CommSessionType requested_type = (const bool) (uintptr_t) data;
  const TransportDestination destination = ((const CommSession *) found_node)->destination;
  switch (requested_type) {
    case CommSessionTypeApp:
      return destination == TransportDestinationApp || destination == TransportDestinationHybrid;
    case CommSessionTypeSystem:
      return destination == TransportDestinationSystem || destination == TransportDestinationHybrid;
    default:
      return false;
  }
}

bool comm_session_has_capability(CommSession *session, CommSessionCapability capability){
  return true;
}

CommSession * comm_session_get_by_type(CommSessionType type) {
  // TODO: This is not going to fly with multiple app sessions
  CommSession *session;
  bt_lock();
  {
    session = (CommSession *) list_find((ListNode *) s_session_head,
                                        prv_find_session_is_system_filter,
                                        (void *) (uintptr_t) type);
  }
  bt_unlock();
  return session;
}

CommSession* comm_session_get_system_session(void) {
  // TODO: What if Pebble App is connected via iSPP *and* PPoGATT ?
  return comm_session_get_by_type(CommSessionTypeSystem);
}

CommSession* comm_session_get_current_app_session(void) {
  // TODO: What if App is connected via iSPP *and* PPoGATT ?
  return comm_session_get_by_type(CommSessionTypeApp);
}

void comm_session_close(CommSession *session, CommSessionCloseReason reason) {
  cl_assert(list_contains((const ListNode *) s_session_head, &session->node));
  if (session->temp_write_buffer) {
    kernel_free(session->temp_write_buffer);
  }
  list_remove(&session->node, (ListNode **) &s_session_head, NULL);
  kernel_free(session);
  ++s_session_close_call_count;
}

void comm_session_receive_router_write(CommSession *session,
                                       const uint8_t *received_data,
                                       size_t num_bytes_to_copy) {
  PBL_LOG(LOG_LEVEL_DEBUG, "Received Data:");
  PBL_HEXDUMP(LOG_LEVEL_DEBUG, received_data, num_bytes_to_copy);
}

bool comm_session_send_data(CommSession *session, uint16_t endpoint_id,
                            const uint8_t *data, size_t length, uint32_t timeout_ms) {
  SendBuffer *sb = comm_session_send_buffer_begin_write(session, endpoint_id, length, timeout_ms);
  if (!sb) {
    return false;
  }
  comm_session_send_buffer_write(sb, data, length);
  comm_session_send_buffer_end_write(sb);
  return true;
}

CommSession * comm_session_open(Transport *transport, const TransportImplementation *implementation,
                                TransportDestination destination) {
  ++s_session_open_call_count;

  CommSession *session = kernel_malloc(sizeof(CommSession));
  memset(session, 0, sizeof(*session));
  *session = (const CommSession) {
    .transport = transport,
    .transport_imp = implementation,
    .destination = destination,
    .max_out_payload_length = COMM_MAX_OUTBOUND_PAYLOAD_SIZE,
  };

  const size_t max_pp_msg_size = session->max_out_payload_length + sizeof(PebbleProtocolHeader);
  // If this fails, you need to bump up the size of the storage[] array in the fake CommSession
  cl_assert(sizeof(session->storage) >= max_pp_msg_size);
  circular_buffer_init(&session->send_buffer, session->storage, max_pp_msg_size);

  s_session_head = (CommSession *) list_prepend((ListNode *) s_session_head, &session->node);
  return session;
}

size_t comm_session_send_queue_get_length(const CommSession *session) {
  cl_assert(list_contains((const ListNode *) s_session_head, &session->node));
  return circular_buffer_get_read_space_remaining(&session->send_buffer);
}

size_t comm_session_send_queue_copy(CommSession *session, uint32_t start_off, size_t length,
                                    uint8_t *data_out) {
  cl_assert(data_out);
  cl_assert(list_contains((const ListNode *) s_session_head, &session->node));
  return circular_buffer_copy_offset(&session->send_buffer, start_off, data_out, length);
}

void comm_session_send_queue_consume(CommSession *session, size_t length) {
  circular_buffer_consume(&session->send_buffer, length);
}

static void prv_send_next_kernel_bg_cb(void *data) {
  CommSession *session = (CommSession *) data;
  if (!list_contains((const ListNode *) s_session_head, (const ListNode *) session)) {
    // Session closed in the mean time
    return;
  }
  // Flip the flag before the send_next callback, so it can schedule again if needed.
  session->is_send_next_call_pending = false;

  // Kick the transport to send out the next bytes from the send buffer
  const size_t read_space = comm_session_send_queue_get_length(session);
  if (read_space) {
    session->transport_imp->send_next(session->transport);
  }
}

void comm_session_send_next(CommSession *session) {
  if (session->is_send_next_call_pending) {
    return;
  }
  system_task_add_callback(prv_send_next_kernel_bg_cb, session);
  session->is_send_next_call_pending = true;
}

bool prv_filter_by_transport_callback(ListNode *node, void *data) {
  return (((CommSession *) node)->transport == data);
}

CommSession * prv_find_session_by_transport(Transport *transport) {
  return (CommSession *) list_find((ListNode *) s_session_head,
                                   prv_filter_by_transport_callback, transport);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Send buffer fakes

size_t comm_session_send_buffer_get_max_payload_length(const CommSession *session) {
  size_t max_length = 0;
  if (comm_session_is_valid(session)) {
    max_length = session->send_buffer.buffer_size - sizeof(PebbleProtocolHeader);
  }
  return max_length;
}

SendBuffer * comm_session_send_buffer_begin_write(CommSession *session, uint16_t endpoint_id,
                                                  size_t required_free_length,
                                                  uint32_t timeout_ms) {
  if (!session) {
    return NULL;
  }
  if (!comm_session_is_valid(session)) {
    return NULL;
  }
  if (required_free_length + sizeof(PebbleProtocolHeader) >
      circular_buffer_get_write_space_remaining(&session->send_buffer)) {
    return NULL;
  }
  if (session->temp_write_buffer) {
    // Already writing, fake doesn't support multiple tasks trying to write at the same time
    return NULL;
  }
  session->temp_write_buffer = (uint8_t *) kernel_malloc(session->max_out_payload_length);
  session->bytes_written = 0;
  session->endpoint_id = endpoint_id;
  return (SendBuffer *) session;
}

bool comm_session_send_buffer_write(SendBuffer *sb, const uint8_t *data, size_t length) {
  CommSession *session = (CommSession *) sb;
  cl_assert(session);
  cl_assert(session->temp_write_buffer);
  cl_assert(length + session->bytes_written <= session->max_out_payload_length);

  memcpy(session->temp_write_buffer + session->bytes_written, data, length);
  session->bytes_written += length;
  return true;
}

void comm_session_send_buffer_end_write(SendBuffer *sb) {
  CommSession *session = (CommSession *) sb;
  cl_assert(session);
  cl_assert(session->temp_write_buffer);

  const PebbleProtocolHeader pp_header = {
    .length = session->bytes_written,
    .endpoint_id = session->endpoint_id,
  };

  circular_buffer_write(&session->send_buffer, (const uint8_t *) &pp_header, sizeof(pp_header));
  circular_buffer_write(&session->send_buffer, session->temp_write_buffer, session->bytes_written);

  kernel_free(session->temp_write_buffer);
  session->temp_write_buffer = NULL;
  session->endpoint_id = ~0;
  session->bytes_written = 0;
}

static uint32_t s_responsiveness_max_period_s;
static bool s_responsiveness_latency_is_reduced;
static ResponsivenessGrantedHandler s_last_responsiveness_granted_handler;

void comm_session_set_responsiveness(
    CommSession *session, BtConsumer consumer, ResponseTimeState state, uint16_t max_period_secs) {
  comm_session_set_responsiveness_ext(session, consumer, state, max_period_secs, NULL);
}

void comm_session_set_responsiveness_ext(CommSession *session, BtConsumer consumer,
                                         ResponseTimeState state, uint16_t max_period_secs,
                                         ResponsivenessGrantedHandler granted_handler) {

  s_responsiveness_max_period_s = max_period_secs;

  if (state == ResponseTimeMiddle) {
    s_responsiveness_latency_is_reduced = true;
  } else if (state == ResponseTimeMax) {
    s_responsiveness_latency_is_reduced = false;
  }

  s_last_responsiveness_granted_handler = granted_handler;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Session related functions

ResponsivenessGrantedHandler fake_comm_session_get_last_responsiveness_granted_handler(void) {
  return s_last_responsiveness_granted_handler;
}

int fake_comm_session_open_call_count(void) {
  return s_session_open_call_count;
}

int fake_comm_session_close_call_count(void) {
  return s_session_close_call_count;
}

void fake_comm_session_process_send_next(void) {
  CommSession *session = s_session_head;
  while (session) {
    CommSession *next = (CommSession *) session->node.next;
    comm_session_send_next(session);
    session = next;
  }
  fake_system_task_callbacks_invoke_pending();
}

uint32_t fake_comm_session_get_responsiveness_max_period(void) {
  return s_responsiveness_max_period_s;
}

uint32_t fake_comm_session_is_latency_reduced(void) {
  return s_responsiveness_latency_is_reduced;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Transport mock

typedef struct {
  ListNode node;
  uint16_t endpoint_id;
  size_t length;
  uint8_t data[];
} DataNode;

typedef struct {
  ListNode node;
  TransportDestination destination;
  FakeTransportSentCallback sent_cb;
  Uuid app_uuid;
  CommSession *session;

  //! When no sent_cb is used, data is appended to this list
  DataNode *sent_data;
} FakeTransport;

static FakeTransport *s_fake_transport_head;

static void prv_fake_transport_send_next(Transport *transport) {
  FakeTransport *fake_transport = (FakeTransport *) transport;
  cl_assert_equal_b(list_contains((const ListNode *) s_fake_transport_head,
  (const ListNode *) fake_transport), true);
  CommSession *session = fake_transport->session;
  PebbleProtocolHeader pp_header;
  uint8_t *buffer = kernel_malloc(1024);
  while (circular_buffer_copy(&session->send_buffer,
                              (uint8_t *) &pp_header, sizeof(pp_header)) == sizeof(pp_header)) {
    circular_buffer_copy_offset(&session->send_buffer, sizeof(pp_header), buffer, pp_header.length);
    if (fake_transport->sent_cb) {
      fake_transport->sent_cb(pp_header.endpoint_id, buffer, pp_header.length);
    } else {
      PBL_LOG(LOG_LEVEL_DEBUG, "Sending Data to PP endpoint %u (0x%x):",
              pp_header.endpoint_id, pp_header.endpoint_id);
      PBL_HEXDUMP(LOG_LEVEL_DEBUG, buffer, pp_header.length);

      DataNode *data_node = kernel_malloc(sizeof(DataNode) + pp_header.length);
      list_init(&data_node->node);
      data_node->endpoint_id = pp_header.endpoint_id;
      data_node->length = pp_header.length;
      memcpy(data_node->data, buffer, pp_header.length);
      fake_transport->sent_data = (DataNode *) list_prepend(&fake_transport->sent_data->node,
                                                            &data_node->node);
    }
    circular_buffer_consume(&session->send_buffer, sizeof(pp_header) + pp_header.length);
  }
  kernel_free(buffer);
}

static void prv_fake_transport_reset(Transport *transport) {
  cl_assert_(false, "Not implemented: prv_fake_transport_reset");
}

static const TransportImplementation s_fake_transport_implementation = {
  .send_next = prv_fake_transport_send_next,
  .reset = prv_fake_transport_reset,
};

Transport *fake_transport_create(TransportDestination destination,
                                 const Uuid *app_uuid,
                                 FakeTransportSentCallback sent_cb) {
  if (app_uuid == NULL) {
    cl_assert_(TransportDestinationSystem == destination ||
               TransportDestinationHybrid == TransportDestinationSystem,
               "When passing NULL app_uuid, the destination can only be System or Hybrid");
  } else {
    cl_assert_(TransportDestinationSystem == destination ||
               TransportDestinationHybrid == TransportDestinationSystem,
               "When passing an app_uuid, the destination can only be App or Hybrid");
  }
  FakeTransport *transport = (FakeTransport *) kernel_malloc(sizeof(FakeTransport));
  *transport = (const FakeTransport) {
    .destination = destination,
    .sent_cb = sent_cb,
  };
  if (app_uuid) {
    transport->app_uuid = *app_uuid;
  }
  s_fake_transport_head = (FakeTransport *) list_prepend((ListNode *) s_fake_transport_head,
                                                         &transport->node);
  return (Transport *) transport;
}

CommSession *fake_transport_set_connected(Transport *transport, bool connected) {
  FakeTransport *fake_transport = (FakeTransport *) transport;
  if (connected) {
    cl_assert_equal_p(fake_transport->session, NULL);
    fake_transport->session = comm_session_open(transport, &s_fake_transport_implementation,
                                                fake_transport->destination);
    return fake_transport->session;
  } else {
    cl_assert(fake_transport->session);
    comm_session_close(fake_transport->session, 0);
    fake_transport->session = NULL;
    return NULL;
  }
}

void fake_transport_set_sent_cb(Transport *transport, FakeTransportSentCallback sent_cb) {
  cl_assert(transport);
  FakeTransport *fake_transport = (FakeTransport *) transport;
  fake_transport->sent_cb = sent_cb;
}

void fake_transport_assert_sent(Transport *transport, uint16_t index, uint16_t endpoint_id,
                                const uint8_t data[], size_t length) {
  cl_assert(transport);
  FakeTransport *fake_transport = (FakeTransport *)transport;
  DataNode *data_node = fake_transport->sent_data;
  for (uint16_t i = 0; i <= index; ++i) {
    cl_assert_(data_node, "Sent out too few packets");

    if (i == index) {
      cl_assert_equal_i(data_node->endpoint_id, endpoint_id);
      cl_assert_equal_i(data_node->length, length);
      cl_assert_equal_m(data_node->data, data, length);
    }

    data_node = (DataNode *) data_node->node.next;
  }
}

void fake_transport_assert_nothing_sent(Transport *transport) {
  cl_assert(transport);
  FakeTransport *fake_transport = (FakeTransport *)transport;
  DataNode *data_node = fake_transport->sent_data;
  cl_assert_equal_p(data_node, NULL);
}

void fake_transport_destroy(Transport *transport) {
  FakeTransport *fake_transport = (FakeTransport *) transport;
  cl_assert(transport);
  cl_assert_equal_b(list_contains((const ListNode *)s_fake_transport_head,
                                  (const ListNode *)fake_transport), true);
  if (fake_transport->session) {
    // Causes clean up of CommSession:
    fake_transport_set_connected((Transport *)fake_transport, false /* connected */);
  }
  list_remove((ListNode *) fake_transport, (ListNode **) &s_fake_transport_head, NULL);
  DataNode *data_node = fake_transport->sent_data;
  while (data_node) {
    DataNode *next_data_node = (DataNode *)data_node->node.next;
    kernel_free(data_node);
    data_node = next_data_node;
  }
  kernel_free(fake_transport);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Transport helper functions

bool fake_comm_session_send_buffer_write_raw_by_transport(Transport *transport,
                                                          const uint8_t *data, size_t length) {
  CommSession *session = prv_find_session_by_transport(transport);
  cl_assert(session);
  return circular_buffer_write(&session->send_buffer, data, length);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Fake life cycle

void fake_comm_session_init(void) {
  cl_assert_(s_fake_transport_head == NULL,
             "Didn't clean up the fake transports? \
             Call fake_comm_session_cleanup() if you don't want to clean them up manually.");

  s_session_close_call_count = 0;
  s_session_open_call_count = 0;
  s_last_responsiveness_granted_handler = NULL;
}

void fake_comm_session_cleanup(void) {
  FakeTransport *fake_transport = s_fake_transport_head;
  while (fake_transport) {
    FakeTransport *next = (FakeTransport *) fake_transport->node.next;
    fake_transport_destroy((Transport *) fake_transport);
    fake_transport = next;
  }
}
