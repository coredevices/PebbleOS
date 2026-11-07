/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/pp/session.h>
#include <pbl/util/list.h>

/**
 * @defgroup pp_send Sending
 * @ingroup pp
 * @brief Queueing outbound messages on a session.
 *
 * pbl_pp_send() copies a complete message. A send buffer builds one piecemeal:
 *
 * @code{.c}
 * struct pbl_pp_send_buffer *sb =
 *     pbl_pp_send_buffer_begin(session, PBL_PP_ENDPOINT_PING, len, PBL_PP_DEFAULT_TIMEOUT_MS);
 * if (sb == NULL) {
 *   return -ENOMEM;
 * }
 * pbl_pp_send_buffer_write(sb, header, sizeof(*header));
 * pbl_pp_send_buffer_write(sb, body, body_len);
 * pbl_pp_send_buffer_end(sb);
 * @endcode
 *
 * Code that already owns the message memory queues it without a copy as a @ref pbl_pp_send_job.
 * None of these functions may be called while holding a lock the transports take.
 * @{
 */

/** @brief Default time to wait for room in the send buffers, in milliseconds. */
#define PBL_PP_DEFAULT_TIMEOUT_MS 4000

/**
 * @brief Send a complete message.
 *
 * @param session Destination session.
 * @param endpoint_id Endpoint ID.
 * @param data Payload.
 * @param length Length of @p data in bytes.
 * @param timeout_ms Maximum time to wait for room in the send buffers.
 * @retval 0 Queued.
 * @retval -ENOTCONN @p session is not open.
 * @retval -EMSGSIZE @p length exceeds pbl_pp_send_buffer_max_payload().
 * @retval -EAGAIN No room before @p timeout_ms.
 */
int pbl_pp_send(struct pbl_pp_session *session, uint16_t endpoint_id, const uint8_t *data,
                size_t length, uint32_t timeout_ms);

/** @brief Message being built, see pbl_pp_send_buffer_begin(). */
struct pbl_pp_send_buffer;

/**
 * @brief Get the largest payload a send buffer can hold.
 *
 * @param session Session.
 * @return Length in bytes, 0 if @p session is not open.
 */
size_t pbl_pp_send_buffer_max_payload(const struct pbl_pp_session *session);

/**
 * @brief Start building a message.
 *
 * Blocks until there is room for @p length bytes of payload, or @p timeout_ms expires.
 *
 * @param session Destination session.
 * @param endpoint_id Endpoint ID.
 * @param length Payload length to reserve, in bytes.
 * @param timeout_ms Maximum time to wait for room.
 * @return Send buffer, or NULL if @p session is not open, @p length is too large or there was no
 *         room in time.
 */
struct pbl_pp_send_buffer *pbl_pp_send_buffer_begin(struct pbl_pp_session *session,
                                                    uint16_t endpoint_id, size_t length,
                                                    uint32_t timeout_ms);

/**
 * @brief Start building a message of a bulk transfer.
 *
 * Same as pbl_pp_send_buffer_begin(), but takes from a separate budget, so that bulk transfers
 * do not starve interactive messages.
 *
 * @param session Destination session.
 * @param endpoint_id Endpoint ID.
 * @param length Payload length to reserve, in bytes.
 * @param timeout_ms Maximum time to wait for room.
 * @return Send buffer, or NULL on failure.
 */
struct pbl_pp_send_buffer *pbl_pp_send_buffer_begin_bulk(struct pbl_pp_session *session,
                                                         uint16_t endpoint_id, size_t length,
                                                         uint32_t timeout_ms);

/**
 * @brief Append payload to a message.
 *
 * @param sb Send buffer.
 * @param data Payload data.
 * @param length Length of @p data in bytes.
 * @return true if it fit in the reserved length.
 */
bool pbl_pp_send_buffer_write(struct pbl_pp_send_buffer *sb, const uint8_t *data, size_t length);

/**
 * @brief Queue a message for sending and release the send buffer.
 *
 * @param sb Send buffer.
 */
void pbl_pp_send_buffer_end(struct pbl_pp_send_buffer *sb);

struct pbl_pp_send_job;

/** @brief Callbacks the send queue uses to drain a job. Called with the queue locked. */
struct pbl_pp_send_job_api {
  /**
   * @brief Get the number of bytes left to send, header included.
   *
   * @param job Job.
   * @return Length in bytes.
   */
  size_t (*get_length)(const struct pbl_pp_send_job *job);

  /**
   * @brief Copy bytes left to send.
   *
   * @param job Job.
   * @param offset Offset into the bytes left to send.
   * @param length Maximum number of bytes to copy.
   * @param[out] data_out Destination.
   * @return Number of bytes copied.
   */
  size_t (*copy)(const struct pbl_pp_send_job *job, size_t offset, size_t length,
                 uint8_t *data_out);

  /**
   * @brief Get the next contiguous bytes left to send.
   *
   * @param job Job.
   * @param[out] data_out Read pointer.
   * @return Number of bytes readable at @p data_out.
   */
  size_t (*get_read_pointer)(const struct pbl_pp_send_job *job, const uint8_t **data_out);

  /**
   * @brief Mark bytes as sent.
   *
   * @param job Job.
   * @param length Number of bytes sent.
   */
  void (*consume)(const struct pbl_pp_send_job *job, size_t length);

  /**
   * @brief Release the job, sent completely or not.
   *
   * @param job Job.
   */
  void (*free)(struct pbl_pp_send_job *job);
};

/** @brief Message, header included, queued without a copy. Embedded in the owner's struct. */
struct pbl_pp_send_job {
  /** Node in the session's send queue. */
  ListNode node;
  /** Callbacks. */
  const struct pbl_pp_send_job_api *api;
};

/**
 * @brief Queue a job for sending.
 *
 * If @p session is not open, the job is freed and @p job is set to NULL.
 *
 * @param session Destination session.
 * @param[in,out] job Job, with @ref pbl_pp_send_job::api set.
 */
void pbl_pp_send_job_submit(struct pbl_pp_session *session, struct pbl_pp_send_job **job);

/** @} */
