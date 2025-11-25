/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/common/comm_session/session.h"
#include "services/common/bluetooth/bluetooth_persistent_storage.h"
#include "services/normal/blob_db/api.h"
#include "services/normal/blob_db/endpoint.h"
#include "services/normal/blob_db/sync.h"

#include <stdio.h>

// Fakes
////////////////////////////////////
#include "fake_system_task.h"

// Stubs
////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_app_cache.h"
#include "stubs_ios_notif_pref_db.h"
#include "stubs_app_db.h"
#include "stubs_contacts_db.h"
#include "stubs_notif_db.h"
#include "stubs_pin_db.h"
#include "stubs_prefs_db.h"
#include "stubs_reminder_db.h"
#include "stubs_watch_app_prefs_db.h"
#include "stubs_weather_db.h"
#include "stubs_bt_lock.h"
#include "stubs_evented_timer.h"
#include "stubs_events.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_reminders.h"
#include "stubs_regular_timer.h"


CommSession *comm_session_get_system_session(void) {
  return (CommSession *)1 ;
}

void blob_db_set_accepting_messages(bool ehh) {
}

static const uint8_t *s_expected_msg;
static bool did_sync_next = false;
static bool did_sync_cancel = false;
static bool did_sync_db = false;


static uint8_t sendbuffer[100];
static int sendbuffer_length;
static int sendbuffer_write_index;

extern void blob_db2_protocol_msg_callback(CommSession *session, const uint8_t* data, size_t length);

typedef void SendBuffer;

SendBuffer *comm_session_send_buffer_begin_write(CommSession *session, uint16_t endpoint_id,
                                                 size_t required_payload_length,
                                                 uint32_t timeout_ms) {
  cl_assert(required_payload_length < 100);
  sendbuffer_length = required_payload_length;
  sendbuffer_write_index = 0;
  return (void*)(uintptr_t)1;
}

bool comm_session_send_buffer_write(SendBuffer *sb, const uint8_t *data, size_t length) {
  memcpy(&sendbuffer[sendbuffer_write_index], data, length);
  sendbuffer_write_index += length;
  return true;
}

void comm_session_send_buffer_end_write(SendBuffer *sb) {
  cl_assert(sendbuffer_length > 0);
  cl_assert_equal_m(sendbuffer, s_expected_msg, sendbuffer_length);
}

bool comm_session_send_data(CommSession *session, uint16_t endpoint_id,
                            const uint8_t *data, size_t length, uint32_t timeout_ms) {
  cl_assert_equal_m(data, s_expected_msg, length);
  return true;
}

extern void prv_send_response(CommSession *session, uint8_t *response, uint8_t response_length) {
  cl_assert_equal_m(response, s_expected_msg, response_length);
}

static const BlobDBToken token = 0x22;
extern BlobDBToken prv_new_token(void) {
  return token;
}

void blob_db_sync_next(BlobDBSyncSession *session) {
  did_sync_next = true;
}

void blob_db_sync_cancel(BlobDBSyncSession *session) {
 did_sync_cancel = true;
}

status_t blob_db_sync_db(BlobDBId db_id) {
  did_sync_db = true;
  return S_SUCCESS;
}

BlobDBSyncSession *blob_db_sync_get_session_for_token(BlobDBToken token) {
  // Don't return NULL
  return (BlobDBSyncSession *)1;
}

extern void blob_db2_set_accepting_messages(bool ehh);
void test_blob_db2_endpoint__initialize(void) {
  blob_db2_set_accepting_messages(true);
  did_sync_next = false;
  did_sync_cancel = false;
  did_sync_db = false;
  sendbuffer_length = 0;
  sendbuffer_write_index = 0;
  memset(sendbuffer, 0, sizeof(sendbuffer));
}

void test_blob_db2_endpoint__cleanup(void) {
}


static const uint8_t s_dirty_dbs_request[] = {
  BLOB_DB_COMMAND_DIRTY_DBS,  // cmd
  0x12, 0x34,                 // token
};

static const uint8_t s_dirty_dbs_response[] = {
  BLOB_DB_COMMAND_DIRTY_DBS_RESPONSE,   // cmd
  0x12, 0x34,                           // token
  BLOB_DB_SUCCESS,                      // status
  0x01,                                 // num db_ids
  BlobDBIdiOSNotifPref                  // dirty db
};

void test_blob_db2_endpoint__handle_dirty_dbs_request(void) {
  s_expected_msg = s_dirty_dbs_response;
  blob_db2_protocol_msg_callback(NULL, s_dirty_dbs_request, sizeof(s_dirty_dbs_request));
}


static const uint8_t s_start_sync_request[] = {
  BLOB_DB_COMMAND_START_SYNC,  // cmd
  0x12, 0x34,                  // token
  BlobDBIdiOSNotifPref,        // db id
};

static const uint8_t s_start_sync_response[] = {
  BLOB_DB_COMMAND_START_SYNC_RESPONSE,  // cmd
  0x12, 0x34,                           // token
  BLOB_DB_SUCCESS,                      // status
};

void test_blob_db2_endpoint__handle_start_sync_request(void) {
  s_expected_msg = s_start_sync_response;
  blob_db2_protocol_msg_callback(NULL, s_start_sync_request, sizeof(s_start_sync_request));
  cl_assert(did_sync_db);
}


static const uint8_t s_start_write_response_success[] = {
  BLOB_DB_COMMAND_WRITE_RESPONSE,  // cmd
  0x12, 0x34,                      // token
  BLOB_DB_SUCCESS,                 // response
};

static const uint8_t s_start_write_response_error[] = {
  BLOB_DB_COMMAND_WRITE_RESPONSE,  // cmd
  0x56, 0x78,                      // token
  BLOB_DB_GENERAL_FAILURE,         // response
};

void test_blob_db2_endpoint__handle_write_response(void) {
  blob_db2_protocol_msg_callback(NULL, s_start_write_response_success,
                                 sizeof(s_start_write_response_success));
  cl_assert(did_sync_next);

  blob_db2_protocol_msg_callback(NULL, s_start_write_response_error,
                                 sizeof(s_start_write_response_error));
  cl_assert(did_sync_cancel);
}


static const uint8_t s_start_writeback_response_success[] = {
  BLOB_DB_COMMAND_WRITEBACK_RESPONSE,  // cmd
  0x12, 0x34,                          // token
  BLOB_DB_SUCCESS,                     // response
};

static const uint8_t s_start_writeback_response_error[] = {
  BLOB_DB_COMMAND_WRITEBACK_RESPONSE,  // cmd
  0x56, 0x78,                          // token
  BLOB_DB_GENERAL_FAILURE,             // response
};

void test_blob_db2_endpoint__handle_writeback_response(void) {
  blob_db2_protocol_msg_callback(NULL, s_start_writeback_response_success,
                                 sizeof(s_start_writeback_response_success));
  cl_assert(did_sync_next);

  blob_db2_protocol_msg_callback(NULL, s_start_writeback_response_error,
                                 sizeof(s_start_writeback_response_error));
  cl_assert(did_sync_cancel);
}

static const uint8_t s_sync_done_response[] = {
  BLOB_DB_COMMAND_SYNC_DONE_RESPONSE,  // cmd
  0x56, 0x78,                          // token
  BLOB_DB_SUCCESS,                     // response
};

void test_blob_db2_endpoint__handle_sync_done_response(void) {
  blob_db2_protocol_msg_callback(NULL, s_sync_done_response, sizeof(s_sync_done_response));
  // We currently don't do anything with this message
}

const uint8_t INVALID_CMD = 123;

static const uint8_t s_invalid_cmd[] = {
  INVALID_CMD,        // invalid cmd
  0x56, 0x78,         // token
  BLOB_DB_SUCCESS,    // response
};

static const uint8_t s_invalid_cmd_response[] = {
  INVALID_CMD | RESPONSE_MASK,    // cmd
  0x56, 0x78,                     // token
  BLOB_DB_INVALID_OPERATION,      // status
};

void test_blob_db2_endpoint__handle_unknown_cmd_id(void) {
  s_expected_msg = s_invalid_cmd_response;
  blob_db2_protocol_msg_callback(NULL, s_invalid_cmd, sizeof(s_invalid_cmd));
}

static const uint8_t s_sync_done_message[] = {
  BLOB_DB_COMMAND_SYNC_DONE,      // cmd
  0x22, 0x00,                     // token
  BlobDBIdiOSNotifPref,           // db id
};

void test_blob_db2_endpoint__send_sync_done(void) {
  s_expected_msg = s_sync_done_message;
  blob_db_endpoint_send_sync_done(BlobDBIdiOSNotifPref);
}

static const time_t last_updated = 1;
static const uint8_t key = 9;
static const uint8_t val = 2;

static const uint8_t s_writeback_message[] = {
  BLOB_DB_COMMAND_WRITEBACK,      // cmd
  0x22, 0x00,                     // token
  BlobDBIdiOSNotifPref,           // db id
  0x01, 0x00, 0x00, 0x00,         // last updated
  0x01,                           // key_len
  key,                            // key
  0x01, 0x00,                     // val_len
  val,                            // val
};

void test_blob_db2_endpoint__send_writeback(void) {
  s_expected_msg = s_writeback_message;
  blob_db_endpoint_send_writeback(BlobDBIdiOSNotifPref, last_updated, &key, 1, &val, 1);
}

static const uint8_t s_write_message[] = {
  BLOB_DB_COMMAND_WRITE,          // cmd
  0x22, 0x00,                     // token
  BlobDBIdiOSNotifPref,           // db id
  0x01, 0x00, 0x00, 0x00,         // last updated
  0x01,                           // key_len
  key,                            // key
  0x01, 0x00,                     // val_len
  val,                            // val
};

void test_blob_db2_endpoint__send_write(void) {
  s_expected_msg = s_write_message;
  blob_db_endpoint_send_write(BlobDBIdiOSNotifPref, last_updated, &key, 1, &val, 1);
}
