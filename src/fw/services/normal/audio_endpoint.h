/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>
#include <stdlib.h>

//! Endpoint for transferring audio data between the watch and phone
//! https://pebbletechnology.atlassian.net/wiki/pages/viewpage.action?pageId=491698

//! Session identifier passed to endpoint functions
typedef uint16_t AudioEndpointSessionId;
#define AUDIO_ENDPOINT_SESSION_INVALID_ID (0)

//! Function signature of the callback to handle stop transfer message received from phone
typedef void (*AudioEndpointStopTransferCallback)(AudioEndpointSessionId session_id);

//! Create a session for transferring audio data from watch to phone
//! @param stop_transfer Callback to handle stop transfer message received from phone.
//! @return Session identifier to pass to other endpoint functions
AudioEndpointSessionId audio_endpoint_setup_transfer(AudioEndpointStopTransferCallback stop_transfer);

//! Add a frame of audio data to session's internal buffer
//! @param session_id Session identifier returned by audio_endpoint_start_transfer
//! @param frame Pointer to frame of encoded audio data
//! @param frame_size Size of frame of encoded audio data in bytes
void audio_endpoint_add_frame(AudioEndpointSessionId session_id, uint8_t *frame,
    uint8_t frame_size);

//! Stop transferring audio data from watch to phone
//! @param session_id Session identifier returned by audio_endpoint_setup_transfer
void audio_endpoint_stop_transfer(AudioEndpointSessionId session_id);

//! Cancel a transfer session without sending a stop transfer message
//! @param session_id Session identifier returned by audio_endpoint_setup_transfer
void audio_endpoint_cancel_transfer(AudioEndpointSessionId session_id);
