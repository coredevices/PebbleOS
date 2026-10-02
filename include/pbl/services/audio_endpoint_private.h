/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

#include <stdint.h>

/**
 * @addtogroup services_audio_endpoint
 * @{
 */

/** @brief Audio endpoint message types. */
typedef enum {
  /** Audio frames, watch to phone. */
  MsgIdDataTransfer = 0x02,
  /** End of transfer, in either direction. */
  MsgIdStopTransfer = 0x03,
} MsgId;

/** @brief Audio data message. */
typedef struct PBL_PACKED {
  /** @ref MsgIdDataTransfer. */
  MsgId msg_id;
  /** Transfer session. */
  AudioEndpointSessionId session_id;
  /** Number of frames that follow. */
  uint8_t frame_count;
  /** Frames, each a length byte followed by that many bytes of encoded audio. */
  uint8_t frames[];
} DataTransferMsg;

/** @brief Stop transfer message. */
typedef struct PBL_PACKED {
  /** @ref MsgIdStopTransfer. */
  MsgId msg_id;
  /** Transfer session. */
  AudioEndpointSessionId session_id;
} StopTransferMsg;

/** @} */
