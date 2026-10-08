/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/comm_session/session.h>

/**
 * @defgroup services_comm_session_session_send_buffer Send buffers
 * @ingroup services_comm_session
 * @brief Build an outbound message piecemeal in a kernel-heap buffer.
 *
 * @code{.c}
 * SendBuffer *sb = comm_session_send_buffer_begin_write(session, endpoint_id, sizeof(hdr) + len,
 *                                                       COMM_SESSION_DEFAULT_TIMEOUT);
 * if (sb) {
 *   comm_session_send_buffer_write(sb, (const uint8_t *)&hdr, sizeof(hdr));
 *   comm_session_send_buffer_write(sb, body, len);
 *   comm_session_send_buffer_end_write(sb);
 * }
 * @endcode
 * @{
 */

/** @brief Opaque buffer holding one outbound message. */
typedef struct SendBuffer SendBuffer;

/**
 * @brief Get the largest payload a send buffer can hold.
 *
 * @param session Destination session.
 * @return Maximum payload length in bytes, or 0 if @p session is not valid.
 */
size_t comm_session_send_buffer_get_max_payload_length(const CommSession *session);

/**
 * @brief Allocate a kernel-heap buffer for an outbound message.
 *
 * Blocks until enough space is available or the timeout expires. Must be followed by
 * comm_session_send_buffer_end_write(). bt_lock() must not be held. Use
 * comm_session_send_queue_add_job() to avoid the kernel-heap allocation, or
 * comm_session_send_data() to send a message in one call.
 *
 * @param session Destination session.
 * @param endpoint_id Pebble Protocol endpoint ID.
 * @param required_free_length Payload space in bytes guaranteed to be available on success.
 * @param timeout_ms Maximum time to wait for the space.
 * @return Send buffer, or NULL on timeout, if the length exceeds the maximum payload, or if
 *         @p session is not valid.
 */
SendBuffer *comm_session_send_buffer_begin_write(CommSession *session, uint16_t endpoint_id,
                                                 size_t required_free_length, uint32_t timeout_ms);

/**
 * @brief Append payload data to a send buffer.
 *
 * bt_lock() may be held.
 *
 * @param send_buffer Buffer from comm_session_send_buffer_begin_write().
 * @param data Data to append.
 * @param length Length of @p data in bytes.
 * @return True if appended; false if it does not fit. Writes within the
 *         @c required_free_length passed to comm_session_send_buffer_begin_write() always fit.
 */
bool comm_session_send_buffer_write(SendBuffer *send_buffer, const uint8_t *data, size_t length);

/**
 * @brief Finish a message and queue it for sending.
 *
 * Ownership of @p send_buffer passes to the send queue. bt_lock() may be held.
 *
 * @param send_buffer Buffer from comm_session_send_buffer_begin_write().
 */
void comm_session_send_buffer_end_write(SendBuffer *send_buffer);

/** @} */
