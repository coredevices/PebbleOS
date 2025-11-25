/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble_worker.h>
#include <inttypes.h>

#define BREAKPOINT __asm("bkpt")

#define ACCEL_BATCH_SIZE        10
#define PERSIST_WRITE_PERIOD_MS 1000

// -------------------------------------------------------------------------------------------------
static void prv_assert(bool condition, const char* msg) {
  if (!condition) {
    APP_LOG(APP_LOG_LEVEL_ERROR, msg);

    // Force an exception
    typedef void (*FuncPtr)(void);
    FuncPtr bad_func = NULL;
    bad_func();
  }
}

// -----------------------------------------------------------------------------------------------
void handle_accel(AccelRawData *accel_data, uint32_t num_samples, uint64_t timestamp) {

  // Display data
  //for (uint32_t i=0; i<num_samples; i++) {
  //  APP_LOG(APP_LOG_LEVEL_INFO, "Got accel data: %d, %d, %d", accel_data[i].x, accel_data[i].y, accel_data[i].z);
  //}

  // Publish new steps count
  AppWorkerMessage steps_data = {
    .data0 = accel_data[0].x,
    .data1 = accel_data[0].y,
    .data2 = accel_data[0].z,
  };
  app_worker_send_message(0 /*type*/, &steps_data);
}

// -----------------------------------------------------------------------------------------------
static void update_persist_callback(void* context) {
  int value = persist_read_int(42);
  // APP_LOG(APP_LOG_LEVEL_INFO, "Updating persist value from %d to %d", value, value + 1);
  persist_write_int(42, value + 1);
  app_timer_register(PERSIST_WRITE_PERIOD_MS /*ms*/, update_persist_callback, NULL);
}


// -----------------------------------------------------------------------------------------------
static void battery_state_handler(BatteryChargeState charge) {
  APP_LOG(APP_LOG_LEVEL_INFO, "got battery state service update");
  APP_LOG(APP_LOG_LEVEL_INFO, "percent: %d, is_charging: %d, is_plugged: %d", charge.charge_percent,
        charge.is_charging, charge.is_plugged);

  AppWorkerMessage battery_data = {
    .data0 = charge.charge_percent,
    .data1 = charge.is_charging,
    .data2 = charge.is_plugged,
  };
  app_worker_send_message(1 /*type*/, &battery_data);
}


// -----------------------------------------------------------------------------------------------
static void connection_handler(bool connected) {
  APP_LOG(APP_LOG_LEVEL_INFO, "got phone connection update");
  APP_LOG(APP_LOG_LEVEL_INFO, "connected: %d", connected);
}


// -----------------------------------------------------------------------------------------------
static void tick_timer_handler(struct tm *tick_time, TimeUnits units_changed) {
  APP_LOG(APP_LOG_LEVEL_INFO, "got tick timer update");
}


// -----------------------------------------------------------------------------------------------
static void worker_message_handler(uint16_t type, AppWorkerMessage *data) {
  if (type == 'x') {
    prv_assert(0, "crashing");
  }
}


// -----------------------------------------------------------------------------------------------
static void health_event_handler(HealthEventType event, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "worker: Got health event update. event_id: %"PRIu32"",
          (uint32_t) event);
  if (event == HealthEventMovementUpdate) {
    HealthValue steps = health_service_sum_today(HealthMetricStepCount);
    APP_LOG(APP_LOG_LEVEL_INFO, "worker: movement event, steps: %"PRIu32"",
            (uint32_t)steps);

  } else if (event == HealthEventSleepUpdate) {
    HealthValue total_sleep = health_service_sum_today(HealthMetricSleepSeconds);
    HealthValue restful_sleep = health_service_sum_today(HealthMetricSleepRestfulSeconds);
    APP_LOG(APP_LOG_LEVEL_INFO, "worker: New sleep event: total: %"PRIu32", restful: %"PRIu32" ",
            total_sleep / SECONDS_PER_MINUTE,  restful_sleep / SECONDS_PER_MINUTE);
  }
}

// -----------------------------------------------------------------------------------------------
int main(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "initializing...");

  accel_raw_data_service_subscribe(ACCEL_BATCH_SIZE, handle_accel);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);

  app_timer_register(PERSIST_WRITE_PERIOD_MS /*ms*/, update_persist_callback, NULL);

  battery_state_service_subscribe(battery_state_handler);

  ConnectionHandlers conn_handlers = {
    .pebble_app_connection_handler = connection_handler
  };
  connection_service_subscribe(conn_handlers);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_timer_handler);

  app_worker_message_subscribe(worker_message_handler);

  // Subscribe to health service
  // health_service_events_subscribe(health_event_handler, NULL);

  worker_event_loop();

  accel_data_service_unsubscribe();
  health_service_events_unsubscribe();
}

