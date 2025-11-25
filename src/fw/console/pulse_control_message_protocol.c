/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if PULSE_EVERYWHERE

#include "pulse_control_message_protocol.h"

#include <util/attributes.h>

#include <stdint.h>
#include <string.h>

typedef struct PACKED PCMPPacket {
  uint8_t code;
  char information[];
} PCMPPacket;

enum PCMPCode {
  PCMPCode_EchoRequest = 1,
  PCMPCode_EchoReply = 2,
  PCMPCode_DiscardRequest = 3,

  PCMPCode_PortClosed = 129,
  PCMPCode_UnknownCode = 130,
};

void pulse_control_message_protocol_on_packet(
    PulseControlMessageProtocol *this, void *raw_packet, size_t packet_length) {
  if (packet_length < sizeof(PCMPPacket)) {
    // Malformed packet; silently discard.
    return;
  }

  PCMPPacket *packet = raw_packet;
  switch (packet->code) {
    case PCMPCode_EchoRequest: {
      PCMPPacket *reply = this->send_begin_fn(PULSE_CONTROL_MESSAGE_PROTOCOL);
      memcpy(reply, packet, packet_length);
      reply->code = PCMPCode_EchoReply;
      this->send_fn(reply, packet_length);
      break;
    }

    case PCMPCode_EchoReply:
    case PCMPCode_DiscardRequest:
    case PCMPCode_PortClosed:
    case PCMPCode_UnknownCode:
      break;

    default: {
      PCMPPacket *reply = this->send_begin_fn(PULSE_CONTROL_MESSAGE_PROTOCOL);
      reply->code = PCMPCode_UnknownCode;
      memcpy(reply->information, &packet->code, sizeof(packet->code));
      this->send_fn(reply, sizeof(PCMPPacket) + sizeof(packet->code));
      break;
    }
  }
}

void pulse_control_message_protocol_send_port_closed_message(
    PulseControlMessageProtocol *this, net16 port) {
  PCMPPacket *message = this->send_begin_fn(PULSE_CONTROL_MESSAGE_PROTOCOL);
  message->code = PCMPCode_PortClosed;
  memcpy(message->information, &port, sizeof(port));
  this->send_fn(message, sizeof(PCMPPacket) + sizeof(port));
}

#endif
