/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

//! Maximum number of data bytes that an outgoing PULSE frame can hold.
#define PULSE_MAX_SEND_SIZE (520)

//! Possible link states for the PULSE link, used to notify protocol handlers
typedef enum {
  PulseLinkState_Open,
  PulseLinkState_Closed,
} PulseLinkState;

//! Retrieve the buffer to fill the frame.
//!
//! @param protocol protocol number
#if PULSE_EVERYWHERE
void *pulse_best_effort_send_begin(uint16_t protocol);
#else
void *pulse_best_effort_send_begin(uint8_t protocol);
#endif

//! Send a PULSE frame.
//!
//! @param [out] buf buffer containing the frame data to send. Must be a buffer
//!        pointer returned by pulse_send_begin
//! @param length length of the buffer pointed to by buf. Must not exceed
//!        PULSE_MAX_SEND_SIZE.
void pulse_best_effort_send(void *buf, size_t length);

//! Release a TX buffer, without sending the frame.
//!
//! @param [out] buf buffer to be released. Must be a buffer
//!        pointer returned by pulse_best_effort_send_begin
void pulse_best_effort_send_cancel(void *buf);

void *pulse_push_send_begin(uint16_t protocol);
void pulse_push_send(void *buf, size_t length);

void *pulse_reliable_send_begin(uint16_t protocol);
void pulse_reliable_send(void *buf, size_t length);
void pulse_reliable_send_cancel(void *buf);

size_t pulse_reliable_max_send_size(void);

#if !PULSE_EVERYWHERE
// PULSEv1 has no equivalent to the PUSH protocol.
#define pulse_push_send_begin pulse_best_effort_send_begin
#define pulse_push_send pulse_best_effort_send
#endif

// Use preprocessor magic to generate function signatures for all protocol
// handler functions.
#define REGISTER_PROTOCOL(n, message_handler, link_state_handler) \
    void message_handler(void *packet, size_t length); \
    void link_state_handler(PulseLinkState link_state);
#include "console/pulse_protocol_registry.def"
#undef REGISTER_PROTOCOL

#define ON_PACKET(N, PACKET_HANDLER) \
  void PACKET_HANDLER(void *packet, size_t length);
#define ON_TRANSPORT_STATE_CHANGE(UP_HANDLER, DOWN_HANDLER) \
  void UP_HANDLER(void); \
  void DOWN_HANDLER(void);
#include "console/pulse2_reliable_protocol_registry.def"
#undef ON_PACKET
#undef ON_TRANSPORT_STATE_CHANGE
