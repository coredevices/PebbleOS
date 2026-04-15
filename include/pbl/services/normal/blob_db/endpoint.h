/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "endpoint_private.h"

//! Send a write message for the given blob db item.
//! @returns the blob db transaction token
BlobDBToken blob_db_endpoint_send_write(BlobDBId db_id,
                                        time_t last_updated,
                                        const void *key,
                                        int key_len,
                                        const void *val,
                                        int val_len);

//! Send a WB message for the given blob db item.
//! @returns the blob db transaction token
BlobDBToken blob_db_endpoint_send_writeback(BlobDBId db_id,
                                            time_t last_updated,
                                            const void *key,
                                            int key_len,
                                            const void *val,
                                            int val_len);

//! Indicate that blob db sync is done for a given db id
void blob_db_endpoint_send_sync_done(BlobDBId db_id);
