/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "dls_private.h"

#include <stdint.h>
#include <time.h>

#include <applib/data_logging.h>

/**
 * @defgroup services_data_logging_dls_list Session list
 * @ingroup services_data_logging
 * @brief In-memory list of data logging sessions, sorted by session ID.
 *
 * The list is protected by a recursive mutex. Each active session also has its own mutex, taken
 * with dls_lock_session().
 * @{
 */

/**
 * @brief Find a session by ID.
 *
 * @param session_id Session ID.
 * @return Session, or NULL if not found.
 */
DataLoggingSession *dls_list_find_by_session_id(uint8_t session_id);

/**
 * @brief Find the active session with a given tag and owner.
 *
 * @param tag Session tag.
 * @param app_uuid Owner UUID.
 * @return Session, or NULL if not found.
 */
DataLoggingSession *dls_list_find_active_session(uint32_t tag, const Uuid *app_uuid);

/**
 * @brief Remove a session from the list and free it.
 *
 * @param logging_session Session to remove.
 */
void dls_list_remove_session(DataLoggingSession *logging_session);

/** @brief Free all sessions in memory without touching flash. */
void dls_list_remove_all(void);

/**
 * @brief Assign a new random unique ID to a session and add it to the list.
 *
 * @param logging_session Session to add.
 * @return Assigned session ID.
 */
uint8_t dls_list_add_new_session(DataLoggingSession *logging_session);

/**
 * @brief Add a session that already has an ID.
 *
 * Used at boot when restoring sessions from flash.
 *
 * @param logging_session Session to add.
 */
void dls_list_insert_session(DataLoggingSession *logging_session);

/**
 * @brief Allocate a session.
 *
 * Only the given fields are set; @ref DataLoggingSession::storage and
 * @ref DataLoggingSession::comm must be initialized separately, and the session added with
 * dls_list_add_new_session() or dls_list_insert_session(). Active sessions also get their
 * @ref DataLoggingActiveState.
 *
 * @param tag Session tag.
 * @param type Item type.
 * @param size Item size in bytes.
 * @param app_uuid Owner UUID.
 * @param timestamp Creation time.
 * @param status Initial status.
 * @return Session, or NULL if there are already @c DLS_MAX_NUM_SESSIONS sessions.
 */
DataLoggingSession *dls_list_create_session(uint32_t tag, DataLoggingItemType type, uint16_t size,
                                            const Uuid *app_uuid, time_t timestamp,
                                            DataLoggingStatus status);

/**
 * @brief Iterate over the session list.
 *
 * @param cur Current session, or NULL to get the first one.
 * @return Next session, or NULL at the end of the list.
 */
DataLoggingSession *dls_list_get_next(DataLoggingSession *cur);

/**
 * @brief Callback for dls_list_for_each_session().
 *
 * Called with the list mutex held, with the session and the user data. Returns false to stop
 * iterating.
 */
typedef bool (*DlsListCallback)(DataLoggingSession *, void *);

/**
 * @brief Call a callback for each session, with the list mutex held.
 *
 * @param cb Callback.
 * @param data User data passed to @p cb.
 * @return false if @p cb stopped the iteration, true otherwise.
 */
bool dls_list_for_each_session(DlsListCallback cb, void *data);

/** @brief Initialize the session list. */
void dls_list_init(void);

/**
 * @brief Check whether a pointer refers to a session in the list.
 *
 * Safe to call with arbitrary pointers: only compares against the known sessions.
 *
 * @param logging_session Pointer to check.
 * @return true if @p logging_session is in the list.
 */
bool dls_list_is_session_valid(DataLoggingSession *logging_session);

/**
 * @brief Lock an active session.
 *
 * Must not be called with the list mutex held.
 *
 * @param session Session to lock.
 * @return true if the session was active and is now locked, false if it is inactive.
 */
bool dls_lock_session(DataLoggingSession *session);

/**
 * @brief Unlock a session locked with dls_lock_session().
 *
 * @param session Session to unlock.
 * @param inactivate Inactivate the session, and free its active state, once its last lock is
 *                   released.
 */
void dls_unlock_session(DataLoggingSession *session, bool inactivate);

/**
 * @brief Get the status of a session.
 *
 * @param session Session.
 * @return Session status.
 */
DataLoggingStatus dls_get_session_status(DataLoggingSession *session);

/** @brief Assert that the current task owns the list mutex. */
void dls_assert_own_list_mutex(void);

/** @brief Lock the list mutex (recursive). */
void dls_list_lock(void);

/** @brief Unlock the list mutex (recursive). */
void dls_list_unlock(void);

/** @} */
