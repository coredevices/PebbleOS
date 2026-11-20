/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/accel.h>
#include <pbl/kernel/delay.h>
#include <pbl/util/math.h>

#define INTERVAL_US 40000U
#define BATCH       10U
#define LISTEN_S    5

/* Written by the listener, which runs with the device lock held. */
static struct pbl_accel_sample s_last;
static unsigned int s_samples;
static unsigned int s_shakes;
static unsigned int s_taps;

static int16_t prv_axis(const AccelRawBatch *batch, const uint8_t *sample, IMUCoordinateAxis axis) {
  const uint8_t *p = sample + batch->axis[axis].offset;
  int16_t raw = (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8U));

  return (int16_t)(batch->axis[axis].sign * (int32_t)raw * batch->scale_num / batch->scale_den);
}

/* Batches carry the sensor's raw samples, which the batch says how to scale to milli-g. */
static void prv_samples(const struct pbl_accel *accel, const AccelRawBatch *batch,
                        void *user_data) {
  const uint8_t *last = batch->data + (batch->num_samples - 1U) * batch->stride;

  if (batch->num_samples == 0U) {
    return;
  }

  s_last.x = prv_axis(batch, last, AXIS_X);
  s_last.y = prv_axis(batch, last, AXIS_Y);
  s_last.z = prv_axis(batch, last, AXIS_Z);
  s_samples += batch->num_samples;
}

static void prv_event(const struct pbl_accel *accel, const struct pbl_accel_event *event,
                      void *user_data) {
  if (event->type == PBL_ACCEL_EVENT_SHAKE) {
    s_shakes++;
  } else if (event->type == PBL_ACCEL_EVENT_DOUBLE_TAP) {
    s_taps++;
  }
}

static const struct pbl_accel_listener s_listener = {
  .samples = prv_samples,
  .event = prv_event,
};

int main(void) {
  struct pbl_accel_sample now;
  uint32_t interval_us;
  uint32_t batch;
  int ret;

  ret = pbl_accel_peek(ACCEL, &now);
  if (ret != 0) {
    fprintf(stderr, "accel: peek failed (%d)\n", ret);
    return ret;
  }

  printf("accel: now x=%d y=%d z=%d mg\n", now.x, now.y, now.z);

  pbl_accel_set_listener(ACCEL, &s_listener);

  ret = pbl_accel_set_sampling_interval(ACCEL, INTERVAL_US, &interval_us);
  if (ret != 0) {
    fprintf(stderr, "accel: could not set the sampling interval (%d)\n", ret);
    return ret;
  }

  /* Up to BATCH samples per report, so the sensor can sleep between them. */
  batch = PBL_MIN(BATCH, pbl_accel_get_max_num_samples(ACCEL));
  ret = pbl_accel_set_num_samples(ACCEL, batch);
  if (ret != 0) {
    fprintf(stderr, "accel: could not start sampling (%d)\n", ret);
    return ret;
  }

  printf("accel: sampling every %d us, batches of up to %d\n", (int)interval_us, (int)batch);

  /* Not every sensor detects these: -ENOTSUP is not an error. */
  ret = pbl_accel_set_shake_detection(ACCEL, true);
  if (ret != 0 && ret != -ENOTSUP) {
    fprintf(stderr, "accel: could not enable shake detection (%d)\n", ret);
    return ret;
  }

  ret = pbl_accel_set_double_tap_detection(ACCEL, true);
  if (ret != 0 && ret != -ENOTSUP) {
    fprintf(stderr, "accel: could not enable double tap detection (%d)\n", ret);
    return ret;
  }

  for (int i = 0; i < LISTEN_S; i++) {
    pbl_sleep_ms(1000);

    /* The lock keeps the listener out while reading what it wrote. */
    pbl_accel_lock(ACCEL);
    printf("accel: x=%d y=%d z=%d mg, %d samples so far\n", s_last.x, s_last.y, s_last.z,
           (int)s_samples);
    pbl_accel_unlock(ACCEL);
  }

  (void)pbl_accel_set_shake_detection(ACCEL, false);
  (void)pbl_accel_set_double_tap_detection(ACCEL, false);
  (void)pbl_accel_set_num_samples(ACCEL, 0U);
  pbl_accel_set_listener(ACCEL, nullptr);

  printf("accel: %d samples, %d shakes, %d double taps\n", (int)s_samples, (int)s_shakes,
         (int)s_taps);
  printf("Sample done\n");

  return 0;
}
