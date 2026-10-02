/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>
#include <stdlib.h>

/**
 * @defgroup services_audio_endpoint Audio endpoint
 * @ingroup services
 * @brief Streams encoded audio frames from the watch to the phone.
 *
 * Only one transfer session can be active at a time. While a session is active, the Bluetooth
 * connection is kept in its low latency mode. Frames that do not fit in the send buffer are
 * dropped.
 *
 * @code{.c}
 * AudioEndpointSessionId id = audio_endpoint_setup_transfer(stop_cb);
 * if (id != AUDIO_ENDPOINT_SESSION_INVALID_ID) {
 *   audio_endpoint_add_frame(id, frame, frame_size);
 *   audio_endpoint_stop_transfer(id);
 * }
 * @endcode
 * @{
 */

/** @brief Identifies a transfer session. */
typedef uint16_t AudioEndpointSessionId;
/** @brief Invalid session identifier. */
#define AUDIO_ENDPOINT_SESSION_INVALID_ID (0)

/**
 * @brief Called when the phone asks to stop a transfer.
 *
 * @param session_id Session being stopped.
 */
typedef void (*AudioEndpointStopTransferCallback)(AudioEndpointSessionId session_id);

/**
 * @brief Create a session for transferring audio from the watch to the phone.
 *
 * @param stop_transfer Called when the phone sends a stop transfer message.
 * @return Session identifier, or @ref AUDIO_ENDPOINT_SESSION_INVALID_ID if a session is already
 *         active.
 */
AudioEndpointSessionId audio_endpoint_setup_transfer(
    AudioEndpointStopTransferCallback stop_transfer);

/**
 * @brief Send a frame of encoded audio.
 *
 * Ignored if @p session_id is not the active session. Never blocks; the frame is dropped if the
 * send buffer is full.
 *
 * @param session_id Session returned by audio_endpoint_setup_transfer().
 * @param frame Encoded audio data.
 * @param frame_size Size of @p frame in bytes.
 */
void audio_endpoint_add_frame(AudioEndpointSessionId session_id, uint8_t *frame,
                              uint8_t frame_size);

/**
 * @brief End a transfer and send a stop transfer message to the phone.
 *
 * The stop callback is not called.
 *
 * @param session_id Session returned by audio_endpoint_setup_transfer().
 */
void audio_endpoint_stop_transfer(AudioEndpointSessionId session_id);

/**
 * @brief End a transfer without notifying the phone.
 *
 * The stop callback is not called.
 *
 * @param session_id Session returned by audio_endpoint_setup_transfer().
 */
void audio_endpoint_cancel_transfer(AudioEndpointSessionId session_id);

/** @} */
