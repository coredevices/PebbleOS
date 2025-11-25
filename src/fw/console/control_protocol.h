/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

typedef const struct PPPControlProtocol PPPControlProtocol;

typedef enum PPPCPCloseWait {
  PPPCPCloseWait_NoWait,
  PPPCPCloseWait_WaitForClosed
} PPPCPCloseWait;

//! Notify the control protocol that the lower layer is ready to carry traffic.
void ppp_control_protocol_lower_layer_is_up(PPPControlProtocol *protocol);

//! Notify the control protocol that the lower layer is no longer ready
//! to carry traffic.
void ppp_control_protocol_lower_layer_is_down(PPPControlProtocol *protocol);

//! Notify the control protocol that the layer is administratively available
//! for carrying traffic.
void ppp_control_protocol_open(PPPControlProtocol *protocol);

//! Notify the control protocol that the layer is not allowed to be opened.
void ppp_control_protocol_close(PPPControlProtocol *protocol,
                                PPPCPCloseWait wait);

//! Pass an incoming packet to the control protocol.
void ppp_control_protocol_handle_incoming_packet(PPPControlProtocol *protocol,
                                                 void *packet, size_t length);
