/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/comm_session/session.h"
#include "pbl/services/comm_session/session_analytics.h"

#include "pbl/util/uuid.h"

/**
 * @defgroup services_comm_session_session_transport Transport interface
 * @ingroup services_comm_session
 * @brief Interface between sessions and the transports that carry them.
 * @{
 */

/** @brief Opaque transport context, defined by each transport. */
typedef struct Transport Transport;

/**
 * @brief Send data enqueued in the session's send queue.
 *
 * @param transport Transport context.
 */
typedef void (*TransportSendNext)(Transport *transport);

/**
 * @brief Close the transport.
 *
 * Called when another transport opens a system session; the older one is closed. The
 * transport must call comm_session_close() before returning.
 *
 * @param transport Transport context.
 */
typedef void (*TransportClose)(Transport *transport);

/**
 * @brief Reset the transport.
 *
 * @param transport Transport context.
 */
typedef void (*TransportReset)(Transport *transport);

/**
 * @brief Request a connection response time through the Bluetooth connection manager.
 *
 * @param transport Transport context.
 * @param consumer Consumer making the request.
 * @param state Requested response time state.
 * @param max_period_secs Maximum time to stay in @p state.
 * @param granted_handler Called once the state is granted. May be NULL.
 */
typedef void (*TransportSetConnectionResponsiveness)(
    Transport *transport, enum pbl_bt_consumer consumer, enum pbl_bt_response_time_state state,
    uint16_t max_period_secs, pbl_bt_responsiveness_granted_cb_t granted_handler);

/**
 * @brief Get the UUID of the app the transport connects to.
 *
 * @param transport Transport context.
 * @return App UUID, or NULL if not known.
 */
typedef const Uuid *(*TransportGetUUID)(Transport *transport);

/**
 * @brief Get the transport type.
 *
 * @param transport Transport context.
 * @return Transport type.
 */
typedef CommSessionTransportType (*TransportGetType)(Transport *transport);

/**
 * @brief Schedule a callback that sends data over the transport.
 *
 * @param session Session with data to send.
 * @return True if the callback was scheduled.
 */
typedef bool (*TransportSchedule)(CommSession *session);

/**
 * @brief Check whether the current task is the one TransportSchedule runs callbacks on.
 *
 * @param transport Transport context.
 * @return True if called from that task.
 */
typedef bool (*TransportScheduleTask)(Transport *transport);

/** @brief Callbacks the session uses to drive its transport. */
typedef struct TransportImplementation {
  /**
   * Send newly enqueued data. Called with bt_lock() held; must cope with an empty send queue
   * (it may flush other data, e.g. acks).
   */
  TransportSendNext send_next;

  /** Close the transport. NULL if it cannot be closed from the watch (iAP). */
  TransportClose close;
  /** Reset the transport. */
  TransportReset reset;
  /** Set the connection response time. */
  TransportSetConnectionResponsiveness set_connection_responsiveness;

  /** Get the connected app UUID. NULL if the transport is not UUID-aware. */
  TransportGetUUID get_uuid;

  /** Get the transport type. */
  TransportGetType get_type;

  /**
   * Schedule a send callback. When NULL, pbl_bt_comm_schedule_send_next_job() is used.
   * Requires @ref is_current_task_schedule_task when set.
   */
  TransportSchedule schedule;
  /** Check whether the current task runs @ref schedule callbacks. */
  TransportScheduleTask is_current_task_schedule_task;
} TransportImplementation;

/** @brief Whose traffic a transport carries. */
typedef enum TransportDestination {
  /** Only the system, e.g. iAP with the Pebble iOS app. */
  TransportDestinationSystem,

  /** Only one Pebble app, e.g. iAP with a third party iOS app using PebbleKit iOS. */
  TransportDestinationApp,

  /** Both system and apps, e.g. plain SPP with the Pebble Android app. */
  TransportDestinationHybrid,
} TransportDestination;

/**
 * @brief Open a session on a transport.
 *
 * Opening a system or hybrid session closes an existing system session, unless either is
 * PULSE. The caller must hold bt_lock().
 *
 * @param transport Transport context.
 * @param implementation Transport callbacks.
 * @param destination Whose traffic the transport carries.
 * @return The new session, or NULL on failure (out of memory, or an existing system session
 *         that cannot be closed).
 */
CommSession *comm_session_open(Transport *transport, const TransportImplementation *implementation,
                               TransportDestination destination);

/**
 * @brief Close a session and release its resources.
 *
 * The caller must hold bt_lock().
 *
 * @param session Session to close.
 * @param reason Close reason, for analytics.
 */
void comm_session_close(CommSession *session, CommSessionCloseReason reason);

/**
 * @brief Feed received bytes into the session's receive router.
 *
 * The caller must hold bt_lock().
 *
 * @param session Receiving session.
 * @param data Received bytes.
 * @param data_size Length of @p data in bytes.
 */
void comm_session_receive_router_write(CommSession *session, const uint8_t *data, size_t data_size);

/**
 * @brief Get the total length of all queued outbound data.
 *
 * The caller must hold bt_lock().
 *
 * @param session Session to query.
 * @return Length in bytes.
 */
size_t comm_session_send_queue_get_length(const CommSession *session);

/**
 * @brief Copy queued outbound data.
 *
 * The caller must ensure enough data is queued (see comm_session_send_queue_get_length()) and
 * hold bt_lock(). comm_session_send_queue_get_read_pointer() avoids the copy.
 *
 * @param session Session to read from.
 * @param start_offset Offset into the queued data.
 * @param length Number of bytes to copy.
 * @param[out] data_out Destination buffer.
 * @return Number of bytes copied.
 */
size_t comm_session_send_queue_copy(CommSession *session, uint32_t start_offset, size_t length,
                                    uint8_t *data_out);

/**
 * @brief Get a pointer to the next contiguous chunk of queued data.
 *
 * The queue is not contiguous: call this and comm_session_send_queue_consume() repeatedly
 * until it returns 0 to access all data.
 *
 * @param session Session to read from.
 * @param[out] data_out Read pointer.
 * @return Number of bytes readable at @p data_out.
 */
size_t comm_session_send_queue_get_read_pointer(const CommSession *session,
                                                const uint8_t **data_out);

/**
 * @brief Remove sent data from the queue, freeing completed jobs.
 *
 * The caller must hold bt_lock().
 *
 * @param session Session whose queue to consume.
 * @param length Number of bytes sent.
 */
void comm_session_send_queue_consume(CommSession *session, size_t length);

/**
 * @brief Schedule a call to the transport's send_next().
 *
 * No-op if a call is already pending. The caller must hold bt_lock().
 *
 * @param session Session with data to send.
 */
void comm_session_send_next(CommSession *session);

/** @} */
