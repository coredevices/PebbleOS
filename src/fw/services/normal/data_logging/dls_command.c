/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "data_logging_service.h"
#include "dls_list.h"

#include "console/prompt.h"

#include <inttypes.h>

static bool command_dls_list_cb(DataLoggingSession *session, void *data) {
  char buffer[80];
  prompt_send_response_fmt(buffer, sizeof(buffer),
      "session_id : %"PRIu8", tag: %"PRIu32", bytes: %"PRIu32", write_offset: %"PRIu32,
      session->comm.session_id, session->tag, session->storage.num_bytes,
      session->storage.write_offset);

  return true;
}

void command_dls_list(void) {
  dls_list_for_each_session(command_dls_list_cb, 0);
}

// Unused, comment out to avoid pulling in the string literals
#if 0
void command_dls_show(const char *id) {
  uint8_t session_id = strtol(id, NULL, 0);
  DataLoggingSession *logging_session = dls_list_find_by_session_id(session_id);

  if (logging_session == NULL) {
    prompt_send_response("LoggingSession not found");
    return;
  }

  char uuid_b[UUID_STRING_BUFFER_LENGTH];
  uuid_to_string(&logging_session->app_uuid, uuid_b);

  char buffer[80];

#define WRITE_LINE(fmt, arg) \
  prompt_send_response_fmt(buffer, sizeof(buffer), fmt, arg)

  WRITE_LINE("session_id %u", logging_session->comm.session_id);
  WRITE_LINE("uuid %s", uuid_b);
  WRITE_LINE("data state %u", logging_session->status);
  WRITE_LINE("tag %lu", logging_session->tag);
  WRITE_LINE("item type %u", logging_session->item_type);
  WRITE_LINE("item size %u", logging_session->item_size);
  WRITE_LINE("storage %lu bytes", logging_session->storage.num_bytes);
  WRITE_LINE("  r sector %u", logging_session->storage.read_sector);
  WRITE_LINE("  r offset %"PRIu32, logging_session->storage.read_offset);
  WRITE_LINE("  w sector %u", logging_session->storage.write_sector);
  WRITE_LINE("  w offset %"PRIu32, logging_session->storage.write_offset);

#undef WRITE_LINE
}
#endif

void command_dls_erase_all(void) {
  dls_clear();
}

void command_dls_send_all(void) {
  // Use this to trigger a send of all data logging data to the phone, helpful for testing
  dls_send_all_sessions();
}
