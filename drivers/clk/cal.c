/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <math.h>
#include <string.h>

#include <pbl/drivers/clk/cal.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DEFINE(clk_cal, CONFIG_CLK_CAL_LOG_LEVEL);

#define RETRY_MS    1000U
#define MAX_RETRIES 5U
#define POLL_MS     10U

static struct pbl_clk_cal *s_cals;

static double prv_gate_s(const struct pbl_clk_cal *cal, uint32_t cycles) {
  return cycles / (cal->rate_mhz / 1000.0);
}

static void prv_set_rate(struct pbl_clk_cal *cal, uint64_t mhz) {
  if (mhz != cal->rate_mhz) {
    cal->rate_mhz = mhz;
    pbl_clk_rate_changed(cal->clk.ctrl, cal->clk.id, mhz);
  }
}

static void prv_publish(struct pbl_clk_cal *cal) {
  double hz = pbl_freq_track_get(&cal->ft);

  if (cal->held_mhz == 0U) {
    prv_set_rate(cal, (uint64_t)llround(hz * 1000.0));
  }
}

static void prv_feed(struct pbl_clk_cal *cal, double hz) {
  if (pbl_freq_track_update(&cal->ft, hz, cal->gate_cycles / hz, pbl_uptime_ms()) != 0) {
    cal->rejected++;
    PBL_LOG_DBG("Rejected %d mHz", (int)llround(hz * 1000.0));
    return;
  }

  cal->accepted++;
  prv_publish(cal);
}

static void prv_schedule(struct pbl_clk_cal *cal, uint32_t ms) {
  pbl_timer_start(&cal->timer, PBL_MSEC(ms));
}

static void prv_timer_cb(struct pbl_timer *timer) {
  struct pbl_clk_cal *cal = container_of(timer, struct pbl_clk_cal, timer);
  double hz;
  int ret;

  if (cal->paused) {
    prv_schedule(cal, RETRY_MS);
    return;
  }

  if (!cal->gate_running) {
    ret = cal->ops->start(cal, cal->gate_cycles);
    if (ret == 0) {
      cal->gate_running = true;
      prv_schedule(cal, (uint32_t)(prv_gate_s(cal, cal->gate_cycles) * 1000.0) + 1U);
      return;
    }
    if (ret == -EBUSY && cal->retries < MAX_RETRIES) {
      cal->retries++;
      prv_schedule(cal, RETRY_MS);
      return;
    }
    cal->failed++;
    if (cal->accepted > 0U) {
      PBL_LOG_WRN("Calibration skipped (%d)", ret);
    } else {
      PBL_LOG_DBG("Calibration not ready (%d)", ret);
    }
  } else {
    cal->gate_running = false;
    ret = cal->ops->finish(cal, cal->gate_cycles, &hz);
    if (ret == 0) {
      prv_feed(cal, hz);
    } else {
      cal->failed++;
      PBL_LOG_WRN("Calibration failed (%d)", ret);
    }
  }

  cal->retries = 0U;
  prv_schedule(cal, (cal->accepted > 0U) ? cal->period_ms : RETRY_MS);
}

void pbl_clk_cal_start(struct pbl_clk_cal *cal, double nominal_hz) {
  pbl_freq_track_init(&cal->ft, cal->params);
  cal->rate_mhz = (uint64_t)llround(nominal_hz * 1000.0);
  pbl_timer_init(&cal->timer, prv_timer_cb);

  pbl_irq_lock();
  cal->next = s_cals;
  s_cals = cal;
  pbl_irq_unlock();

  prv_schedule(cal, 0U);
}

struct pbl_clk_cal *pbl_clk_cal_find(const char *name) {
  for (struct pbl_clk_cal *cal = s_cals; cal != nullptr; cal = cal->next) {
    if (strcmp(cal->clk.ctrl->dev.name, name) == 0) {
      return cal;
    }
  }

  return nullptr;
}

int pbl_clk_cal_sample(struct pbl_clk_cal *cal, uint32_t cycles, double *hz) {
  int ret = -EBUSY;

  // Let a periodic gate in progress finish, and keep the next one off.
  for (;;) {
    pbl_irq_lock();
    if (!cal->gate_running) {
      cal->paused = true;
    }
    pbl_irq_unlock();
    if (cal->paused) {
      break;
    }
    pbl_thread_sleep(PBL_MSEC(POLL_MS));
  }

  for (unsigned int i = 0U; i < MAX_RETRIES && ret == -EBUSY; i++) {
    ret = cal->ops->start(cal, cycles);
    if (ret == -EBUSY) {
      pbl_thread_sleep(PBL_MSEC(POLL_MS));
    }
  }
  if (ret == 0) {
    pbl_thread_sleep(PBL_MSEC((uint32_t)(prv_gate_s(cal, cycles) * 1000.0) + 1U));
    ret = cal->ops->finish(cal, cycles, hz);
  }

  cal->paused = false;

  return ret;
}

void pbl_clk_cal_hold(struct pbl_clk_cal *cal, uint64_t rate_mhz) {
  pbl_irq_lock();
  cal->held_mhz = rate_mhz;
  pbl_irq_unlock();

  if (rate_mhz != 0U) {
    prv_set_rate(cal, rate_mhz);
  } else if (cal->accepted > 0U) {
    prv_publish(cal);
  }
}

void pbl_clk_cal_get_status(const struct pbl_clk_cal *cal, struct pbl_clk_cal_status *status) {
  pbl_irq_lock();
  double hz = pbl_freq_track_get(&cal->ft);

  *status = (struct pbl_clk_cal_status){
    .tracked_mhz = (uint64_t)llround(hz * 1000.0),
    .rate_mhz = cal->rate_mhz,
    .sigma_ppb = (uint32_t)llround(pbl_freq_track_get_dev(&cal->ft) * 1e9),
    .accepted = cal->accepted,
    .rejected = cal->rejected,
    .failed = cal->failed,
    .held = cal->held_mhz != 0U,
  };
  pbl_irq_unlock();
}

uint64_t pbl_clk_cal_get_rate(const struct pbl_clk_cal *cal) {
  return cal->rate_mhz;
}
