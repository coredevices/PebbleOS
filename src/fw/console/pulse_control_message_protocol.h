/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <util/net.h>

#include <stddef.h>
#include <stdint.h>

#define PULSE_CONTROL_MESSAGE_PROTOCOL (0x0001)

typedef const struct PulseControlMessageProtocol {
  void *(*send_begin_fn)(uint16_t app_protocol);
  void (*send_fn)(void *buf, size_t length);
} PulseControlMessageProtocol;

void pulse_control_message_protocol_on_packet(PulseControlMessageProtocol *this,
                                              void *information, size_t length);

void pulse_control_message_protocol_send_port_closed_message(
    PulseControlMessageProtocol *this, net16 port);
