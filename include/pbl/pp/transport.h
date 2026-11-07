/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/bluetooth/responsiveness.h>
#include <pbl/pp/session.h>
#include <pbl/util/uuid.h>

/**
 * @defgroup pp_transport Transports
 * @ingroup pp
 * @brief Interface between sessions and the links that carry them.
 *
 * A transport opens a session once its link is up, feeds it the received bytes, drains its send
 * queue when asked to, and closes it when the link goes down. Each transport serializes these
 * calls with its own lock, which it may hold while calling into the session; the session never
 * holds its lock while calling the transport.
 *
 * The callbacks may therefore race with the transport closing the session. They receive the
 * session, not the transport's context: a transport looks the session up among its own under its
 * own lock, and ignores the call if it no longer owns it.
 * @{
 */

/** @brief Whose traffic a session carries. */
enum pbl_pp_destination : uint8_t {
  /** Only the system, i.e. the Pebble mobile app. */
  PBL_PP_DESTINATION_SYSTEM,
  /** Only one phone app, e.g. a third party app using PebbleKit iOS. */
  PBL_PP_DESTINATION_APP,
  /** Both the system and PebbleKit apps, e.g. the Pebble Android app. */
  PBL_PP_DESTINATION_HYBRID,
};

/** @brief A transport. Statically allocated by each transport. */
struct pbl_pp_transport {
  /** Name, for logs. */
  const char *name;

  /**
   * Preference among open system sessions: pbl_pp_session_get_system() returns the one on the
   * transport with the highest value.
   */
  uint8_t priority;

  /**
   * Whether its system sessions coexist with others. Opening a system session closes the
   * existing one unless either of them is on a shared transport.
   */
  bool shared;

  /**
   * @brief Send queued data. Mandatory.
   *
   * Called on KernelMain after pbl_pp_session_send_next(), or on the task blocked in
   * pbl_pp_send_buffer_begin() if that is KernelMain. Must cope with an empty send queue.
   *
   * @param session Session with data to send.
   */
  void (*send_next)(struct pbl_pp_session *session);

  /**
   * @brief Close a session to make way for a new system session.
   *
   * Must call pbl_pp_session_close() before returning. NULL if sessions cannot be closed from the
   * watch, in which case new system sessions are refused.
   *
   * @param session Session.
   */
  void (*close)(struct pbl_pp_session *session);

  /**
   * @brief Reset a session, see pbl_pp_session_reset(). NULL if not supported.
   *
   * @param session Session.
   */
  void (*reset)(struct pbl_pp_session *session);

  /**
   * @brief Apply a connection response time, see pbl_pp_session_set_responsiveness().
   *
   * NULL if the transport has no such notion; requests are then granted at once.
   */
  void (*set_responsiveness)(struct pbl_pp_session *session, enum pbl_bt_consumer consumer,
                             enum pbl_bt_response_time_state state, uint16_t max_period_secs,
                             pbl_bt_responsiveness_granted_cb_t granted);
};

/**
 * @brief Open a session.
 *
 * A system or hybrid session closes the existing system session, see
 * @ref pbl_pp_transport::shared.
 *
 * @param transport Transport carrying the session.
 * @param destination Whose traffic the session carries.
 * @param app_uuid UUID of the phone app's watch app counterpart for app sessions, or NULL if
 *                 unknown.
 * @return The session, or NULL if out of memory or the existing system session cannot be closed.
 */
struct pbl_pp_session *pbl_pp_session_open(const struct pbl_pp_transport *transport,
                                           enum pbl_pp_destination destination,
                                           const Uuid *app_uuid);

/**
 * @brief Close a session and release its resources, dropping any queued data.
 *
 * @param session Session, open on the calling transport.
 */
void pbl_pp_session_close(struct pbl_pp_session *session);

/**
 * @brief Set the capabilities of a session.
 *
 * Used by transports that know their peer's capabilities up front. Otherwise the session asks
 * the phone once it opens.
 *
 * @param session Session.
 * @param capabilities Capability bits, see @ref pbl_pp_capability.
 */
void pbl_pp_session_set_capabilities(struct pbl_pp_session *session, uint32_t capabilities);

/**
 * @brief Feed received bytes to a session.
 *
 * Bytes may be split anywhere; complete messages are dispatched to their endpoints.
 *
 * @param session Receiving session, open on the calling transport.
 * @param data Received bytes.
 * @param length Length of @p data in bytes.
 */
void pbl_pp_session_receive(struct pbl_pp_session *session, const uint8_t *data, size_t length);

/**
 * @brief Schedule a call to the transport's @ref pbl_pp_transport::send_next.
 *
 * No-op if a call is already pending.
 *
 * @param session Session.
 */
void pbl_pp_session_send_next(struct pbl_pp_session *session);

/**
 * @brief Get the number of bytes queued for sending.
 *
 * @param session Session.
 * @return Length in bytes.
 */
size_t pbl_pp_session_send_queue_length(const struct pbl_pp_session *session);

/**
 * @brief Copy queued bytes without removing them.
 *
 * @param session Session.
 * @param offset Offset into the queued bytes.
 * @param length Maximum number of bytes to copy.
 * @param[out] data_out Destination.
 * @return Number of bytes copied.
 */
size_t pbl_pp_session_send_queue_copy(struct pbl_pp_session *session, size_t offset, size_t length,
                                      uint8_t *data_out);

/**
 * @brief Get the next contiguous queued bytes without copying them.
 *
 * The queue is not contiguous: alternate with pbl_pp_session_send_queue_consume() until it
 * returns 0 to read everything.
 *
 * @param session Session.
 * @param[out] data_out Read pointer, valid until the next consume.
 * @return Number of bytes readable at @p data_out.
 */
size_t pbl_pp_session_send_queue_peek(const struct pbl_pp_session *session,
                                      const uint8_t **data_out);

/**
 * @brief Remove sent bytes from the queue.
 *
 * @param session Session.
 * @param length Number of bytes sent.
 */
void pbl_pp_session_send_queue_consume(struct pbl_pp_session *session, size_t length);

/** @} */
