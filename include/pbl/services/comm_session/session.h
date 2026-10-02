/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "comm/bt_conn_mgr.h"

/**
 * @defgroup services_comm_session Communication sessions
 * @ingroup services
 * @brief Pebble Protocol sessions with the phone.
 *
 * A session carries Pebble Protocol messages over a transport (iAP for iOS, plain SPP for
 * Android, PPoGATT over BLE, QEMU, PULSE) and hides the differences between them.
 *
 * There are two kinds of sessions:
 * - The system session talks to the Pebble mobile app. Only one exists at a time; when another
 *   transport opens a system session, the previous one is closed. On Android it uses a hybrid
 *   transport that also carries PebbleKit app traffic.
 * - An app session connects directly to a third party phone app. PPoGATT can carry several app
 *   sessions at once, iAP only one.
 *
 * Sessions may disconnect at any time: a @ref CommSession pointer is only a handle, validated
 * by every call. Outbound messages are queued per session and drained by the transport.
 *
 * @code{.c}
 * CommSession *session = comm_session_get_system_session();
 * const uint8_t msg[] = {0x01, 0x02};
 *
 * if (!comm_session_send_data(session, endpoint_id, msg, sizeof(msg),
 *                             COMM_SESSION_DEFAULT_TIMEOUT)) {
 *   // Not connected, or no room in the send buffer before the timeout.
 * }
 * @endcode
 * @{
 */

/** @brief Opaque Pebble Protocol communication session. */
typedef struct CommSession CommSession;

/** @brief Kind of session. */
typedef enum {
  /** Not a valid (or no longer connected) session. */
  CommSessionTypeInvalid = -1,
  /** Session with the Pebble mobile app. */
  CommSessionTypeSystem = 0,
  /** Session with a third party phone app. */
  CommSessionTypeApp = 1,
  /** Number of valid session types. */
  NumCommSessions,
} CommSessionType;

/**
 * @brief Protocol capabilities announced by the phone.
 *
 * Bit positions match the fields of @ref PebbleProtocolCapabilities.
 */
typedef enum {
  /** App run state endpoint. */
  CommSessionRunState = 1 << 0,
  /** Infinite log dumping. */
  CommSessionInfiniteLogDumping = 1 << 1,
  /** Extended music service. */
  CommSessionExtendedMusicService = 1 << 2,
  /** Extended notification service. */
  CommSessionExtendedNotificationService = 1 << 3,
  /** Language pack installation. */
  CommSessionLanguagePackSupport = 1 << 4,
  /** 8 KiB AppMessage buffers. */
  CommSessionAppMessage8kSupport = 1 << 5,
  /** Activity insights. */
  CommSessionActivityInsightsSupport = 1 << 6,
  /** Voice API. */
  CommSessionVoiceApiSupport = 1 << 7,
  /** Send text. */
  CommSessionSendTextSupport = 1 << 8,
  /** Notification filtering. */
  CommSessionNotificationFilteringSupport = 1 << 9,
  /** Fetching unread coredumps. */
  CommSessionUnreadCoredumpSupport = 1 << 10,
  /** Weather app. */
  CommSessionWeatherAppSupport = 1 << 11,
  /** Reminders app. */
  CommSessionRemindersAppSupport = 1 << 12,
  /** Workout app. */
  CommSessionWorkoutAppSupport = 1 << 13,
  /** Smooth firmware install progress. */
  CommSessionSmoothFwInstallProgressSupport = 1 << 14,
  /** Phone serves images through the imaging endpoint. */
  CommSessionImagingSupport = 1 << 17,
  /** Phone syncs settings through BlobDB. */
  CommSessionSettingsSyncSupport = 1 << 23,
  /** First value past the defined capabilities. */
  CommSessionOutOfRange
} CommSessionCapability;

/** @brief Default send timeout in milliseconds. */
#define COMM_SESSION_DEFAULT_TIMEOUT (4000)

/**
 * @brief Check whether a session supports a capability.
 *
 * @param session Session to check.
 * @param capability Capability to look for.
 * @return True if @p session is valid and supports @p capability.
 */
bool comm_session_has_capability(CommSession *session, CommSessionCapability capability);

/**
 * @brief Get the capabilities of a session.
 *
 * @param session Session to query.
 * @return Capability bitset, 0 if @p session is not valid.
 */
CommSessionCapability comm_session_get_capabilities(CommSession *session);

/**
 * @brief Get the system (Pebble mobile app) session.
 *
 * The session may disconnect at any time after this returns.
 *
 * @return The system session, or NULL if not connected.
 */
CommSession *comm_session_get_system_session(void);

/**
 * @brief Get the session serving the currently running watch app.
 *
 * For apps that use PebbleKit JS this is the system session. The session may disconnect at
 * any time after this returns.
 *
 * @return The app session, or NULL if not connected.
 */
CommSession *comm_session_get_current_app_session(void);

/**
 * @brief Restrict a session to the one the current app is permitted to use.
 *
 * A NULL session selects the current app session. A non-NULL one is kept only if it is the
 * current app session.
 *
 * @param[in,out] session_in_out Session to sanitize; set to NULL if not permitted or if no
 *                               session is available.
 */
void comm_session_sanitize_app_session(CommSession **session_in_out);

/**
 * @brief Get the kind of a session.
 *
 * @param session Session to query.
 * @return Session type, or #CommSessionTypeInvalid if @p session is not valid.
 */
CommSessionType comm_session_get_type(const CommSession *session);

/**
 * @brief Get a session by kind.
 *
 * @param type Session type. #CommSessionTypeApp returns the session of the currently running
 *             app.
 * @return The session, or NULL if it does not exist.
 */
CommSession *comm_session_get_by_type(CommSessionType type);

/**
 * @brief Get the UUID of the app a session is connected to.
 *
 * The caller must hold bt_lock() and stop using the pointer once it is released.
 *
 * @param session Session to query.
 * @return App UUID, or NULL if not known.
 */
const Uuid *comm_session_get_uuid(const CommSession *session);

/**
 * @brief Check whether a session is the system session.
 *
 * @param session Session to check.
 * @return True if @p session is a valid system session.
 */
bool comm_session_is_system(CommSession *session);

/**
 * @brief Reset a session by asking its transport to close and reopen it.
 *
 * With iAP this closes every session on the transport, since a single iAP session cannot be
 * closed on its own.
 *
 * @param session Session to reset.
 */
void comm_session_reset(CommSession *session);

/**
 * @brief Send a complete Pebble Protocol message.
 *
 * Wraps comm_session_send_buffer_begin_write(), comm_session_send_buffer_write() and
 * comm_session_send_buffer_end_write(), so the message is copied to the kernel heap. Use
 * comm_session_send_queue_add_job() to avoid the copy, or the send buffer functions to build a
 * message piecemeal. bt_lock() must not be held.
 *
 * @param session Destination session.
 * @param endpoint_id Pebble Protocol endpoint ID.
 * @param data Message payload.
 * @param length Length of @p data in bytes.
 * @param timeout_ms Maximum time to block waiting for room in the send buffer.
 * @return True if the message was queued for sending.
 */
bool comm_session_send_data(CommSession *session, uint16_t endpoint_id, const uint8_t *data,
                            size_t length, uint32_t timeout_ms);

/**
 * @brief Request a connection response time for a session.
 *
 * Same as comm_session_set_responsiveness_ext() without a granted callback.
 *
 * @param session Session whose connection to adjust; ignored if not valid.
 * @param consumer Consumer making the request.
 * @param state Requested response time state.
 * @param max_period_secs Maximum time to stay in @p state before falling back to the slowest
 *                        response time.
 */
void comm_session_set_responsiveness(CommSession *session, enum pbl_bt_consumer consumer,
                                     enum pbl_bt_response_time_state state,
                                     uint16_t max_period_secs);

/**
 * @brief Request a connection response time for a session, with a granted callback.
 *
 * Forwarded to the transport, which applies it through the Bluetooth connection manager.
 *
 * @param session Session whose connection to adjust; ignored if not valid.
 * @param consumer Consumer making the request.
 * @param state Requested response time state.
 * @param max_period_secs Maximum time to stay in @p state before falling back to the slowest
 *                        response time.
 * @param granted_handler Called on KernelMain once a state at least as responsive as @p state
 *                        is entered. May be NULL.
 */
void comm_session_set_responsiveness_ext(CommSession *session, enum pbl_bt_consumer consumer,
                                         enum pbl_bt_response_time_state state,
                                         uint16_t max_period_secs,
                                         pbl_bt_responsiveness_granted_cb_t granted_handler);

/**
 * @brief Initialize the session module.
 *
 * Called when Bluetooth is enabled; no session may exist yet.
 */
void comm_session_init(void);

/** @} */
