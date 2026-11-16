/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/mag.h>
#include <pbl/kernel/delay.h>

#define INTERVAL_US 200000U
#define LISTEN_S    3

/* Written by the listener, which runs with the device lock held. */
static unsigned int s_samples;

static void prv_sample(const struct pbl_mag *mag, const MagData *sample, void *user_data) {
  s_samples++;
}

static const struct pbl_mag_listener s_listener = {
  .sample = prv_sample,
};

int main(void) {
  uint32_t interval_us;
  MagData field;
  int ret;

  /* The magnetometer sleeps until someone samples it: there is nothing to read yet. */
  ret = pbl_mag_peek(MAG, &field);
  printf("mag: before sampling: %s\n", ret == -ENODATA ? "no data" : "data");

  pbl_mag_set_listener(MAG, &s_listener);

  ret = pbl_mag_set_sampling_interval(MAG, INTERVAL_US, &interval_us);
  if (ret != 0) {
    fprintf(stderr, "mag: could not start sampling (%d)\n", ret);
    return ret;
  }

  printf("mag: sampling every %d us\n", (int)interval_us);

  for (int i = 0; i < LISTEN_S; i++) {
    unsigned int samples;

    pbl_sleep_ms(1000);

    ret = pbl_mag_peek(MAG, &field);
    if (ret != 0) {
      fprintf(stderr, "mag: peek failed (%d)\n", ret);
      break;
    }

    /* The lock keeps the listener out while reading what it wrote. */
    pbl_mag_lock(MAG);
    samples = s_samples;
    pbl_mag_unlock(MAG);

    printf("mag: x=%d y=%d z=%d mG, %d samples so far\n", field.x, field.y, field.z, (int)samples);
  }

  /* An interval of 0 stops sampling, and lets the sensor sleep again. */
  (void)pbl_mag_set_sampling_interval(MAG, 0U, nullptr);
  pbl_mag_set_listener(MAG, nullptr);
  if (ret != 0) {
    return ret;
  }

  printf("mag: %d samples\n", (int)s_samples);
  printf("Sample done\n");

  return 0;
}
