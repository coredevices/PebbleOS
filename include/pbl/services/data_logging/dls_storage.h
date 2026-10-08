/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/data_logging/dls_private.h>

/**
 * @defgroup services_data_logging_dls_storage Session storage
 * @ingroup services_data_logging
 * @brief Session data stored in a PFS file per session.
 *
 * All functions must be called from KernelBG, except during dls_storage_rebuild() at boot. On a
 * write error the session's file is deleted.
 * @{
 */

/** @brief Delete the storage files of all sessions. */
void dls_storage_invalidate_all(void);

/**
 * @brief Delete the storage file of a session.
 *
 * The file must not be open.
 *
 * @param session Session.
 */
void dls_storage_delete_logging_storage(DataLoggingSession *session);

/**
 * @brief Read data from a session without consuming it.
 *
 * Only whole items are read, so fewer bytes than requested may be returned.
 *
 * @param logging_session Session to read from.
 * @param[out] buffer Destination, or NULL to only get the number of bytes available.
 * @param num_bytes Maximum number of bytes to read, ignored when @p buffer is NULL.
 * @param[out] new_read_offset File offset of the byte following the data read.
 * @return Number of bytes read (or available), or -1 on error.
 */
int32_t dls_storage_read(DataLoggingSession *logging_session, uint8_t *buffer, int32_t num_bytes,
                         uint32_t *new_read_offset);

/**
 * @brief Consume data from a session without reading it.
 *
 * @param logging_session Session to consume from.
 * @param num_bytes Number of bytes to consume. 0 only resynchronizes the read offset with the
 *                  data already consumed, as done when rebuilding sessions at boot.
 * @return Number of bytes consumed, or -1 on error.
 */
int32_t dls_storage_consume(DataLoggingSession *logging_session, int32_t num_bytes);

/**
 * @brief Move the data from a buffered session's RAM buffer to flash.
 *
 * Must be called with the list mutex held. Does nothing for inactive or unbuffered sessions.
 *
 * @param session Session.
 * @return true on success.
 */
bool dls_storage_write_session(DataLoggingSession *session);

/**
 * @brief Write data directly to a session's file.
 *
 * Used for unbuffered sessions; the session must be locked with dls_lock_session().
 *
 * @param session Session.
 * @param data Data to write.
 * @param num_bytes Length of @p data in bytes.
 * @return true on success.
 */
bool dls_storage_write_data(DataLoggingSession *session, const void *data, uint32_t num_bytes);

/** @brief Recreate the sessions stored in flash, called at boot from dls_init(). */
void dls_storage_rebuild(void);

/** @} */
