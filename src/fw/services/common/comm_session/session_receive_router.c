/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "session_receive_router.h"

#include "services/common/comm_session/meta_endpoint.h"
#include "services/common/comm_session/session_analytics.h"
#include "services/common/comm_session/session_internal.h"
#include "system/hexdump.h"
#include "system/logging.h"
#include "util/math.h"
#include "util/net.h"
#include "util/size.h"

// Generated table of endpoint handler (s_protocol_endpoints):
#include "services/common/comm_session/protocol_endpoints_table.auto.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Static helper functions

static const PebbleProtocolEndpoint* prv_find_endpoint(uint16_t endpoint_id) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_protocol_endpoints); ++i) {
    const PebbleProtocolEndpoint* endpoint = &s_protocol_endpoints[i];
    if (!endpoint || endpoint->endpoint_id > endpoint_id) {
      break;
    }
    if (endpoint->endpoint_id == endpoint_id) {
      return endpoint;
    }
  }
  return NULL;
}

static bool prv_is_endpoint_allowed_with_session(const PebbleProtocolEndpoint* endpoint,
                                                 CommSession *session) {
  PebbleProtocolAccess granted_access_bitset = PebbleProtocolAccessNone;
  switch (comm_session_get_type(session)) {
    case CommSessionTypeSystem:
      granted_access_bitset = PebbleProtocolAccessPrivate; // Pebble app
      break;
    case CommSessionTypeApp:
      granted_access_bitset = PebbleProtocolAccessPublic; // 3rd party PebbleKit app
      break;
    default:
      break;
  }
  return (endpoint->access_mask & granted_access_bitset);
}

static MetaResponseCode prv_error_for_endpoint(const PebbleProtocolEndpoint* endpoint,
                                               CommSession *session) {
  if (!endpoint) {
    return MetaResponseCodeUnhandled;
  }
  if (!prv_is_endpoint_allowed_with_session(endpoint, session)) {
    return MetaResponseCodeDisallowed;
  }
  return MetaResponseCodeNoError;
}

static void prv_cleanup_router(ReceiveRouter *rtr) {
  memset(rtr, 0, sizeof(*rtr));
}

static bool prv_copy_header(ReceiveRouter *rtr, size_t *data_size_p, const uint8_t **data_p) {
  // New message or still gathering the header of the message
  const uint16_t header_bytes_missing = sizeof(PebbleProtocolHeader) - rtr->bytes_received;
  const uint16_t header_bytes_to_copy = MIN(header_bytes_missing, *data_size_p);
  memcpy(rtr->header_buffer + rtr->bytes_received, *data_p, header_bytes_to_copy);
  *data_size_p -= header_bytes_to_copy;
  *data_p += header_bytes_to_copy;
  rtr->bytes_received += header_bytes_to_copy;

  if (rtr->bytes_received < sizeof(PebbleProtocolHeader)) {
    // Incomplete header, wait for more data to come.
    return true;
  }

  return false;
}

static bool prv_handle_endpoint_error_and_skip_message_if_needed(CommSession *session,
                                                             const PebbleProtocolEndpoint *endpoint,
                                                             const uint16_t endpoint_id) {
  MetaResponseInfo meta_response_info;
  meta_response_info.payload.error_code = prv_error_for_endpoint(endpoint, session);
  if (MetaResponseCodeNoError != meta_response_info.payload.error_code) {
    meta_response_info.payload.endpoint_id = endpoint_id;
    meta_response_info.session = session;
    meta_endpoint_send_response_async(&meta_response_info);
    return true;
  }
  return false;
}

static void prv_skip_message(ReceiveRouter *rtr, const uint32_t payload_length) {
  rtr->bytes_to_ignore = payload_length;
  rtr->bytes_received = 0;
}

static bool prv_ignore_skipped_message_if_needed(const uint8_t **data_p, size_t *data_size_p,
                                                 ReceiveRouter *rtr) {
  // Eat any bytes from an ignored, previous message:
  if (rtr->bytes_to_ignore) {
    const uint32_t num_ignored_bytes = MIN(*data_size_p, rtr->bytes_to_ignore);
    rtr->bytes_to_ignore -= num_ignored_bytes;
    *data_size_p -= num_ignored_bytes;
    if (*data_size_p == 0) {
      return true;  // we're done
    }
    *data_p += num_ignored_bytes;
  }
  return false;
}

static bool prv_prepare_receiver(const uint32_t payload_length,
                                 const PebbleProtocolEndpoint *endpoint, const uint16_t endpoint_id,
                                 CommSession *session, ReceiveRouter *rtr) {
  Receiver *receiver = endpoint->receiver_imp->prepare(session, endpoint,
                                                       payload_length);
  if (!receiver) {
    // If no receiver could be provided (buffers full?), ignore the message:

    // TODO: What to do here?
    // - Look into SPP flow control
    // - With PPoGATT: drop packet and rely on automatic retransmission?

    PBL_LOG(LOG_LEVEL_ERROR, "No receiver for endpoint=%"PRIu16" len=%"PRIu32,
            endpoint_id, payload_length);
    prv_skip_message(rtr, payload_length);
    return true;
  }

  rtr->receiver = receiver;
  rtr->msg_payload_length = payload_length;
  rtr->receiver_imp = endpoint->receiver_imp;

  return false;
}

static void prv_write_payload_to_receiver(ReceiveRouter *rtr, size_t *data_size_p,
                                          const uint8_t **data_p) {
  // Write the (partial) payload bytes to the Receiver:
  const uint32_t num_payload_bytes_left_to_receive =
  rtr->msg_payload_length + sizeof(PebbleProtocolHeader) - rtr->bytes_received;
  const uint32_t num_payload_bytes_received = MIN(*data_size_p, num_payload_bytes_left_to_receive);
  rtr->bytes_received += num_payload_bytes_received;
  rtr->receiver_imp->write(rtr->receiver, *data_p, num_payload_bytes_received);
  *data_p += num_payload_bytes_received;
  *data_size_p -= num_payload_bytes_received;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Exported functions

void comm_session_receive_router_write(CommSession *session,
                                       const uint8_t *data, size_t data_size) {
  comm_session_analytics_inc_bytes_received(session, data_size);

  PBL_LOG_D_VERBOSE(LOG_DOMAIN_COMM, "Received packet from BT");
  PBL_HEXDUMP_D(LOG_DOMAIN_COMM, LOG_LEVEL_DEBUG_VERBOSE, data, data_size);

  ReceiveRouter *rtr = &session->recv_router;

  while (data_size) {
    if (prv_ignore_skipped_message_if_needed(&data, &data_size, rtr)) {
      return;  // we're done
    }

    // Deal with the header:
    if (rtr->bytes_received < sizeof(PebbleProtocolHeader)) {
      if (prv_copy_header(rtr, &data_size, &data)) {
        return;  // Incomplete header, wait for more data to come.
      }

      // Complete header received!
      const PebbleProtocolHeader *header_big_endian = (PebbleProtocolHeader *)rtr->header_buffer;
      const uint16_t endpoint_id = ntohs(header_big_endian->endpoint_id);

      const PebbleProtocolEndpoint* endpoint = prv_find_endpoint(endpoint_id);
      const uint32_t payload_length = ntohs(header_big_endian->length);

      if (prv_handle_endpoint_error_and_skip_message_if_needed(session, endpoint, endpoint_id)) {
        prv_skip_message(rtr, payload_length);
        continue;  // while (data_size)
      }

      PBL_LOG_D(LOG_DOMAIN_COMM, LOG_LEVEL_DEBUG,
                "Receiving message:  endpoint_id 0x%"PRIx16" (%"PRIu16"), payload_length %"PRIu32,
                endpoint_id, endpoint_id, payload_length);

      if (prv_prepare_receiver(payload_length, endpoint, endpoint_id, session, rtr)) {
        continue;  // while (data_size)
      }
    }

    prv_write_payload_to_receiver(rtr, &data_size, &data);

    // If the message payload is completed, call the Receiver to process it:
    if (rtr->bytes_received == (sizeof(PebbleProtocolHeader) + rtr->msg_payload_length)) {
      rtr->receiver_imp->finish(rtr->receiver);

      // Wipe it, to avoid confusing ourselves when looking at core dumps:
      prv_cleanup_router(rtr);
    }
  }
}

void comm_session_receive_router_cleanup(CommSession *session) {
  ReceiveRouter *rtr = &session->recv_router;

  if (rtr->receiver_imp) {
    rtr->receiver_imp->cleanup(rtr->receiver);
  }

  // Wipe it, to avoid confusing ourselves when looking at core dumps:
  prv_cleanup_router(rtr);
}
