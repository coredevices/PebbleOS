/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/uuid.h"
#include "services/common/analytics/analytics.h"
#include "services/common/analytics/analytics_heartbeat.h"
#include "services/common/analytics/analytics_storage.h"
#include "services/common/analytics/analytics_logging.h"

#include "services/common/comm_session/session_transport.h"

#include "services/normal/data_logging/data_logging_service.h"

#include "clar.h"
#include "stubs_bt_lock.h"
#include "stubs_analytics_external.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_rand_ptr.h"
#include "stubs_tick.h"
#include "stubs_worker_manager.h"

#include "fake_app_manager.h"
#include "fake_pbl_malloc.h"
#include "fake_new_timer.h"
#include "fake_rtc.h"
#include "fake_system_task.h"
#include "fake_time.h"

void dls_clear() {}
bool dls_initialized(void) {return true;}

const PebbleProcessMd* launcher_menu_app_get_app_info(void) {
  return NULL;
}

GAPLEConnection *gap_le_connection_get_gateway(void) {
  return NULL;
}

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

bool comm_session_is_valid(const CommSession *session) {
  return true;
}

CommSessionTransportType comm_session_analytics_get_transport_type(CommSession *session) {
  return CommSessionTransportType_PPoGATT;
}

void sys_analytics_logging_log_event(AnalyticsEventBlob *event_blob) {
}

void test_analytics__initialize(void) {
  analytics_init();
  fake_rtc_init(0, 0);
}

void test_analytics__cleanup(void) {

}

// Make sure the stopwatches record time elapsed in ms (not ticks)
void test_analytics__stopwatches_should_record_ms(void) {
  const AnalyticsMetric metric = ANALYTICS_DEVICE_METRIC_BATTERY_CHARGE_TIME;
  analytics_stopwatch_start(metric, AnalyticsClient_System);
  fake_rtc_increment_ticks(1024);

  analytics_storage_take_lock();
  extern void analytics_stopwatches_update(uint64_t current_ticks);
  analytics_stopwatches_update(rtc_get_ticks());

  AnalyticsHeartbeat *heartbeat = analytics_storage_find(metric, NULL, AnalyticsClient_System);
  int64_t value = analytics_heartbeat_get(heartbeat, metric);
  analytics_storage_give_lock();

  printf("Battery change period: %"PRId64"\n", value);
  // 1024 ticks === 1000 ms
  cl_assert(value == 1000);
}


static bool dls_log_called = false;
static int64_t expected_value = 254307546;
void test_analytics__minimal_logging_test(void) {
  analytics_set(ANALYTICS_DEVICE_METRIC_BATTERY_CHARGE_TIME, expected_value,
                AnalyticsClient_System);

  // After calling analytics_logging_system_task_cb, the analytics code should call into
  // dls_log with the device heartbeat data, where we check that the value
  // therein is stored correctly.
  analytics_logging_system_task_cb(NULL);
  cl_assert(dls_log_called);
}

DataLoggingResult dls_log(DataLoggingSession *logging_session, const void *data,
                          uint32_t num_items) {
  // Make sure we aren't called more than once.
  cl_assert(dls_log_called == false);

  // We only set a device metric, getting app heartbeat data is a bug.
  uint8_t got_type = *((uint8_t *)data);
  printf("Type: %"PRIu8"\n", got_type);
  cl_assert(got_type == ANALYTICS_HEARTBEAT_KIND_DEVICE);

  uint16_t got_version = *((uint16_t *)(data + 1));
  printf("Version: %"PRIu16"\n", got_version);
  cl_assert(got_version == ANALYTICS_DEVICE_HEARTBEAT_BLOB_VERSION);

  uint32_t got_value = *((uint32_t *)(data + 28));
  printf("Battery change period: %"PRIu32"\n", got_value);
  cl_assert(got_value == expected_value);

  dls_log_called = true;
  return DATA_LOGGING_SUCCESS;
}

DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  // We want a truthy value where dereferencing it *should* crash.
  return (DataLoggingSession*)0x1;
}
