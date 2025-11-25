/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include "console/pulse_llc.h"
#include "console/pulse_protocol_impl.h"

#include "console/pulse_internal.h"
#include "util/attributes.h"
#include "util/math.h"

#define LLC_INMSG_LINK_ESTABLISHMENT_REQUEST (1)
#define LLC_INMSG_LINK_CLOSE_REQUEST (3)
#define LLC_INMSG_ECHO_REQUEST (5)
#define LLC_INMSG_CHANGE_BAUD (7)

#define LLC_OUTMSG_LINK_OPENED (2)
#define LLC_OUTMSG_LINK_CLOSED (4)
#define LLC_OUTMSG_ECHO_REPLY (6)
#define LLC_OUTMSG_INVALID_LLC_MESSAGE (128)
#define LLC_OUTMSG_UNKNOWN_PROTOCOL_NUMBER (129)


static void prv_bad_packet_response(uint8_t type, uint8_t bad_id, void *body,
                                    size_t body_length);
static void prv_handle_change_baud(void *body, size_t body_length);


void pulse_llc_handler(void *packet, size_t length) {
  if (!length) {
    // Message too small; doesn't contain a type field.
    uint8_t *message = pulse_best_effort_send_begin(PULSE_PROTOCOL_LLC);
    *message = LLC_OUTMSG_INVALID_LLC_MESSAGE;
    pulse_best_effort_send(message, sizeof(uint8_t));
    return;
  }

  uint8_t type = *(uint8_t*)packet;
  switch (type) {
    case LLC_INMSG_LINK_ESTABLISHMENT_REQUEST:
      pulse_llc_send_link_opened_msg();
      return;
    case LLC_INMSG_LINK_CLOSE_REQUEST:
      pulse_end();
      return;
    case LLC_INMSG_ECHO_REQUEST:
      *(uint8_t*)packet = LLC_OUTMSG_ECHO_REPLY;

      uint8_t *message = pulse_best_effort_send_begin(PULSE_PROTOCOL_LLC);
      memcpy(message, packet, length);

      pulse_best_effort_send(message, length);
      return;
    case LLC_INMSG_CHANGE_BAUD:
      prv_handle_change_baud((char*)packet + 1, length - 1);
      return;
    default:
      prv_bad_packet_response(LLC_OUTMSG_INVALID_LLC_MESSAGE, type,
                              (char*)packet + 1, length - 1);
      return;
  }
}

void pulse_llc_link_state_handler(PulseLinkState link_state) {
}

void pulse_llc_send_link_opened_msg(void) {
  typedef struct PACKED Response {
    uint8_t type;
    uint8_t pulse_version;
    uint16_t mtu;
    uint16_t mru;
    uint8_t timeout;
  } Response;

  Response *response = pulse_best_effort_send_begin(PULSE_PROTOCOL_LLC);
  *response = (Response) {
    .type = LLC_OUTMSG_LINK_OPENED,
    .pulse_version = 1,
    .mtu = PULSE_MAX_SEND_SIZE + PULSE_MIN_FRAME_LENGTH,
    .mru = PULSE_MAX_RECEIVE_UNIT,
    .timeout = PULSE_KEEPALIVE_TIMEOUT_DECISECONDS
  };

  pulse_best_effort_send(response, sizeof(Response));
}

void pulse_llc_send_link_closed_msg(void) {
  uint8_t *link_close_response = pulse_best_effort_send_begin(
      PULSE_PROTOCOL_LLC);
  *link_close_response = LLC_OUTMSG_LINK_CLOSED;

  pulse_best_effort_send(link_close_response, sizeof(uint8_t));
}

static void prv_bad_packet_response(uint8_t type, uint8_t bad_id, void *body,
                                    size_t body_length) {
  typedef struct PACKED Response {
    uint8_t type;
    uint8_t bad_identifier;
    char body[8];
  } Response;

  Response *response = pulse_best_effort_send_begin(PULSE_PROTOCOL_LLC);
  *response = (Response) {
    .type = type,
    .bad_identifier = bad_id
  };

  body_length = MIN(sizeof(response->body), body_length);
  if (body_length) {
    memcpy(response->body, body, body_length);
  }

  pulse_best_effort_send(response, 2 + body_length);
}

void pulse_llc_unknown_protocol_handler(uint8_t protocol, void *packet,
                                        size_t length) {
  prv_bad_packet_response(LLC_OUTMSG_UNKNOWN_PROTOCOL_NUMBER, protocol, packet,
                          length);
}

void prv_handle_change_baud(void *body, size_t body_length) {
  if (body_length != sizeof(uint32_t)) {
    // Don't send a response; the client will have already changed its receiver
    // baud rate.
    return;
  }
  uint32_t *new_baud = body;
  pulse_change_baud_rate(*new_baud);
}
