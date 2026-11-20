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

PBL_LOG_MODULE_DECLARE(driver_clk, CONFIG_DRIVER_CLK_LOG_LEVEL);

#define RETRY_MS    1000U
#define MAX_RETRIES 5U
#define POLL_MS     10U

static const struct pbl_clk_cal *s_cals;

static double prv_gate_s(const struct pbl_clk_cal *cal, uint32_t cycles) {
  return cycles / (cal->state->rate_mhz / 1000.0);
}

static void prv_set_rate(const struct pbl_clk_cal *cal, uint64_t mhz) {
  if (mhz != cal->state->rate_mhz) {
    cal->state->rate_mhz = mhz;
    pbl_clk_rate_changed(&cal->clk);
  }
}

static void prv_publish(const struct pbl_clk_cal *cal) {
  double hz = pbl_freq_track_get(&cal->state->ft);

  if (cal->state->held_mhz == 0U) {
    prv_set_rate(cal, (uint64_t)llround(hz * 1000.0));
  }
}

static void prv_feed(const struct pbl_clk_cal *cal, double hz) {
  struct pbl_clk_cal_state *st = cal->state;

  if (pbl_freq_track_update(&st->ft, hz, cal->gate_cycles / hz, pbl_uptime_ms()) != 0) {
    st->rejected++;
    PBL_LOG_DBG("Rejected %d mHz", (int)llround(hz * 1000.0));
    return;
  }

  st->accepted++;
  prv_publish(cal);
}

static void prv_schedule(const struct pbl_clk_cal *cal, uint32_t ms) {
  pbl_timer_start(&cal->state->timer, PBL_MSEC(ms));
}

static void prv_timer_cb(struct pbl_timer *timer) {
  struct pbl_clk_cal_state *st = PBL_CONTAINER_OF(timer, struct pbl_clk_cal_state, timer);
  const struct pbl_clk_cal *cal = st->cal;
  double hz;
  int ret;

  if (st->paused) {
    prv_schedule(cal, RETRY_MS);
    return;
  }

  if (!st->gate_running) {
    ret = cal->ops->start(cal, cal->gate_cycles);
    if (ret == 0) {
      st->gate_running = true;
      prv_schedule(cal, (uint32_t)(prv_gate_s(cal, cal->gate_cycles) * 1000.0) + 1U);
      return;
    }
    if (ret == -EBUSY && st->retries < MAX_RETRIES) {
      st->retries++;
      prv_schedule(cal, RETRY_MS);
      return;
    }
    st->failed++;
    if (st->accepted > 0U) {
      PBL_LOG_WRN("Calibration skipped (%d)", ret);
    } else {
      PBL_LOG_DBG("Calibration not ready (%d)", ret);
    }
  } else {
    ret = cal->ops->finish(cal, cal->gate_cycles, &hz);
    // Kernel time may run from another clock than the one measured, so a gate can end late
    if (ret == -EINPROGRESS) {
      prv_schedule(cal, POLL_MS);
      return;
    }
    st->gate_running = false;
    if (ret == 0) {
      prv_feed(cal, hz);
    } else {
      st->failed++;
      PBL_LOG_WRN("Calibration failed (%d)", ret);
    }
  }

  st->retries = 0U;
  prv_schedule(cal, (st->accepted > 0U) ? cal->period_ms : RETRY_MS);
}

void pbl_clk_cal_start(const struct pbl_clk_cal *cal, double nominal_hz) {
  struct pbl_clk_cal_state *st = cal->state;

  st->cal = cal;
  pbl_freq_track_init(&st->ft, cal->params);
  st->rate_mhz = (uint64_t)llround(nominal_hz * 1000.0);
  pbl_timer_init(&st->timer, prv_timer_cb);

  pbl_irq_lock();
  st->next = s_cals;
  s_cals = cal;
  pbl_irq_unlock();

  prv_schedule(cal, 0U);
}

#ifdef CONFIG_DEVICE_NAMES
const struct pbl_clk_cal *pbl_clk_cal_find(const char *name) {
  for (const struct pbl_clk_cal *cal = s_cals; cal != nullptr; cal = cal->state->next) {
    if (strcmp(cal->clk.ctrl->dev.name, name) == 0) {
      return cal;
    }
  }

  return nullptr;
}
#endif

int pbl_clk_cal_sample(const struct pbl_clk_cal *cal, uint32_t cycles, double *hz) {
  struct pbl_clk_cal_state *st = cal->state;
  int ret = -EBUSY;

  // Let a periodic gate in progress finish, and keep the next one off.
  for (;;) {
    pbl_irq_lock();
    if (!st->gate_running) {
      st->paused = true;
    }
    pbl_irq_unlock();
    if (st->paused) {
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
    while (ret == -EINPROGRESS) {
      pbl_thread_sleep(PBL_MSEC(POLL_MS));
      ret = cal->ops->finish(cal, cycles, hz);
    }
  }

  st->paused = false;

  return ret;
}

void pbl_clk_cal_hold(const struct pbl_clk_cal *cal, uint64_t rate_mhz) {
  pbl_irq_lock();
  cal->state->held_mhz = rate_mhz;
  pbl_irq_unlock();

  if (rate_mhz != 0U) {
    prv_set_rate(cal, rate_mhz);
  } else if (cal->state->accepted > 0U) {
    prv_publish(cal);
  }
}

void pbl_clk_cal_get_status(const struct pbl_clk_cal *cal, struct pbl_clk_cal_status *status) {
  const struct pbl_clk_cal_state *st = cal->state;

  pbl_irq_lock();
  double hz = pbl_freq_track_get(&st->ft);

  *status = (struct pbl_clk_cal_status){
    .tracked_mhz = (uint64_t)llround(hz * 1000.0),
    .rate_mhz = st->rate_mhz,
    .sigma_ppb = (uint32_t)llround(pbl_freq_track_get_dev(&st->ft) * 1e9),
    .accepted = st->accepted,
    .rejected = st->rejected,
    .failed = st->failed,
    .held = st->held_mhz != 0U,
  };
  pbl_irq_unlock();
}

uint64_t pbl_clk_cal_get_rate(const struct pbl_clk_cal *cal) {
  return cal->state->rate_mhz;
}
