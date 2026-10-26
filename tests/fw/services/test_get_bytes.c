/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/conn_event_stats.h>
#include <pbl/services/get_bytes/get_bytes.h>
#include <pbl/services/get_bytes/get_bytes_private.h>
#include <pbl/services/get_bytes/get_bytes_storage.h>

#include <clar.h>
#include <fake_events.h>
#include <fake_pbl_malloc.h>
#include <fake_rtc.h>
#include <fake_session.h>
#include <fake_system_task.h>

#define OBJECT_SIZE 4096

extern void get_bytes_protocol_msg_callback(CommSession *session, const uint8_t *msg_data,
                                            uint32_t msg_len);

// Fakes
//////////////////////////////////////////////////////////

static int s_setup_count;
static int s_cleanup_count;
static bool s_last_cleanup_successful;
static int s_sent_count;
static GetBytesRspObjectInfo s_last_info;

bool gb_storage_setup(GetBytesStorage *storage, GetBytesObjectType object_type,
                      GetBytesStorageInfo *info) {
  *storage = (GetBytesStorage){};
  s_setup_count++;
  return true;
}

GetBytesInfoErrorCode gb_storage_get_size(GetBytesStorage *storage, uint32_t *size) {
  *size = OBJECT_SIZE;
  return GET_BYTES_OK;
}

bool gb_storage_read_next_chunk(GetBytesStorage *storage, uint8_t *buffer, uint32_t len) {
  memset(buffer, 0xA5, len);
  storage->current_offset += len;
  return true;
}

void gb_storage_cleanup(GetBytesStorage *storage, bool successful) {
  s_cleanup_count++;
  s_last_cleanup_successful = successful;
}

void bluetooth_analytics_handle_get_bytes_stats(
    GetBytesObjectType type, uint32_t total_size, uint32_t elapsed_time_ms,
    const struct pbl_bt_slave_conn_event_stats *orig_stats) {
}

static void prv_sent_cb(uint16_t endpoint_id, const uint8_t *data, unsigned int data_length) {
  cl_assert_equal_i(endpoint_id, GET_BYTES_ENDPOINT_ID);
  if (data[0] == GET_BYTES_CMD_OBJECT_INFO) {
    cl_assert_equal_i(data_length, sizeof(s_last_info));
    memcpy(&s_last_info, data, sizeof(s_last_info));
  }
  s_sent_count++;
}

// Helpers
//////////////////////////////////////////////////////////

static Transport *s_transport;
static CommSession *s_session;

static void prv_request_coredump(CommSession *session, uint8_t transaction_id) {
  const GetBytesHeader hdr = {
    .cmd_id = GET_BYTES_CMD_GET_COREDUMP,
    .transaction_id = transaction_id,
  };
  get_bytes_protocol_msg_callback(session, (const uint8_t *)&hdr, sizeof(hdr));
}

// The fake send buffer never blocks, so drain it between chunk callbacks
static void prv_pump_transfer(void) {
  while (fake_system_task_count_callbacks()) {
    comm_session_send_next(s_session);
    fake_system_task_callbacks_invoke(fake_system_task_count_callbacks());
  }
}

// Tests
//////////////////////////////////////////////////////////

void test_get_bytes__initialize(void) {
  s_setup_count = 0;
  s_cleanup_count = 0;
  s_last_cleanup_successful = false;
  s_sent_count = 0;
  s_last_info = (GetBytesRspObjectInfo){};

  // Elapsed transfer time is a divisor in the stats
  fake_rtc_init(0, 0);
  fake_rtc_auto_increment_ticks(100);

  fake_comm_session_init();
  fake_event_reset_count();

  s_transport = fake_transport_create(TransportDestinationSystem, nullptr, prv_sent_cb);
  s_session = fake_transport_set_connected(s_transport, true /* connected */);
}

void test_get_bytes__cleanup(void) {
  fake_comm_session_cleanup();
  fake_system_task_callbacks_cleanup();
  fake_event_clear_last();

  fake_pbl_malloc_check_net_allocs();
  fake_pbl_malloc_clear_tracking();
}

void test_get_bytes__transfer_completes(void) {
  prv_request_coredump(s_session, 1);

  prv_pump_transfer();

  cl_assert_equal_i(s_last_info.error_code, GET_BYTES_OK);
  cl_assert(s_sent_count > 1);
  cl_assert_equal_i(s_cleanup_count, 1);
  cl_assert(s_last_cleanup_successful);
}

void test_get_bytes__session_closed_mid_transfer_aborts(void) {
  prv_request_coredump(s_session, 1);
  cl_assert_equal_i(fake_system_task_count_callbacks(), 1);

  fake_transport_set_connected(s_transport, false /* connected */);
  fake_system_task_callbacks_invoke(1);

  cl_assert_equal_i(fake_system_task_count_callbacks(), 0);
  cl_assert_equal_i(s_cleanup_count, 1);
  cl_assert(!s_last_cleanup_successful);
  cl_assert_equal_i(fake_event_get_last().debug_info.state, DebugInfoStateFinished);

  // A request on a new session must not be rejected as already in progress
  s_session = fake_transport_set_connected(s_transport, true /* connected */);
  prv_request_coredump(s_session, 2);
  cl_assert_equal_i(s_setup_count, 2);

  prv_pump_transfer();
  cl_assert_equal_i(s_cleanup_count, 2);
  cl_assert(s_last_cleanup_successful);
}
