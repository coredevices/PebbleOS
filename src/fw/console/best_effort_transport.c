/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if PULSE_EVERYWHERE

#include "pulse_protocol_impl.h"

#include "console/control_protocol.h"
#include "console/control_protocol_impl.h"
#include "console/pulse.h"
#include "console/pulse2_transport_impl.h"
#include "console/pulse_control_message_protocol.h"
#include "system/passert.h"
#include <util/attributes.h>
#include <util/net.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool s_layer_up = false;

// Best Effort transport Control Protocol
// ======================================

static void prv_on_this_layer_up(PPPControlProtocol *this) {
  s_layer_up = true;
#define REGISTER_PROTOCOL(N, HANDLER, LAYER_STATE_HANDLER) \
  LAYER_STATE_HANDLER(PulseLinkState_Open);
#include "console/pulse_protocol_registry.def"
#undef REGISTER_PROTOCOL
}

static void prv_on_this_layer_down(PPPControlProtocol *this) {
  s_layer_up = false;
#define REGISTER_PROTOCOL(N, HANDLER, LAYER_STATE_HANDLER) \
  LAYER_STATE_HANDLER(PulseLinkState_Closed);
#include "console/pulse_protocol_registry.def"
#undef REGISTER_PROTOCOL
}

static void prv_on_receive_code_reject(PPPControlProtocol *this,
                                       LCPPacket *packet) {
  // TODO
}

static PPPControlProtocolState s_becp_state = {};

static PPPControlProtocol s_becp_protocol = {
  .protocol_number = PULSE2_BEST_EFFORT_CONTROL_PROTOCOL,
  .state = &s_becp_state,
  .on_this_layer_up = prv_on_this_layer_up,
  .on_this_layer_down = prv_on_this_layer_down,
  .on_receive_code_reject = prv_on_receive_code_reject,
};

PPPControlProtocol * const PULSE2_BECP = &s_becp_protocol;

void pulse2_best_effort_control_on_packet(void *packet, size_t length) {
  ppp_control_protocol_handle_incoming_packet(PULSE2_BECP, packet, length);
}

// Best Effort Application Transport protocol
// ==========================================

typedef struct PACKED BestEffortPacket {
  net16 protocol;
  net16 length;
  char information[];
} BestEffortPacket;

static PulseControlMessageProtocol s_best_effort_pcmp = {
  .send_begin_fn = pulse_best_effort_send_begin,
  .send_fn = pulse_best_effort_send,
};

void pulse2_best_effort_transport_on_packet(void *raw_packet, size_t length) {
  if (!s_layer_up) {
    return;
  }

  BestEffortPacket *packet = raw_packet;
  if (length < ntoh16(packet->length)) {
    // Packet truncated; discard
    return;
  }
  size_t info_length = ntoh16(packet->length) - sizeof(BestEffortPacket);
  // This variable is read in the macro-expansion below, but linters
  // have a hard time figuring that out.
  (void)info_length;

  switch (ntoh16(packet->protocol)) {
    case PULSE_CONTROL_MESSAGE_PROTOCOL:
      pulse_control_message_protocol_on_packet(
          &s_best_effort_pcmp, packet->information, info_length);
      break;
#define REGISTER_PROTOCOL(N, HANDLER, LAYER_STATE_HANDLER) \
    case N: \
      HANDLER(packet->information, info_length); \
      break;
#include "console/pulse_protocol_registry.def"
#undef REGISTER_PROTOCOL
    default:
      pulse_control_message_protocol_send_port_closed_message(
          &s_best_effort_pcmp, packet->protocol);
      break;
  }
}

void *pulse_best_effort_send_begin(const uint16_t app_protocol) {
  PBL_ASSERTN(s_layer_up);
  BestEffortPacket *packet = pulse_link_send_begin(
      PULSE2_BEST_EFFORT_TRANSPORT_PROTOCOL);
  packet->protocol = hton16(app_protocol);
  return &packet->information;
}

void pulse_best_effort_send(void *buf, const size_t length) {
  PBL_ASSERTN(s_layer_up);
  PBL_ASSERT(length <= pulse_link_max_send_size() - sizeof(BestEffortPacket),
             "Packet too big to send");
  // We're blindly assuming that buf is the same pointer returned by
  // pulse_best_effort_send_begin. If it isn't, we'll either crash here
  // when trying to dereference it or we'll hit the assert in
  // pulse_link_send.
  BestEffortPacket *packet =
    (void *)((char *)buf - offsetof(BestEffortPacket, information));
  size_t packet_size = length + sizeof(BestEffortPacket);
  packet->length = hton16(packet_size);
  pulse_link_send(packet, packet_size);
}

// Shared events
// =============
void pulse2_best_effort_on_link_up(void) {
  ppp_control_protocol_lower_layer_is_up(PULSE2_BECP);
}

void pulse2_best_effort_on_link_down(void) {
  ppp_control_protocol_lower_layer_is_down(PULSE2_BECP);
}

void pulse2_best_effort_init(void) {
  ppp_control_protocol_init(PULSE2_BECP);
  ppp_control_protocol_open(PULSE2_BECP);
}

#endif
