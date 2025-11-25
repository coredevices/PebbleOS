/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if PULSE_EVERYWHERE

#include "pulse_protocol_impl.h"

#include "console/pulse2_transport_impl.h"
#include "system/passert.h"
#include <util/attributes.h>
#include <util/net.h>

#include <stddef.h>
#include <stdint.h>

#define PULSE2_PUSH_TRANSPORT_PROTOCOL (0x5021)

typedef struct PACKED PushPacket {
  net16 protocol;
  net16 length;
  char information[];
} PushPacket;

void *pulse_push_send_begin(uint16_t app_protocol) {
  PushPacket *packet = pulse_link_send_begin(PULSE2_PUSH_TRANSPORT_PROTOCOL);
  packet->protocol = hton16(app_protocol);
  return &packet->information;
}

void pulse_push_send(void *buf, size_t length) {
  PBL_ASSERT(length <= pulse_link_max_send_size() - sizeof(PushPacket),
             "Packet to big to send");
  // We're blindly assuming that buf is the same pointer returned by
  // pulse_push_send_begin. If it isn't, we'll either crash here
  // when trying to dereference it or we'll hit the assert in
  // pulse_link_send.
  PushPacket *packet = (void *)((char *)buf - offsetof(PushPacket,
                                                       information));
  size_t packet_size = length + sizeof(PushPacket);
  packet->length = hton16(packet_size);
  pulse_link_send(packet, packet_size);
}

#endif
