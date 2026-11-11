/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/imu/sim.h>
#include <pbl/drivers/mag.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>

static MagData s_field;
static uint32_t s_sampling_interval_us;
static struct pbl_delayed_work s_timer;

static void prv_timer_fn(struct pbl_work *work) {
  const struct pbl_mag *mag = SIM_MAG;

  pbl_mag_lock(mag);

  if (s_sampling_interval_us != 0U) {
    pbl_mag_report_sample(mag, &s_field);

    if (!pbl_delayed_work_is_pending(&s_timer)) {
      pbl_delayed_work_schedule_next(&s_timer, pbl_ms_to_ticks(s_sampling_interval_us / 1000U));
    }
  }

  pbl_mag_unlock(mag);
}

void pbl_mag_sim_set(const struct pbl_mag *mag, const MagData *field) {
  pbl_mag_lock(mag);
  s_field = *field;
  pbl_mag_unlock(mag);
}

static int prv_init(const struct pbl_mag *mag) {
  s_field = (MagData){};
  s_sampling_interval_us = 0U;
  pbl_delayed_work_init(&s_timer, prv_timer_fn);

  return 0;
}

static int prv_set_sampling_interval(const struct pbl_mag *mag, uint32_t interval_us,
                                     uint32_t *actual_us) {
  s_sampling_interval_us = interval_us;

  if (interval_us == 0U) {
    pbl_delayed_work_cancel(&s_timer);
  } else {
    pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_timer, PBL_MSEC(interval_us / 1000U));
  }

  *actual_us = interval_us;

  return 0;
}

static int prv_peek(const struct pbl_mag *mag, MagData *sample) {
  if (s_sampling_interval_us == 0U) {
    return -ENODATA;
  }

  *sample = s_field;

  return 0;
}

static const struct pbl_mag_ops s_ops = {
  .init = prv_init,
  .set_sampling_interval = prv_set_sampling_interval,
  .peek = prv_peek,
};

PBL_MAG_STATE_DEFINE(pbl_mag_sim);
const struct pbl_mag pbl_mag_sim = PBL_MAG_INIT(pbl_mag_sim, "mag", nullptr, &s_ops);
PBL_DEVICE_REGISTER(pbl_mag_sim, &pbl_mag_sim.dev);
