/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pp_private.h"

#include <string.h>

#include <pbl/logging/logging.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>

PBL_LOG_MODULE_DECLARE(pp, CONFIG_PP_LOG_LEVEL);

#ifndef PBL_NO_LINKER_SCRIPT
extern const struct pbl_pp_endpoint __pbl_pp_endpoints_start[];
extern const struct pbl_pp_endpoint __pbl_pp_endpoints_end[];
#else
extern const struct pbl_pp_endpoint __pbl_pp_endpoints_start[] PBL_UNSORTED_SECTION_START(
    pbl_pp_ep);
extern const struct pbl_pp_endpoint __pbl_pp_endpoints_end[] PBL_UNSORTED_SECTION_END(pbl_pp_ep);
#endif

static const struct pbl_pp_endpoint *prv_find_endpoint(uint16_t id) {
  for (const struct pbl_pp_endpoint *e = __pbl_pp_endpoints_start; e < __pbl_pp_endpoints_end;
       e++) {
    if (e->id == id) {
      return e;
    }
  }

  return nullptr;
}

static bool prv_is_allowed(const struct pbl_pp_endpoint *endpoint,
                           const struct pbl_pp_session *session) {
  switch (pbl_pp_session_get_type(session)) {
    case PBL_PP_SESSION_SYSTEM:
      return (endpoint->access & PBL_PP_ACCESS_PRIVATE) != 0U;
    case PBL_PP_SESSION_APP:
      return (endpoint->access & PBL_PP_ACCESS_PUBLIC) != 0U;
    default:
      return false;
  }
}

static void prv_reset(struct pbl_pp__receive_state *rx) {
  *rx = (struct pbl_pp__receive_state){};
}

static void prv_skip(struct pbl_pp__receive_state *rx, uint16_t length) {
  prv_reset(rx);
  rx->bytes_to_ignore = length;
}

static void prv_start_message(struct pbl_pp_session *session) {
  struct pbl_pp__receive_state *rx = &session->rx;
  const struct pbl_pp_header *header = (const struct pbl_pp_header *)rx->header;
  const uint16_t id = pbl_be16_to_cpu(header->endpoint_id);
  const uint16_t length = pbl_be16_to_cpu(header->length);
  const struct pbl_pp_endpoint *endpoint = prv_find_endpoint(id);

  if (endpoint == nullptr || !prv_is_allowed(endpoint, session)) {
    pbl_pp__meta_send_error(
        session, (endpoint == nullptr) ? PBL_PP__META_UNHANDLED : PBL_PP__META_DISALLOWED, id);
    prv_skip(rx, length);
    return;
  }

  PBL_LOG_DBG("Receiving message: endpoint_id 0x%" PRIx16 ", payload_length %" PRIu16, id, length);

  const struct pbl_pp_receiver_api *api =
      (endpoint->receiver != nullptr) ? endpoint->receiver : &pbl_pp_kernel_receiver;
  struct pbl_pp_receiver *receiver = api->prepare(session, endpoint, length);

  if (receiver == nullptr) {
    PBL_LOG_ERR("No receiver for endpoint=%" PRIu16 " len=%" PRIu16, id, length);
    prv_skip(rx, length);
    return;
  }

  rx->receiver_api = api;
  rx->receiver = receiver;
  rx->payload_length = length;
}

void pbl_pp_session_receive(struct pbl_pp_session *session, const uint8_t *data, size_t length) {
  struct pbl_pp__receive_state *rx = &session->rx;

  PBL_LOG_HEXDUMP_VERBOSE(data, length);

  while (length > 0U) {
    if (rx->bytes_to_ignore > 0U) {
      const size_t ignored = MIN(length, rx->bytes_to_ignore);

      rx->bytes_to_ignore -= ignored;
      data += ignored;
      length -= ignored;
      continue;
    }

    if (rx->bytes_received < sizeof(struct pbl_pp_header)) {
      const size_t copied = MIN(sizeof(struct pbl_pp_header) - rx->bytes_received, length);

      memcpy(&rx->header[rx->bytes_received], data, copied);
      rx->bytes_received += copied;
      data += copied;
      length -= copied;

      if (rx->bytes_received < sizeof(struct pbl_pp_header)) {
        return;
      }

      prv_start_message(session);
      if (rx->receiver == nullptr) {
        continue;
      }
    }

    const size_t received =
        MIN(length, rx->payload_length + sizeof(struct pbl_pp_header) - rx->bytes_received);

    rx->receiver_api->write(rx->receiver, data, received);
    rx->bytes_received += received;
    data += received;
    length -= received;

    if (rx->bytes_received == sizeof(struct pbl_pp_header) + rx->payload_length) {
      rx->receiver_api->finish(rx->receiver);
      prv_reset(rx);
    }
  }
}

void pbl_pp__receive_drop(struct pbl_pp_session *session) {
  struct pbl_pp__receive_state *rx = &session->rx;

  if (rx->receiver_api != nullptr) {
    rx->receiver_api->cleanup(rx->receiver);
  }

  prv_reset(rx);
}
