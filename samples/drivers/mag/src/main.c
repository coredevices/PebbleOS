/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/sensor.h>
#include <pbl/kernel/delay.h>

#define INTERVAL_US 200000
#define LISTEN_S    3

/* Written by the listener, which runs with the device lock held. */
static unsigned int s_samples;

static void prv_batch(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch,
                      void *user_data) {
  s_samples += batch->num_frames;
}

static const struct pbl_sensor_listener s_listener = {
  .batch = prv_batch,
};

static int prv_print_field(const char *when) {
  int32_t nt[3];
  int ret;

  ret = pbl_sensor_read(MAG, PBL_SENSOR_MAGN, nt, nullptr);
  if (ret != 0) {
    fprintf(stderr, "mag: read failed (%d)\n", ret);
    return ret;
  }

  printf("mag: %s: x=%d y=%d z=%d nT\n", when, (int)nt[0], (int)nt[1], (int)nt[2]);

  return 0;
}

int main(void) {
  int32_t interval_us = INTERVAL_US;
  int ret;

  /* While the magnetometer is not sampling, a read runs a single measurement. */
  ret = prv_print_field("single");
  if (ret != 0) {
    return ret;
  }

  pbl_sensor_set_listener(MAG, &s_listener);

  ret = pbl_sensor_attr_set(MAG, PBL_SENSOR_MAGN, PBL_SENSOR_ATTR_INTERVAL, &interval_us);
  if (ret != 0) {
    fprintf(stderr, "mag: could not start sampling (%d)\n", ret);
    return ret;
  }

  printf("mag: sampling every %d us\n", (int)interval_us);

  for (int i = 0; i < LISTEN_S; i++) {
    unsigned int samples;

    pbl_sleep_ms(1000);

    ret = prv_print_field("latest");
    if (ret != 0) {
      break;
    }

    /* The lock keeps the listener out while reading what it wrote. */
    pbl_sensor_lock(MAG);
    samples = s_samples;
    pbl_sensor_unlock(MAG);

    printf("mag: %d samples so far\n", (int)samples);
  }

  /* An interval of 0 stops sampling, and lets the sensor sleep again. */
  (void)pbl_sensor_attr_set(MAG, PBL_SENSOR_MAGN, PBL_SENSOR_ATTR_INTERVAL, &(int32_t){0});
  pbl_sensor_set_listener(MAG, nullptr);
  if (ret != 0) {
    return ret;
  }

  printf("mag: %d samples\n", (int)s_samples);
  printf("Sample done\n");

  return 0;
}
