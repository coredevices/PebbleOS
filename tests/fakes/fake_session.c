/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_session.h"

#include <errno.h>
#include <string.h>

#include <pbl/kernel/malloc.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/protocol.h>
#include <pbl/pp/send.h>
#include <pbl/services/app_session.h>
#include <pbl/util/circular_buffer.h>
#include <pbl/util/list.h>

#include <clar_asserts.h>

extern void fake_sys_work_callbacks_invoke_pending(void);

struct pbl_pp_session {
  ListNode node;
  const struct pbl_pp_transport *transport;
  bool is_send_next_call_pending;
  enum pbl_pp_destination destination;
  uint32_t capabilities;
  uint8_t *temp_write_buffer;
  uint16_t endpoint_id;
  uint16_t bytes_written;
  uint16_t max_out_payload_length;
  CircularBuffer send_buffer;
  uint8_t storage[1024];
};

static struct pbl_pp_session *s_session_head;

static int s_session_close_call_count;
static int s_session_open_call_count;

static bool prv_is_open(const struct pbl_pp_session *session) {
  return (session != nullptr) && list_contains((ListNode *)s_session_head, &session->node);
}

static bool prv_find_session_filter(ListNode *found_node, void *data) {
  const enum pbl_pp_session_type requested_type = (enum pbl_pp_session_type)(uintptr_t)data;
  const enum pbl_pp_destination destination =
      ((const struct pbl_pp_session *)found_node)->destination;
  switch (requested_type) {
    case PBL_PP_SESSION_APP:
      return destination == PBL_PP_DESTINATION_APP || destination == PBL_PP_DESTINATION_HYBRID;
    case PBL_PP_SESSION_SYSTEM:
      return destination == PBL_PP_DESTINATION_SYSTEM || destination == PBL_PP_DESTINATION_HYBRID;
    default:
      return false;
  }
}

static struct pbl_pp_session *prv_get_by_type(enum pbl_pp_session_type type) {
  return (struct pbl_pp_session *)list_find((ListNode *)s_session_head, prv_find_session_filter,
                                            (void *)(uintptr_t)type);
}

bool pbl_pp_session_has_capability(const struct pbl_pp_session *session,
                                   enum pbl_pp_capability capability) {
  return true;
}

uint32_t pbl_pp_session_get_capabilities(const struct pbl_pp_session *session) {
  return prv_is_open(session) ? session->capabilities : 0U;
}

void pbl_pp_session_set_capabilities(struct pbl_pp_session *session, uint32_t capabilities) {
  if (prv_is_open(session)) {
    session->capabilities = capabilities;
  }
}

struct pbl_pp_session *pbl_pp_session_get_system(void) {
  return prv_get_by_type(PBL_PP_SESSION_SYSTEM);
}

struct pbl_pp_session *pbl_pp_session_get_app(const Uuid *app_uuid) {
  return prv_get_by_type(PBL_PP_SESSION_APP);
}

enum pbl_pp_session_type pbl_pp_session_get_type(const struct pbl_pp_session *session) {
  if (!prv_is_open(session)) {
    return PBL_PP_SESSION_INVALID;
  }
  return (session->destination == PBL_PP_DESTINATION_APP) ? PBL_PP_SESSION_APP
                                                          : PBL_PP_SESSION_SYSTEM;
}

bool pbl_pp_session_is_system(const struct pbl_pp_session *session) {
  return pbl_pp_session_get_type(session) == PBL_PP_SESSION_SYSTEM;
}

void pbl_pp_session_reset(struct pbl_pp_session *session) {
  cl_assert_(false, "Not implemented: pbl_pp_session_reset");
}

struct pbl_pp_session *app_session_get_current(void) {
  return prv_get_by_type(PBL_PP_SESSION_APP);
}

void pbl_pp_session_close(struct pbl_pp_session *session) {
  cl_assert(prv_is_open(session));
  if (session->temp_write_buffer) {
    pbl_free(session->temp_write_buffer);
  }
  list_remove(&session->node, (ListNode **)&s_session_head, nullptr);
  pbl_free(session);
  ++s_session_close_call_count;
}

void pbl_pp_session_receive(struct pbl_pp_session *session, const uint8_t *data, size_t length) {
  PBL_LOG_DBG("Received Data:");
  PBL_LOG_HEXDUMP_DBG(data, length);
}

int pbl_pp_send(struct pbl_pp_session *session, uint16_t endpoint_id, const uint8_t *data,
                size_t length, uint32_t timeout_ms) {
  struct pbl_pp_send_buffer *sb =
      pbl_pp_send_buffer_begin(session, endpoint_id, length, timeout_ms);
  if (!sb) {
    return -EAGAIN;
  }
  pbl_pp_send_buffer_write(sb, data, length);
  pbl_pp_send_buffer_end(sb);
  return 0;
}

struct pbl_pp_session *pbl_pp_session_open(const struct pbl_pp_transport *transport,
                                           enum pbl_pp_destination destination,
                                           const Uuid *app_uuid) {
  ++s_session_open_call_count;

  struct pbl_pp_session *session = pbl_malloc(sizeof(struct pbl_pp_session));
  memset(session, 0, sizeof(*session));
  *session = (const struct pbl_pp_session){
    .transport = transport,
    .destination = destination,
    .max_out_payload_length = PBL_PP_MAX_OUTBOUND_PAYLOAD,
  };

  const size_t max_pp_msg_size = session->max_out_payload_length + sizeof(struct pbl_pp_header);
  // If this fails, you need to bump up the size of the storage[] array in the fake session
  cl_assert(sizeof(session->storage) >= max_pp_msg_size);
  circular_buffer_init(&session->send_buffer, session->storage, max_pp_msg_size);

  s_session_head =
      (struct pbl_pp_session *)list_prepend((ListNode *)s_session_head, &session->node);
  return session;
}

size_t pbl_pp_session_send_queue_length(const struct pbl_pp_session *session) {
  cl_assert(prv_is_open(session));
  return circular_buffer_get_read_space_remaining(&session->send_buffer);
}

size_t pbl_pp_session_send_queue_copy(struct pbl_pp_session *session, size_t offset, size_t length,
                                      uint8_t *data_out) {
  cl_assert(data_out);
  cl_assert(prv_is_open(session));
  return circular_buffer_copy_offset(&session->send_buffer, offset, data_out, length);
}

void pbl_pp_session_send_queue_consume(struct pbl_pp_session *session, size_t length) {
  circular_buffer_consume(&session->send_buffer, length);
}

static void prv_send_next_kernel_bg_cb(void *data) {
  struct pbl_pp_session *session = (struct pbl_pp_session *)data;
  if (!prv_is_open(session)) {
    return;
  }
  session->is_send_next_call_pending = false;

  if (pbl_pp_session_send_queue_length(session)) {
    session->transport->send_next(session);
  }
}

void pbl_pp_session_send_next(struct pbl_pp_session *session) {
  if (session->is_send_next_call_pending) {
    return;
  }
  pbl_sys_work_call(prv_send_next_kernel_bg_cb, session);
  session->is_send_next_call_pending = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Send buffer fakes

size_t pbl_pp_send_buffer_max_payload(const struct pbl_pp_session *session) {
  size_t max_length = 0;
  if (prv_is_open(session)) {
    max_length = session->send_buffer.buffer_size - sizeof(struct pbl_pp_header);
  }
  return max_length;
}

struct pbl_pp_send_buffer *pbl_pp_send_buffer_begin(struct pbl_pp_session *session,
                                                    uint16_t endpoint_id,
                                                    size_t required_free_length,
                                                    uint32_t timeout_ms) {
  if (!prv_is_open(session)) {
    return nullptr;
  }
  if (required_free_length + sizeof(struct pbl_pp_header) >
      circular_buffer_get_write_space_remaining(&session->send_buffer)) {
    return nullptr;
  }
  if (session->temp_write_buffer) {
    // Already writing, fake doesn't support multiple tasks trying to write at the same time
    return nullptr;
  }
  session->temp_write_buffer = (uint8_t *)pbl_malloc(session->max_out_payload_length);
  session->bytes_written = 0;
  session->endpoint_id = endpoint_id;
  return (struct pbl_pp_send_buffer *)session;
}

struct pbl_pp_send_buffer *pbl_pp_send_buffer_begin_bulk(struct pbl_pp_session *session,
                                                         uint16_t endpoint_id,
                                                         size_t required_free_length,
                                                         uint32_t timeout_ms) {
  return pbl_pp_send_buffer_begin(session, endpoint_id, required_free_length, timeout_ms);
}

bool pbl_pp_send_buffer_write(struct pbl_pp_send_buffer *sb, const uint8_t *data, size_t length) {
  struct pbl_pp_session *session = (struct pbl_pp_session *)sb;
  cl_assert(session);
  cl_assert(session->temp_write_buffer);
  cl_assert(length + session->bytes_written <= session->max_out_payload_length);

  memcpy(session->temp_write_buffer + session->bytes_written, data, length);
  session->bytes_written += length;
  return true;
}

void pbl_pp_send_buffer_end(struct pbl_pp_send_buffer *sb) {
  struct pbl_pp_session *session = (struct pbl_pp_session *)sb;
  cl_assert(session);
  cl_assert(session->temp_write_buffer);

  const struct pbl_pp_header pp_header = {
    .length = session->bytes_written,
    .endpoint_id = session->endpoint_id,
  };

  circular_buffer_write(&session->send_buffer, (const uint8_t *)&pp_header, sizeof(pp_header));
  circular_buffer_write(&session->send_buffer, session->temp_write_buffer, session->bytes_written);

  pbl_free(session->temp_write_buffer);
  session->temp_write_buffer = nullptr;
  session->endpoint_id = ~0;
  session->bytes_written = 0;
}

static uint32_t s_responsiveness_max_period_s;
static bool s_responsiveness_latency_is_reduced;
static pbl_bt_responsiveness_granted_cb_t s_last_responsiveness_granted_handler;

void pbl_pp_session_set_responsiveness(struct pbl_pp_session *session,
                                       enum pbl_bt_consumer consumer,
                                       enum pbl_bt_response_time_state state,
                                       uint16_t max_period_secs,
                                       pbl_bt_responsiveness_granted_cb_t granted_handler) {
  s_responsiveness_max_period_s = max_period_secs;

  if (state == PBL_BT_RESPONSE_TIME_MIDDLE) {
    s_responsiveness_latency_is_reduced = true;
  } else if (state == PBL_BT_RESPONSE_TIME_MAX) {
    s_responsiveness_latency_is_reduced = false;
  }

  s_last_responsiveness_granted_handler = granted_handler;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Session related functions

pbl_bt_responsiveness_granted_cb_t fake_comm_session_get_last_responsiveness_granted_handler(void) {
  return s_last_responsiveness_granted_handler;
}

int fake_comm_session_open_call_count(void) {
  return s_session_open_call_count;
}

int fake_comm_session_close_call_count(void) {
  return s_session_close_call_count;
}

void fake_comm_session_process_send_next(void) {
  struct pbl_pp_session *session = s_session_head;
  while (session) {
    struct pbl_pp_session *next = (struct pbl_pp_session *)session->node.next;
    pbl_pp_session_send_next(session);
    session = next;
  }
  fake_sys_work_callbacks_invoke_pending();
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

struct fake_transport {
  ListNode node;
  enum pbl_pp_destination destination;
  FakeTransportSentCallback sent_cb;
  Uuid app_uuid;
  struct pbl_pp_session *session;

  //! When no sent_cb is used, data is appended to this list
  DataNode *sent_data;
};

static struct fake_transport *s_fake_transport_head;

static bool prv_filter_by_session_callback(ListNode *node, void *data) {
  return ((struct fake_transport *)node)->session == data;
}

static void prv_fake_transport_send_next(struct pbl_pp_session *session) {
  struct fake_transport *fake_transport = (struct fake_transport *)list_find(
      (ListNode *)s_fake_transport_head, prv_filter_by_session_callback, session);
  cl_assert(fake_transport);
  struct pbl_pp_header pp_header;
  uint8_t *buffer = pbl_malloc(1024);
  while (circular_buffer_copy(&session->send_buffer, (uint8_t *)&pp_header, sizeof(pp_header)) ==
         sizeof(pp_header)) {
    circular_buffer_copy_offset(&session->send_buffer, sizeof(pp_header), buffer, pp_header.length);
    if (fake_transport->sent_cb) {
      fake_transport->sent_cb(pp_header.endpoint_id, buffer, pp_header.length);
    } else {
      PBL_LOG_DBG("Sending Data to PP endpoint %u (0x%x):", pp_header.endpoint_id,
                  pp_header.endpoint_id);
      PBL_LOG_HEXDUMP_DBG(buffer, pp_header.length);

      DataNode *data_node = pbl_malloc(sizeof(DataNode) + pp_header.length);
      list_init(&data_node->node);
      data_node->endpoint_id = pp_header.endpoint_id;
      data_node->length = pp_header.length;
      memcpy(data_node->data, buffer, pp_header.length);
      fake_transport->sent_data =
          (DataNode *)list_prepend(&fake_transport->sent_data->node, &data_node->node);
    }
    circular_buffer_consume(&session->send_buffer, sizeof(pp_header) + pp_header.length);
  }
  pbl_free(buffer);
}

static const struct pbl_pp_transport s_fake_transport = {
  .name = "fake",
  .send_next = prv_fake_transport_send_next,
};

struct fake_transport *fake_transport_create(enum pbl_pp_destination destination,
                                             const Uuid *app_uuid,
                                             FakeTransportSentCallback sent_cb) {
  struct fake_transport *transport = pbl_malloc(sizeof(*transport));
  *transport = (const struct fake_transport){
    .destination = destination,
    .sent_cb = sent_cb,
  };
  if (app_uuid) {
    transport->app_uuid = *app_uuid;
  }
  s_fake_transport_head =
      (struct fake_transport *)list_prepend((ListNode *)s_fake_transport_head, &transport->node);
  return transport;
}

struct pbl_pp_session *fake_transport_set_connected(struct fake_transport *transport,
                                                    bool connected) {
  if (connected) {
    cl_assert_equal_p(transport->session, nullptr);
    transport->session =
        pbl_pp_session_open(&s_fake_transport, transport->destination, &transport->app_uuid);
    return transport->session;
  }

  cl_assert(transport->session);
  pbl_pp_session_close(transport->session);
  transport->session = nullptr;
  return nullptr;
}

void fake_transport_set_sent_cb(struct fake_transport *transport,
                                FakeTransportSentCallback sent_cb) {
  cl_assert(transport);
  transport->sent_cb = sent_cb;
}

void fake_transport_assert_sent(struct fake_transport *transport, uint16_t index,
                                uint16_t endpoint_id, const uint8_t data[], size_t length) {
  cl_assert(transport);
  DataNode *data_node = transport->sent_data;
  for (uint16_t i = 0; i <= index; ++i) {
    cl_assert_(data_node, "Sent out too few packets");

    if (i == index) {
      cl_assert_equal_i(data_node->endpoint_id, endpoint_id);
      cl_assert_equal_i(data_node->length, length);
      cl_assert_equal_m(data_node->data, data, length);
    }

    data_node = (DataNode *)data_node->node.next;
  }
}

void fake_transport_assert_nothing_sent(struct fake_transport *transport) {
  cl_assert(transport);
  cl_assert_equal_p(transport->sent_data, nullptr);
}

void fake_transport_destroy(struct fake_transport *transport) {
  cl_assert(transport);
  cl_assert_equal_b(
      list_contains((const ListNode *)s_fake_transport_head, (const ListNode *)transport), true);
  if (transport->session) {
    fake_transport_set_connected(transport, false /* connected */);
  }
  list_remove((ListNode *)transport, (ListNode **)&s_fake_transport_head, nullptr);
  DataNode *data_node = transport->sent_data;
  while (data_node) {
    DataNode *next_data_node = (DataNode *)data_node->node.next;
    pbl_free(data_node);
    data_node = next_data_node;
  }
  pbl_free(transport);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Transport helper functions

bool fake_comm_session_send_buffer_write_raw(struct pbl_pp_session *session, const uint8_t *data,
                                             size_t length) {
  cl_assert(prv_is_open(session));
  return circular_buffer_write(&session->send_buffer, data, length);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Fake life cycle

void fake_comm_session_init(void) {
  cl_assert_(s_fake_transport_head == nullptr,
             "Didn't clean up the fake transports? \
             Call fake_comm_session_cleanup() if you don't want to clean them up manually.");

  s_session_close_call_count = 0;
  s_session_open_call_count = 0;
  s_last_responsiveness_granted_handler = nullptr;
}

void fake_comm_session_cleanup(void) {
  struct fake_transport *fake_transport = s_fake_transport_head;
  while (fake_transport) {
    struct fake_transport *next = (struct fake_transport *)fake_transport->node.next;
    fake_transport_destroy(fake_transport);
    fake_transport = next;
  }
}
