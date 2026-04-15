/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <util/attributes.h>
#include <stdint.h>

typedef struct PACKED {
  uint16_t length;
  uint16_t endpoint_id;
} PebbleProtocolHeader;

#define COMM_PRIVATE_MAX_INBOUND_PAYLOAD_SIZE 2044
#define COMM_PUBLIC_MAX_INBOUND_PAYLOAD_SIZE 144
// TODO: If we have memory to spare, let's crank this up to improve data spooling
#define COMM_MAX_OUTBOUND_PAYLOAD_SIZE 656
