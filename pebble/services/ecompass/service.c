/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "calibration.h"

#include <math.h>

#include <pbl/drivers/sensor.h>
#include <pbl/kernel/syscall.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/sensing/sensing.h>
#include <pbl/services/ecompass.h>
#include <pbl/services/event_service.h>
#include <pbl/settings/settings.h>
#include <pbl/util/trig.h>

#include <applib/compass_service.h>
#include <syscall/syscall.h>

PBL_LOG_MODULE_DEFINE(service_ecompass, CONFIG_SERVICE_ECOMPASS_LOG_LEVEL);

// Faster magnetometer sampling while uncalibrated, for at most this long per subscription
#ifdef CONFIG_BOARD_ASTERIX
#define FAST_MINUTES 6
#else
#define FAST_MINUTES 2
#endif

#define MAG_INTERVAL_US      200000U
#define MAG_FAST_INTERVAL_US 50000U

// Heading events stay at the slow rate while sampling fast
#define FAST_EVENT_DIVIDER (MAG_INTERVAL_US / MAG_FAST_INTERVAL_US)

// Gravity: accelerometer at 25 Hz, low-pass filtered with a ~0.4 s time constant
#define ACCEL_INTERVAL_US 40000U
#define ACCEL_BATCH       5U
#define ACCEL_ALPHA       0.1f

// Fit error thresholds for the reported status
#define CALIBRATED_ERROR  0.035f
#define CALIBRATING_ERROR 0.10f

// A sample this far off the field is disturbed: hold the status at calibrating
#define DISTURBED_DEVIATION 0.25f

// Below this horizontal fraction, the top of the watch points up or down: use its back
#define MIN_HORIZONTAL 0.14f

// Least time between two saves of an improving calibration
#define SAVE_INTERVAL_S 600U

#define SETTINGS_FILE     "ecompass"
#define SETTINGS_MAX_SIZE 256
#define SETTINGS_VERSION  1U

struct prv_saved {
  uint8_t version;
  struct ecompass_cal cal;
};

static const char s_key[] = "cal";

static struct ecompass_calibrator s_cal;
static bool s_loaded;
static bool s_dirty;
static uint32_t s_last_save_s;

static CompassStatus s_status = CompassStatusDataInvalid;
static int32_t s_last_heading = -1;

static bool s_charger_plugged;
static bool s_fast;
static bool s_fast_done;
static uint32_t s_events;
static struct pbl_wallclock_work s_fast_timer;

static uint8_t s_subscribers;
static bool s_subscribed[NumPebbleTask];

static struct pbl_sensing_client s_mag_client;
static struct pbl_sensing_client s_accel_client;

// Latest samples, under the device lock of their sensor
static int32_t s_mag_nt[3];
static float s_gravity[3];
static bool s_gravity_valid;

/*
 * Persistence
 */

static void prv_load(void) {
  struct pbl_settings_file file;
  struct prv_saved saved;

  s_loaded = true;

  if (pbl_settings_open(&file, SETTINGS_FILE, SETTINGS_MAX_SIZE) != 0) {
    ecompass_cal_init(&s_cal, nullptr);
    return;
  }

  if (pbl_settings_get(&file, s_key, sizeof(s_key), &saved, sizeof(saved)) == 0 &&
      saved.version == SETTINGS_VERSION) {
    PBL_LOG_DBG("Loaded a %d-parameter calibration", (int)saved.cal.order);
    ecompass_cal_init(&s_cal, &saved.cal);
  } else {
    ecompass_cal_init(&s_cal, nullptr);
  }

  pbl_settings_close(&file);
}

static void prv_save(void) {
  struct pbl_settings_file file;
  const struct prv_saved saved = {
    .version = SETTINGS_VERSION,
    .cal = s_cal.cal,
  };

  s_dirty = false;
  s_last_save_s = (uint32_t)(pbl_uptime_ms() / 1000U);

  if (pbl_settings_open(&file, SETTINGS_FILE, SETTINGS_MAX_SIZE) != 0) {
    return;
  }

  if (pbl_settings_set(&file, s_key, sizeof(s_key), &saved, sizeof(saved)) != 0) {
    PBL_LOG_WRN("Could not save the calibration");
  }

  pbl_settings_close(&file);
}

/*
 * Sampling rate
 */

static void prv_set_fast(bool fast) {
  if (fast == s_fast) {
    return;
  }

  s_fast = fast;
  pbl_sensing_client_set_interval(&s_mag_client, fast ? MAG_FAST_INTERVAL_US : MAG_INTERVAL_US);

  if (fast) {
    pbl_wallclock_work_start(&pbl_sys_workq_high, &s_fast_timer, 60, FAST_MINUTES);
  } else {
    pbl_wallclock_work_stop(&s_fast_timer);
  }
}

static void prv_fast_expired(struct pbl_work *work) {
  PBL_LOG_DBG("Fast calibration time expired");
  s_fast_done = true;
  prv_set_fast(false);
}

/*
 * Sensor clients
 */

static void prv_post_event_work(void *data) {
  PebbleEvent e = {
    .type = PEBBLE_ECOMPASS_SERVICE_EVENT,
  };
  event_put(&e);
}

static bool prv_mag_notify(struct pbl_sensing_client *client) {
  int32_t nt[3];
  bool got = false;

  while (pbl_sensing_read(client, nt, 1U, nullptr) == 1U) {
    got = true;
  }

  if (got) {
    s_mag_nt[0] = nt[0];
    s_mag_nt[1] = nt[1];
    s_mag_nt[2] = nt[2];

    // event_put() may block on KernelMain, which takes the device lock: post outside of it
    pbl_sys_work_call_high(prv_post_event_work, nullptr);
  }

  pbl_sensing_arm(client, 1U);

  return true;
}

static bool prv_accel_notify(struct pbl_sensing_client *client) {
  int32_t ug[ACCEL_BATCH][3];
  uint16_t n;

  while ((n = pbl_sensing_read(client, &ug[0][0], ACCEL_BATCH, nullptr)) > 0U) {
    for (uint16_t k = 0U; k < n; k++) {
      for (int i = 0; i < 3; i++) {
        float g = (float)ug[k][i] / 1e6f;
        s_gravity[i] = s_gravity_valid ? s_gravity[i] + ACCEL_ALPHA * (g - s_gravity[i]) : g;
      }
      s_gravity_valid = true;
    }
  }

  pbl_sensing_arm(client, ACCEL_BATCH);

  return true;
}

/*
 * Heading
 */

static void prv_cross(const float a[3], const float b[3], float out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

static float prv_norm(const float v[3]) {
  return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

// Heading of the top of the watch, clockwise from magnetic north, from the field and gravity in
// the watch frame
static bool prv_heading(const float m[3], const float up[3], int32_t *heading) {
  const float down[3] = {-up[0], -up[1], -up[2]};
  float east[3];
  float north[3];

  prv_cross(down, m, east);
  prv_cross(east, down, north);

  float en = prv_norm(east);
  float nn = prv_norm(north);
  if (en <= 0.0f || nn <= 0.0f) {
    return false;
  }

  // Top of the watch, or its back when the top points up or down
  float fe = east[1] / en;
  float fn = north[1] / nn;
  if (fe * fe + fn * fn < MIN_HORIZONTAL * MIN_HORIZONTAL) {
    fe = -east[2] / en;
    fn = -north[2] / nn;
  }

  *heading = atan2_lookup((int16_t)(fe * 16384.0f), (int16_t)(fn * 16384.0f)) % TRIG_MAX_ANGLE;

  return true;
}

static CompassStatus prv_status(const float m[3]) {
  const struct ecompass_cal *cal = &s_cal.cal;

  if (cal->order == 0U || cal->error > CALIBRATING_ERROR) {
    return CompassStatusDataInvalid;
  }

  if (!s_cal.verified || s_charger_plugged || cal->error > CALIBRATED_ERROR ||
      fabsf(ecompass_cal_deviation(cal, m)) > DISTURBED_DEVIATION) {
    return CompassStatusCalibrating;
  }

  return CompassStatusCalibrated;
}

/*
 * Subscriptions
 */

static void prv_stop(PebbleTask task) {
  if (!s_subscribed[task]) {
    return;
  }

  s_subscribed[task] = false;
  PBL_LOG_DBG("subscribers %d", (int)(s_subscribers - 1U));

  if (--s_subscribers != 0U) {
    return;
  }

  prv_set_fast(false);
  pbl_sensing_client_remove(&s_accel_client);
  pbl_sensing_client_remove(&s_mag_client);
  s_gravity_valid = false;

  if (s_dirty) {
    prv_save();
  }
}

static void prv_start(PebbleTask task) {
  prv_stop(task);

  s_subscribed[task] = true;
  PBL_LOG_DBG("subscribers %d", (int)(s_subscribers + 1U));

  if (s_subscribers++ != 0U) {
    return;
  }

  if (!s_loaded) {
    prv_load();
  }

  s_fast = false;
  s_events = 0U;
  s_mag_client.interval_us = MAG_INTERVAL_US;
  if (pbl_sensing_client_add(&s_mag_client) == 0) {
    pbl_sensing_arm(&s_mag_client, 1U);
  }
  if (pbl_sensing_client_add(&s_accel_client) == 0) {
    pbl_sensing_arm(&s_accel_client, ACCEL_BATCH);
  }
}

/*
 * Public API
 */

void ecompass_handle_battery_state_change_event(PreciseBatteryChargeState new_state) {
  if (new_state.is_plugged == s_charger_plugged) {
    return;
  }

  // Charging currents distort the field: calibration pauses meanwhile, and starts afresh after
  s_charger_plugged = new_state.is_plugged;
  if (!s_charger_plugged) {
    ecompass_cal_reset_samples(&s_cal);
    s_fast_done = false;
  }
}

void ecompass_service_init(void) {
  pbl_wallclock_work_init(&s_fast_timer, prv_fast_expired);

  s_mag_client = (struct pbl_sensing_client){
    .sensor = MAG,
    .type = PBL_SENSOR_MAGN,
    .batch = 1U,
    .notify = prv_mag_notify,
  };
  s_accel_client = (struct pbl_sensing_client){
    .sensor = ACCEL,
    .type = PBL_SENSOR_ACCEL,
    .interval_us = ACCEL_INTERVAL_US,
    .batch = ACCEL_BATCH,
    .notify = prv_accel_notify,
  };

  event_service_init(PEBBLE_COMPASS_DATA_EVENT, &prv_start, &prv_stop);
}

void ecompass_service_handle(void) {
  float m[3];
  float up[3];
  float corrected[3];
  bool gravity_valid;
  int32_t heading;

  if (s_subscribers == 0U) {
    return;
  }

  pbl_sensor_lock(MAG);
  for (int i = 0; i < 3; i++) {
    m[i] = (float)s_mag_nt[i] / 1000.0f;
  }
  pbl_sensor_unlock(MAG);

  pbl_sensor_lock(ACCEL);
  gravity_valid = s_gravity_valid;
  for (int i = 0; i < 3; i++) {
    up[i] = s_gravity[i];
  }
  pbl_sensor_unlock(ACCEL);

  if (!s_charger_plugged && ecompass_cal_add(&s_cal, m)) {
    PBL_LOG_INFO("Calibrated: %d parameters, error %d permille", (int)s_cal.cal.order,
                 (int)(s_cal.cal.error * 1000.0f));
    s_dirty = true;
  }

  s_status = prv_status(m);

  if (s_dirty && s_status == CompassStatusCalibrated &&
      (uint32_t)(pbl_uptime_ms() / 1000U) - s_last_save_s >= SAVE_INTERVAL_S) {
    prv_save();
  }

  // Sample faster until calibrated, for a while
  prv_set_fast(s_status != CompassStatusCalibrated && !s_fast_done && !s_charger_plugged);

  if (s_fast && (s_events++ % FAST_EVENT_DIVIDER) != 0U) {
    return;
  }

  ecompass_cal_apply(&s_cal.cal, m, corrected);
  if (!gravity_valid || !prv_heading(corrected, up, &heading)) {
    return;
  }

  s_last_heading = heading;

  PebbleEvent e = {
    .type = PEBBLE_COMPASS_DATA_EVENT,
    .compass_data = {
      .magnetic_heading = heading,
      .calib_status = s_status,
    },
  };
  event_put(&e);
}

/*
 * System calls
 */

PBL_SYSCALL(bool, sys_ecompass_service_subscribed) {
  PebbleTask task = pebble_task_get_current();
  return s_subscribed[task];
}

PBL_SYSCALL(void, sys_ecompass_get_last_heading, (OUT, CompassHeadingData *, data)) {
  *data = (CompassHeadingData){
    .magnetic_heading = s_last_heading,
    .true_heading = s_last_heading,
    .compass_status = s_status,
    .is_declination_valid = false,
  };
}
