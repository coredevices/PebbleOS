/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "endpoint_private.h"

#include <stdbool.h>

extern void blob_db_set_accepting_messages(bool enabled);
extern void blob_db2_set_accepting_messages(bool enabled);

void blob_db_enabled(bool enabled) {
  blob_db_set_accepting_messages(enabled);
  blob_db2_set_accepting_messages(enabled);
}

const uint8_t *endpoint_private_read_token_db_id(const uint8_t *iter, BlobDBToken *out_token,
                                                 BlobDBId *out_db_id) {
  // read token
  *out_token = *(BlobDBToken*)iter;
  iter += sizeof(BlobDBToken);
  // read database id
  *out_db_id = *iter;
  iter += sizeof(BlobDBId);

  return iter;
}
