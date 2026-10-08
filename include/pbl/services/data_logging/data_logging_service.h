/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <applib/data_logging.h>
#include <pbl/util/uuid.h>
#include <kernel/pebble_tasks.h>

/**
 * @defgroup services_data_logging Data logging
 * @ingroup services
 * @brief Sessions of fixed-size items persisted to flash and spooled to the phone.
 *
 * A session is identified by a tag and the UUID of its owner (@c UUID_SYSTEM for system
 * services). Logged items are stored in a PFS file per session and sent to the phone over the
 * data logging endpoint every few minutes, or right away when the session is finished.
 *
 * Buffered sessions copy items into a RAM circular buffer that KernelBG writes to flash, so
 * logging does not block. Unbuffered sessions write to flash directly and may only be used from
 * KernelBG.
 *
 * @code{.c}
 * DataLoggingSession *s = dls_create(DlsSystemTagActivitySession, DATA_LOGGING_BYTE_ARRAY,
 *                                    sizeof(struct record), true, false, &(Uuid)UUID_SYSTEM);
 * if (s) {
 *   if (dls_log(s, &record, 1) != DATA_LOGGING_SUCCESS) {
 *     // dropped
 *   }
 *   dls_finish(s);
 * }
 * @endcode
 *
 * Defining @c DLS_DEBUG_SEND_IMMEDIATELY sends stored data to the phone after every dls_log(). A
 * long press on any launcher menu item also flushes all sessions.
 * @{
 */

// #define DLS_DEBUG_SEND_IMMEDIATELY

struct DataLoggingSession;
/** @brief Data logging session handle. */
typedef struct DataLoggingSession DataLoggingSession;

/** @brief Tags used by system services, all registered with @c UUID_SYSTEM. */
typedef enum {
  /** Device analytics heartbeat. */
  DlsSystemTagAnalyticsDeviceHeartbeat = 78,
  /** App analytics heartbeat. */
  DlsSystemTagAnalyticsAppHeartbeat = 79,
  /** Analytics event. */
  DlsSystemTagAnalyticsEvent = 80,
  /** Activity minute data. */
  DlsSystemTagActivityMinuteData = 81,
  /** Raw accelerometer samples. */
  DlsSystemTagActivityAccelSamples = 82,
  /** Activity sessions. */
  DlsSystemTagActivitySession = 84,
  /** Protobuf log sessions, see @ref services_protobuf_log. */
  DlsSystemTagProtobufLogSession = 85,
  // Tag 86 is retired; do not reuse.
  /** Native analytics heartbeat. */
  DlsSystemTagAnalyticsNativeHeartbeat = 87,
} DlsSystemTag;

/**
 * @brief Initialize the data logging service.
 *
 * Called at boot. Rebuilds the sessions stored in flash and starts the periodic flush.
 */
void dls_init(void);

/**
 * @brief Check whether dls_init() has run.
 *
 * @return true if data logging is initialized.
 */
bool dls_initialized(void);

/** @brief Delete all sessions, both in memory and in flash. */
void dls_clear(void);

/** @brief Stop the periodic flush of sessions to the phone. */
void dls_pause(void);

/** @brief Restart the periodic flush of sessions to the phone. */
void dls_resume(void);

/**
 * @brief Inactivate all non-system sessions created by a task.
 *
 * Used when the task exits. Data still in a session's RAM buffer may be lost; data already in
 * flash is still sent to the phone.
 *
 * @param task Task whose sessions are inactivated.
 */
void dls_inactivate_sessions(PebbleTask task);

/**
 * @brief Create a buffered session owned by the current process.
 *
 * @param tag Session tag.
 * @param item_type Type of the logged items.
 * @param item_size Size of one item in bytes, at most @c DLS_SESSION_MAX_BUFFERED_ITEM_SIZE.
 * @param buffer Buffer of at least @c DLS_SESSION_MIN_BUFFER_SIZE bytes, freed by the service
 *               when the session is closed. May be NULL only from the worker or kernel tasks,
 *               in which case the buffer is allocated on the kernel heap.
 * @param resume Reuse an active session with the same tag and UUID instead of finishing it.
 * @return Session, or NULL on invalid parameters or too many sessions.
 */
DataLoggingSession *dls_create_current_process(uint32_t tag, DataLoggingItemType item_type,
                                               uint16_t item_size, void *buffer, bool resume);

/**
 * @brief Create a session.
 *
 * Integer items must be 1, 2 or 4 bytes wide.
 *
 * @param tag Session tag.
 * @param item_type Type of the logged items.
 * @param item_size Size of one item in bytes.
 * @param buffered Use a RAM buffer allocated on the kernel heap. Buffered sessions may be created
 *                 from the worker or kernel tasks, unbuffered ones only from KernelBG.
 * @param resume Reuse an active session with the same tag and UUID instead of finishing it.
 * @param uuid Owner UUID.
 * @return Session, or NULL on invalid parameters or too many sessions.
 */
DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid);

/**
 * @brief Append items to a session.
 *
 * Buffered sessions copy the data and return; unbuffered ones write it to flash before returning.
 * Must not be called with the Bluetooth lock held.
 *
 * @param s Session.
 * @param data Items to log.
 * @param num_items Number of items in @p data.
 * @retval DATA_LOGGING_SUCCESS Data logged.
 * @retval DATA_LOGGING_INVALID_PARAMS @p num_items is 0, or the data does not fit in the buffer.
 * @retval DATA_LOGGING_CLOSED The session is not active.
 * @retval DATA_LOGGING_BUSY Not enough room in the session buffer.
 * @retval DATA_LOGGING_INTERNAL_ERR Writing to flash failed.
 */
DataLoggingResult dls_log(DataLoggingSession *s, const void *data, uint32_t num_items);

/**
 * @brief Finish a session.
 *
 * Waits up to one second for buffered data to reach flash, inactivates the session and triggers a
 * send of all sessions. The session is deleted once all its data has been sent.
 *
 * @param s Session.
 */
void dls_finish(DataLoggingSession *s);

/**
 * @brief Check whether a pointer refers to an existing session.
 *
 * Safe to call with arbitrary pointers: only compares against the known sessions.
 *
 * @param logging_session Pointer to check.
 * @return true if @p logging_session is a known session.
 */
bool dls_is_session_valid(DataLoggingSession *logging_session);

/**
 * @brief Send all stored data to the phone now instead of at the next periodic flush.
 *
 * Does nothing when sending is disabled.
 */
void dls_send_all_sessions(void);

/**
 * @brief Check whether sending to the phone is enabled.
 *
 * @return true if enabled by both the phone and the run level.
 */
bool dls_get_send_enable(void);

/**
 * @brief Enable or disable sending, as requested by the phone.
 *
 * @param setting true to enable.
 */
void dls_set_send_enable_pp(bool setting);

/**
 * @brief Enable or disable sending, as requested by the run level.
 *
 * @param setting true to enable.
 */
void dls_set_send_enable_run_level(bool setting);

/** @} */
