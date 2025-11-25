/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/data_logging/dls_private.h"

//! @file dls_storage.h
//!
//! All these functions are only safe to call from the system task.

//! Invalidate all data logging storage space.
void dls_storage_invalidate_all(void);

//! Erase the storage for the given session
//! @param[in] session pointer to session
void dls_storage_delete_logging_storage(DataLoggingSession *session);

//! Read data from the given session. If buffer is NULL, this routine simply returns
//! the number bytes available for reading (and num_bytes is ignored). This method returns
//! the actual number of bytes read, which may be less than the number of bytes requested
//! if the last read would end in the middle of a data chunk.
//! @param[in] logging_session session to read from
//! @param[in] buffer buffer to read bytes into
//! @param[in] num_bytes number of bytes to read
//! @param[out] *new_read_offset file offset of the next byte to read
//! @return number of bytes read, or -1 if error
int32_t dls_storage_read(DataLoggingSession *logging_session, uint8_t *buffer, int32_t num_bytes,
                         uint32_t *new_read_offset);

//! Consume data from the session without reading it into a buffer.
//! @param[in] logging_session session to read from
//! @param[in] num_bytes number of bytes to consume.
//! @return number of bytes consumed, or -1 if error
int32_t dls_storage_consume(DataLoggingSession *logging_session, int32_t num_bytes);

//! Move the data from the circular buffer in memory to flash.
//! @param[in] logging_session session to write to
//! @return true on success, false on failure
bool dls_storage_write_session(DataLoggingSession *session);

//! Write data directly to logging session storage from a passed in buffer
//! @param[in] logging_session session to write to
//! @param[in] data buffer to write from
//! @param[in] num_bytes number of bytes to write
//! @return true on success, false on failure
bool dls_storage_write_data(DataLoggingSession *session, const void *data, uint32_t num_bytes);

//! Call this at startup to read the session state that's been stored in the file system.
void dls_storage_rebuild(void);
