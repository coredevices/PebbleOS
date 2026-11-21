/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/sensor.h>
#include <pbl/kernel/delay.h>
#include <pbl/util/math.h>

#define INTERVAL_US 40000
#define BATCH       10
#define LISTEN_S    5

/* Written by the listener, which runs with the device lock held. */
static int32_t s_last[3];
static unsigned int s_samples;
static unsigned int s_shakes;
static unsigned int s_taps;

/* Batches carry the sensor's raw frames, which the class decodes to micro-g in the watch frame. */
static void prv_batch(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch,
                      void *user_data) {
  if (batch->num_frames == 0U) {
    return;
  }

  (void)pbl_sensor_decode(sensor, batch, PBL_SENSOR_ACCEL, batch->num_frames - 1U, 1U, s_last);
  s_samples += batch->num_frames;
}

static void prv_event(const struct pbl_sensor *sensor, const struct pbl_sensor_event *event,
                      void *user_data) {
  if (event->type == PBL_SENSOR_EVENT_SHAKE) {
    s_shakes++;
  } else if (event->type == PBL_SENSOR_EVENT_DOUBLE_TAP) {
    s_taps++;
  }
}

static const struct pbl_sensor_listener s_listener = {
  .batch = prv_batch,
  .event = prv_event,
};

static int prv_set(enum pbl_sensor_attr attr, int32_t *val) {
  return pbl_sensor_attr_set(ACCEL, PBL_SENSOR_ACCEL, attr, val);
}

int main(void) {
  int32_t now[3];
  int32_t interval_us = INTERVAL_US;
  int32_t batch = 0;
  int ret;

  ret = pbl_sensor_read(ACCEL, PBL_SENSOR_ACCEL, now, nullptr);
  if (ret != 0) {
    fprintf(stderr, "accel: read failed (%d)\n", ret);
    return ret;
  }

  printf("accel: now x=%d y=%d z=%d mg\n", (int)(now[0] / 1000), (int)(now[1] / 1000),
         (int)(now[2] / 1000));

  pbl_sensor_set_listener(ACCEL, &s_listener);

  ret = prv_set(PBL_SENSOR_ATTR_INTERVAL, &interval_us);
  if (ret != 0) {
    fprintf(stderr, "accel: could not set the sampling interval (%d)\n", ret);
    return ret;
  }

  /* Up to BATCH samples per report, so the sensor can sleep between them. */
  (void)pbl_sensor_attr_get(ACCEL, PBL_SENSOR_ACCEL, PBL_SENSOR_ATTR_BATCH_MAX, &batch);
  batch = PBL_MIN(BATCH, batch);
  ret = prv_set(PBL_SENSOR_ATTR_BATCH, &batch);
  if (ret != 0) {
    fprintf(stderr, "accel: could not start sampling (%d)\n", ret);
    return ret;
  }

  printf("accel: sampling every %d us, batches of up to %d\n", (int)interval_us, (int)batch);

  /* Not every sensor detects these: -ENOTSUP is not an error. */
  ret = pbl_sensor_set_event(ACCEL, PBL_SENSOR_EVENT_SHAKE, true);
  if (ret != 0 && ret != -ENOTSUP) {
    fprintf(stderr, "accel: could not enable shake detection (%d)\n", ret);
    return ret;
  }

  ret = pbl_sensor_set_event(ACCEL, PBL_SENSOR_EVENT_DOUBLE_TAP, true);
  if (ret != 0 && ret != -ENOTSUP) {
    fprintf(stderr, "accel: could not enable double tap detection (%d)\n", ret);
    return ret;
  }

  for (int i = 0; i < LISTEN_S; i++) {
    pbl_sleep_ms(1000);

    /* The lock keeps the listener out while reading what it wrote. */
    pbl_sensor_lock(ACCEL);
    printf("accel: x=%d y=%d z=%d mg, %d samples so far\n", (int)(s_last[0] / 1000),
           (int)(s_last[1] / 1000), (int)(s_last[2] / 1000), (int)s_samples);
    pbl_sensor_unlock(ACCEL);
  }

  (void)pbl_sensor_set_event(ACCEL, PBL_SENSOR_EVENT_SHAKE, false);
  (void)pbl_sensor_set_event(ACCEL, PBL_SENSOR_EVENT_DOUBLE_TAP, false);
  (void)prv_set(PBL_SENSOR_ATTR_BATCH, &(int32_t){0});
  pbl_sensor_set_listener(ACCEL, nullptr);

  printf("accel: %d samples, %d shakes, %d double taps\n", (int)s_samples, (int)s_shakes,
         (int)s_taps);
  printf("Sample done\n");

  return 0;
}
