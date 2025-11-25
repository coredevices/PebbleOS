/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

void pulse_llc_send_link_opened_msg(void);

void pulse_llc_send_link_closed_msg(void);

void pulse_llc_unknown_protocol_handler(uint8_t protocol, void *packet,
                                        size_t length);
