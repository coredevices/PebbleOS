/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/common/comm_session/session.h"
#include "services/common/bluetooth/bluetooth_persistent_storage.h"
#include "services/normal/blob_db/api.h"
#include "services/normal/blob_db/endpoint.h"
#include "util/attributes.h"

#include <stdio.h>

// Fakes
////////////////////////////////////
#include "fake_system_task.h"
#include "fake_session.h"

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

void bt_persistent_storage_set_unfaithful(bool is_unfaithful) {
  return;
}

void blob_db2_set_accepting_messages(bool ehh) {
}

typedef struct PACKED {
  uint16_t length;
  uint16_t endpoint_id;
} PebbleProtocolHeader;

extern void blob_db_protocol_msg_callback(CommSession *session, const uint8_t* data, size_t length);

static const uint16_t BLOB_DB_ENDPOINT_ID = 0xb1db;

// used to test the filling of the system_task queue
static uint32_t system_task_queue_size = 7;

// used to examine sending buffer for valid contents
static uint8_t s_data[1024];
static uint16_t s_sending_endpoint_id = 0;
static uint32_t s_sending_data_length = 0;
static CommSession *s_session;

static const uint8_t TEST_KEY_SIZE = 16;
static const uint16_t TEST_VALUE_SIZE = 320;
static const uint8_t TEST_DB_ID = 0x01;

/* Helper functions */

static void prv_sent_data_cb(uint16_t endpoint_id,
                             const uint8_t* data, unsigned int data_length) {
  s_sending_endpoint_id = endpoint_id;
  s_sending_data_length = data_length;
  memcpy(s_data, data, data_length);
}

static uint8_t *process_blob_db_command(const uint8_t *command, uint32_t length) {
  s_sending_endpoint_id = 0;
  s_sending_data_length = 0;

  blob_db_protocol_msg_callback(s_session, command, length);
  fake_system_task_callbacks_invoke_pending();
  fake_comm_session_process_send_next();
  cl_assert(BLOB_DB_ENDPOINT_ID == s_sending_endpoint_id);
  return s_data;
}

/* Start of test */

extern void blob_db_set_accepting_messages(bool enabled);
void test_blob_db_endpoint__initialize(void) {
  blob_db_set_accepting_messages(true);
  fake_comm_session_init();
  Transport *transport = fake_transport_create(TransportDestinationSystem, NULL, prv_sent_data_cb);
  s_session = fake_transport_set_connected(transport, true /* connected */);
  system_task_set_available_space(system_task_queue_size);
}

void test_blob_db_endpoint__cleanup(void) {
  fake_comm_session_cleanup();
}

/*************************************
 * Checking for valid INSERT command *
 *************************************/

static const uint8_t s_insert_cmd_success[] = {
  // Message Header
  0x01,                     // Pebble protocol message ID: INSERT
  0x17, 0x00,               // Token
  TEST_DB_ID,                     // BlobDBDatabaseId: 0x01
  0x10,                     // Size primary key: sizeof(Uuid) = 16 = 0x10

  // Primary Key: UUID:16
  0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,

  0x40, 0x01,               // Size value

  // value (made up for now)
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,

};

void test_blob_db_endpoint__handle_insert_command_success(void) {
  uint8_t *cmd_ptr = (uint8_t*)s_insert_cmd_success;

  // check for INSERT
  cl_assert(BLOB_DB_COMMAND_INSERT == *(uint8_t*)cmd_ptr);
  cmd_ptr += sizeof(uint8_t);

  // check for token
  uint16_t token = *(uint16_t*)cmd_ptr;
  cl_assert(0 < token);
  cmd_ptr += sizeof(uint16_t);

  // check for a Database entry
  cl_assert(TEST_DB_ID == *(uint8_t*)cmd_ptr);
  cmd_ptr += sizeof(uint8_t);

  // check for a key size
  cl_assert(TEST_KEY_SIZE == *(uint8_t*)cmd_ptr);
  cmd_ptr += sizeof(uint8_t);

  // go past key_bytes
  cmd_ptr += TEST_KEY_SIZE;

  // size value
  cl_assert(TEST_VALUE_SIZE == *(uint16_t*)cmd_ptr);
  cmd_ptr += sizeof(uint16_t);

  // go past value_bytes
  cmd_ptr += TEST_VALUE_SIZE;

  cl_assert((cmd_ptr - s_insert_cmd_success) == sizeof(s_insert_cmd_success));

  // Process Command
  uint8_t *resp_ptr = process_blob_db_command(s_insert_cmd_success, sizeof(s_insert_cmd_success));

  // Check Response
  cl_assert(token == *(uint16_t*)resp_ptr);
  resp_ptr += sizeof(uint16_t);

  cl_assert(BLOB_DB_SUCCESS == *resp_ptr);
  resp_ptr += sizeof(uint8_t);

  cl_assert((resp_ptr - s_data) == s_sending_data_length);
}

/**************************************************
 * Checking for key size zero INSERT command *
 **************************************************/

static const uint8_t s_insert_cmd_zero_key_size[] = {
  // Message Header
  0x01,                     // Pebble protocol message ID: INSERT
  0x17, 0x00,               // Token
  TEST_DB_ID,                     // BlobDBDatabaseId: 0x01
  0x00,                     // Size primary key: sizeof(Uuid) = 16 = 0x10

  // garbage data to put message above minimum
  0x01, 0x02, 0x03, 0x04, 0x05,

  // no value size
};

void test_blob_db_endpoint__handle_insert_command_zero_key_size(void) {

  // Ensure key length is 0
  cl_assert(s_insert_cmd_zero_key_size[2] == 0);

  // Process Command
  uint8_t *resp_ptr = process_blob_db_command(s_insert_cmd_zero_key_size, sizeof(s_insert_cmd_zero_key_size));

  // Check Response
  cl_assert(0 < *(uint16_t*)resp_ptr);
  resp_ptr += sizeof(uint16_t);

  cl_assert(BLOB_DB_INVALID_DATA == *resp_ptr);
  resp_ptr += sizeof(uint8_t);

  cl_assert((resp_ptr - s_data) == s_sending_data_length);
}

/**************************************************
 * Checking for value size of zero INSERT command *
 * This is valid because one can ZERO out a value *
 **************************************************/

static const uint8_t s_insert_cmd_zero_value_size[] = {
  // Message Header
  0x01,                     // Pebble protocol message ID: INSERT
  0x17, 0x00,               // Token
  TEST_DB_ID,                     // BlobDBDatabaseId: 0x01
  0x10,                     // Size primary key: sizeof(Uuid) = 16 = 0x10

  // Primary Key: UUID:16
  0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,

  0x00, 0x00,               // Size value
};

void test_blob_db_endpoint__handle_insert_command_zero_value_size(void) {

  // Process Command
  uint8_t *resp_ptr = process_blob_db_command(s_insert_cmd_zero_value_size, sizeof(s_insert_cmd_zero_value_size));

  // Check Response
  cl_assert(0 < *(uint16_t*)resp_ptr);
  resp_ptr += sizeof(uint16_t);

  cl_assert(BLOB_DB_INVALID_DATA == *resp_ptr);
  resp_ptr += sizeof(uint8_t);

  cl_assert((resp_ptr - s_data) == s_sending_data_length);
}

/**************************************************
 * Checking for below minimum size INSERT command *
 **************************************************/

static const uint8_t s_insert_cmd_no_value_size[] = {
  // Message Header
  0x01,                     // Pebble protocol message ID: INSERT
  0x17, 0x00,               // Token
  TEST_DB_ID,                     // BlobDBDatabaseId: 0x01
  0x01,                     // Size primary key: 1 byte

  // Primary Key: byte
  0x6b

  // no value size
};

void test_blob_db_endpoint__handle_insert_command_no_value_size(void) {

  // Ensure packet is too small
  cl_assert(sizeof(s_insert_cmd_no_value_size) < 8);

  // Process Command
  uint8_t *resp_ptr = process_blob_db_command(s_insert_cmd_no_value_size, sizeof(s_insert_cmd_no_value_size));

  // Check Response
  cl_assert(0 < *(uint16_t*)resp_ptr);
  resp_ptr += sizeof(uint16_t);

  cl_assert(BLOB_DB_INVALID_DATA == *resp_ptr);
  resp_ptr += sizeof(uint8_t);

  cl_assert((resp_ptr - s_data) == s_sending_data_length);
}

/**************************************************
 * Checking for length data mismatch INSERT command *
 **************************************************/

static const uint8_t s_insert_cmd_size_value_wrong[] = {
  // Message Header
  0x01,                     // Pebble protocol message ID: INSERT
  0x17, 0x00,               // Token
  TEST_DB_ID,                     // BlobDBDatabaseId: 0x01
  0x10,                     // Size primary key: sizeof(Uuid) = 16 = 0x10

  // Primary Key: UUID:16
  0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,

  0x21, 0x00,               // Size value is 1 more than it should be

  // value (made up for now)
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
};

void test_blob_db_endpoint__handle_insert_command_length_data_mismatch(void) {

  // Process Command
  uint8_t *resp_ptr = process_blob_db_command(s_insert_cmd_size_value_wrong, sizeof(s_insert_cmd_size_value_wrong));

  // Check Response
  cl_assert(0 < *(uint16_t*)resp_ptr);
  resp_ptr += sizeof(uint16_t);

  cl_assert(BLOB_DB_INVALID_DATA == *resp_ptr);
  resp_ptr += sizeof(uint8_t);

  cl_assert((resp_ptr - s_data) == s_sending_data_length);
}

/**************************************************
 * Checking for smallest possible valid INSERT command *
 **************************************************/

static const uint8_t s_insert_cmd_smallest_length[] = {
  // Message Header
  0x01,                     // Pebble protocol message ID: INSERT
  0x17, 0x00,               // Token
  TEST_DB_ID,                     // BlobDBDatabaseId: 0x01
  0x01,                     // Size primary key: 1 byte

  // Primary Key: 1 byte
  0x00,

  0x01, 0x00,               // Size value is 1 more than it should be

  // value (made up for now)
  0x00
};

void test_blob_db_endpoint__handle_insert_command_smallest_length(void) {

  // Process Command
  uint8_t *resp_ptr = process_blob_db_command(s_insert_cmd_smallest_length, sizeof(s_insert_cmd_smallest_length));

  // Check Response
  cl_assert(0 < *(uint16_t*)resp_ptr);
  resp_ptr += sizeof(uint16_t);

  cl_assert(BLOB_DB_SUCCESS == *resp_ptr);
  resp_ptr += sizeof(uint8_t);

  cl_assert((resp_ptr - s_data) == s_sending_data_length);
}

/*************************************
 * Checking for valid DELETE command *
 *************************************/

static const uint8_t s_delete_cmd_success[] = {
  // Message Header
  0x04,                     // Pebble protocol message ID: DELETE
  0x17, 0x00,               // Token
  TEST_DB_ID,                     // BlobDBDatabaseId: 0x01
  0x10,                     // Size primary key: sizeof(Uuid) = 16 = 0x10

  // Primary Key: UUID:16
  0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
  0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
};

void test_blob_db_endpoint__handle_delete_command_success(void) {
  uint8_t *cmd_ptr = (uint8_t*)s_delete_cmd_success;

  // check for DELETE
  cl_assert(BLOB_DB_COMMAND_DELETE == *(uint8_t*)cmd_ptr);
  cmd_ptr += sizeof(uint8_t);

  // check for token
  uint16_t token = *(uint16_t*)cmd_ptr;
  cl_assert(0 < token);
  cmd_ptr += sizeof(uint16_t);

  // check for a Database entry
  cl_assert(TEST_DB_ID == *(uint8_t*)cmd_ptr);
  cmd_ptr += sizeof(uint8_t);

  // check for a key size
  cl_assert(TEST_KEY_SIZE == *(uint8_t*)cmd_ptr);
  cmd_ptr += sizeof(uint8_t);

  // go past key_bytes
  cmd_ptr += TEST_KEY_SIZE;

  cl_assert((cmd_ptr - s_delete_cmd_success) == sizeof(s_delete_cmd_success)) ;

  // Process Command
  uint8_t *resp_ptr = process_blob_db_command(s_delete_cmd_success, sizeof(s_delete_cmd_success));

  // Check Response
  cl_assert(token == *(uint16_t*)resp_ptr);
  resp_ptr += sizeof(uint16_t);

  cl_assert(BLOB_DB_SUCCESS == *resp_ptr);
  resp_ptr += sizeof(uint8_t);

  cl_assert((resp_ptr - s_data) == s_sending_data_length);
}

/*************************************
 * Checking for valid CLEAR command *
 *************************************/

static const uint8_t s_clear_cmd_success[] = {
  // Message Header
  0x05,                     // Pebble protocol message ID: DELETE
  0x17, 0x00,               // Token
  TEST_DB_ID,                     // BlobDBDatabaseId: 0x01
};

void test_blob_db_endpoint__handle_clear_command_success(void) {
  uint8_t *cmd_ptr = (uint8_t*)s_clear_cmd_success;

  // check for CLEAR
  cl_assert(BLOB_DB_COMMAND_CLEAR == *(uint8_t*)cmd_ptr);
  cmd_ptr += sizeof(uint8_t);

  // check for token
  uint16_t token = *(uint16_t*)cmd_ptr;
  cl_assert(0 < token);
  cmd_ptr += sizeof(uint16_t);

  // check for a Database entry
  cl_assert(TEST_DB_ID == *(uint8_t*)cmd_ptr);
  cmd_ptr += sizeof(uint8_t);

  cl_assert((cmd_ptr - s_clear_cmd_success) == sizeof(s_clear_cmd_success)) ;

  // Process Command
  uint8_t *resp_ptr = process_blob_db_command(s_clear_cmd_success, sizeof(s_clear_cmd_success));

  // Check Response
  cl_assert_equal_i(token, *(uint16_t*)resp_ptr);
  resp_ptr += sizeof(uint16_t);

  cl_assert(BLOB_DB_SUCCESS == *resp_ptr);
  resp_ptr += sizeof(uint8_t);

  cl_assert_equal_i((resp_ptr - s_data), s_sending_data_length);
}

/*************************************
 * Checking for invalid operation *
 *************************************/

static const uint8_t s_invalid_operation_cmd[] = {
  // Message Header
  0x42,                     // Pebble protocol message ID: This does not exist
  0x17, 0x00,               // Token
  TEST_DB_ID,                     // BlobDBDatabaseId: 0x01
};

void test_blob_db_endpoint__handle_invalid_operation_command(void) {
  uint8_t *cmd_ptr = (uint8_t*)s_invalid_operation_cmd;

  cmd_ptr += sizeof(uint8_t);

  // check for token
  uint16_t token = *(uint16_t*)cmd_ptr;
  cl_assert(0 < token);
  cmd_ptr += sizeof(uint16_t);

  // check for a Database entry
  cl_assert(TEST_DB_ID == *(uint8_t*)cmd_ptr);
  cmd_ptr += sizeof(uint8_t);

  cl_assert((cmd_ptr - s_invalid_operation_cmd) == sizeof(s_invalid_operation_cmd)) ;

  // Process Command
  uint8_t *resp_ptr = process_blob_db_command(s_invalid_operation_cmd, sizeof(s_invalid_operation_cmd));

  // Check Response
  cl_assert(token == *(uint16_t*)resp_ptr);
  resp_ptr += sizeof(uint16_t);

  cl_assert(BLOB_DB_INVALID_OPERATION == *resp_ptr);
  resp_ptr += sizeof(uint8_t);

  cl_assert((resp_ptr - s_data) == s_sending_data_length);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// BLOBDB SYNC TESTS
///////////////////////////////////////////////////////////////////////////////////////////////////

static const uint8_t *s_expected_data;

extern void prv_send_v2_response(CommSession *session, uint8_t *response, uint8_t response_length) {
  cl_assert_equal_m(response, s_expected_data, response_length);
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

void test_blob_db_endpoint__handle_dirty_dbs_request(void) {
  s_expected_data = s_dirty_dbs_response;
  blob_db_protocol_msg_callback(NULL, s_dirty_dbs_request, sizeof(s_dirty_dbs_request));
}

static const uint8_t s_start_sync_request[] = {
  BLOB_DB_COMMAND_START_SYNC,  // cmd
  0x12, 0x34,                  // token
};

static const uint8_t s_start_sync_response[] = {
  BLOB_DB_COMMAND_START_SYNC_RESPONSE,  // cmd
  0x12, 0x34,                           // token
  BLOB_DB_SUCCESS,                      // status
};

void test_blob_db_endpoint__handle_start_sync_request(void) {
  s_expected_data = s_start_sync_response;
  blob_db_protocol_msg_callback(NULL, s_start_sync_request, sizeof(s_start_sync_request));
}

