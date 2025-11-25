/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

static const uint8_t HDLC_FLAG = 0x7E;
static const uint8_t HDLC_ESCAPE = 0x7D;
static const uint8_t HDLC_ESCAPE_MASK = 0x20;

typedef struct {
  bool escape;
} HdlcStreamingContext;

void hdlc_streaming_decode_reset(HdlcStreamingContext *ctx);
bool hdlc_streaming_decode(HdlcStreamingContext *ctx, uint8_t *data, bool *complete,
                           bool *is_invalid);
bool hdlc_encode(uint8_t *data);
