/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "endpoint_private.h"

/**
 * @defgroup services_blob_db_endpoint Endpoint
 * @ingroup services_blob_db
 * @brief Messages sent by the watch on the BlobDB sync endpoint.
 *
 * Messages go to the system session; when no session is connected they are dropped.
 * @{
 */

/**
 * @brief Send a WRITE message for a record.
 *
 * @param db_id Database.
 * @param last_updated Modification time of the record.
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param val Value data.
 * @param val_len Length of @p val in bytes.
 * @return Token of the message.
 */
BlobDBToken blob_db_endpoint_send_write(BlobDBId db_id, time_t last_updated, const void *key,
                                        int key_len, const void *val, int val_len);

/**
 * @brief Send a WRITEBACK message for a record.
 *
 * @param db_id Database.
 * @param last_updated Modification time of the record.
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param val Value data.
 * @param val_len Length of @p val in bytes.
 * @return Token of the message.
 */
BlobDBToken blob_db_endpoint_send_writeback(BlobDBId db_id, time_t last_updated, const void *key,
                                            int key_len, const void *val, int val_len);

/**
 * @brief Send a SYNC_DONE message for a database.
 *
 * @param db_id Database.
 */
void blob_db_endpoint_send_sync_done(BlobDBId db_id);

/** @} */
