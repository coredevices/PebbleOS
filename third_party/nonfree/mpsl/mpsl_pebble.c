/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>

#include <devicetree/regs.h>
#include <hardware/nrf54l_clock.h>
#include <hardware/nrf54l_rramc.h>
#include <mpsl_pebble.h>

#undef UNUSED
#include <mpsl.h>
#include <mpsl_clock.h>
#include <mpsl_timeslot.h>

#define HFXO_STARTUP_US   1650U
#define HFXO_TIMEOUT_US   (4U * HFXO_STARTUP_US)
#define LFXO_ACCURACY_PPM 50U

#define RRAMC_POWER_LOWPOWERCONFIG_STANDBY 1U

// Longest wait for a timeslot, and for each request to be granted
#define TIMESLOT_TIMEOUT_MS     1000U
#define TIMESLOT_REQ_TIMEOUT_US 100000U
#define TIMESLOT_MAX_ATTEMPTS   5U

static PBL_MUTEX_DEFINE(s_lock);
static uint32_t s_rram_lowpower;
static bool s_started;

static struct {
  struct pbl_mutex lock;
  struct pbl_sem idle;
  uint8_t mem[MPSL_TIMESLOT_CONTEXT_SIZE] PBL_ALIGNED(4);
  mpsl_timeslot_request_t req;
  mpsl_timeslot_signal_return_param_t ret;
  void (*fn)(void *arg);
  void *arg;
  volatile bool ran;
} s_ts;

static void prv_low_prio_work(struct pbl_work *work) {
  mpsl_pebble_lock();
  mpsl_low_priority_process();
  mpsl_pebble_unlock();
}

static PBL_WORK_DEFINE(s_low_prio, prv_low_prio_work);
// Own queue, so that callers on any other one can wait for its signals
static PBL_WORKQ_DEFINE(s_workq);
static struct pbl_thread s_thread;
PBL_THREAD_STACK_DEFINE(s_stack, CONFIG_MPSL_WORKQ_STACK_SIZE);

PBL_IRQ_DIRECT(RADIO_0, 0, PBL_IRQ_ZERO_LATENCY) {
  MPSL_IRQ_RADIO_Handler();
}

PBL_IRQ_DIRECT(TIMER10, 0, PBL_IRQ_ZERO_LATENCY) {
  MPSL_IRQ_TIMER0_Handler();
}

PBL_IRQ_DIRECT(GRTC_3, 0, PBL_IRQ_ZERO_LATENCY) {
  MPSL_IRQ_RTC0_Handler();
}

PBL_IRQ_DIRECT(CLOCK_POWER, 0, 0) {
  MPSL_IRQ_CLOCK_Handler();
}

PBL_IRQ_DIRECT(SWI00, CONFIG_MPSL_LOW_PRIO, 0) {
  (void)pbl_work_submit(&s_workq, &s_low_prio);
}

static void prv_assert(const char *const file, const uint32_t line) {
  pbl_panic_at(file, (int)line, "MPSL assert");
}

static void prv_hfxo_started(mpsl_clock_evt_type_t evt) {
}

// The RRAM must wake fast enough for the radio events, and the regulators keep constant latency
void mpsl_low_latency_acquire_callback(void) {
  pbl_writel(1U, PBL_DT_REG_CLOCK + NRF54L_POWER_TASKS_CONSTLAT_OFFSET);
  s_rram_lowpower = pbl_readl(PBL_DT_REG_RRAMC + NRF54L_RRAMC_POWER_LOWPOWERCONFIG_OFFSET);
  pbl_writel(RRAMC_POWER_LOWPOWERCONFIG_STANDBY,
             PBL_DT_REG_RRAMC + NRF54L_RRAMC_POWER_LOWPOWERCONFIG_OFFSET);
}

void mpsl_low_latency_release_callback(void) {
  pbl_writel(1U, PBL_DT_REG_CLOCK + NRF54L_POWER_TASKS_LOWPWR_OFFSET);
  pbl_writel(s_rram_lowpower, PBL_DT_REG_RRAMC + NRF54L_RRAMC_POWER_LOWPOWERCONFIG_OFFSET);
}

static mpsl_timeslot_signal_return_param_t *prv_timeslot_cb(mpsl_timeslot_session_id_t session,
                                                            uint32_t signal) {
  s_ts.ret.callback_action = MPSL_TIMESLOT_SIGNAL_ACTION_NONE;

  switch (signal) {
    case MPSL_TIMESLOT_SIGNAL_START:
      s_ts.fn(s_ts.arg);
      s_ts.ran = true;
      s_ts.ret.callback_action = MPSL_TIMESLOT_SIGNAL_ACTION_END;
      break;
    case MPSL_TIMESLOT_SIGNAL_SESSION_IDLE:
      pbl_sem_give(&s_ts.idle);
      break;
    default:
      break;
  }

  return &s_ts.ret;
}

// One timeslot request, returning once the session is idle again
static int prv_timeslot_once(uint32_t length_us) {
  mpsl_timeslot_session_id_t session;
  int ret = 0;

  s_ts.ran = false;
  pbl_sem_reset(&s_ts.idle);
  s_ts.req = (mpsl_timeslot_request_t){
    .request_type = MPSL_TIMESLOT_REQ_TYPE_EARLIEST,
    .params.earliest = {
      .hfclk = MPSL_TIMESLOT_HFCLK_CFG_NO_GUARANTEE,
      .priority = MPSL_TIMESLOT_PRIORITY_NORMAL,
      .length_us = length_us,
      .timeout_us = TIMESLOT_REQ_TIMEOUT_US,
    },
  };

  mpsl_pebble_lock();
  if (mpsl_timeslot_session_open(prv_timeslot_cb, &session) != 0) {
    mpsl_pebble_unlock();
    return -EIO;
  }
  if (mpsl_timeslot_request(session, &s_ts.req) != 0) {
    ret = -EIO;
  }
  mpsl_pebble_unlock();

  if (ret == 0 && pbl_sem_take(&s_ts.idle, PBL_MSEC(TIMESLOT_TIMEOUT_MS)) != 0) {
    ret = -ETIMEDOUT;
  }

  mpsl_pebble_lock();
  (void)mpsl_timeslot_session_close(session);
  mpsl_pebble_unlock();

  if (ret == 0 && !s_ts.ran) {
    ret = -ETIMEDOUT;
  }

  return ret;
}

int mpsl_pebble_timeslot_run(void (*fn)(void *arg), void *arg, uint32_t length_us) {
  int ret = -ETIMEDOUT;

  if (!s_started) {
    fn(arg);
    return 0;
  }

  pbl_mutex_lock(&s_ts.lock, PBL_FOREVER);
  s_ts.fn = fn;
  s_ts.arg = arg;
  for (unsigned int i = 0U; i < TIMESLOT_MAX_ATTEMPTS && ret == -ETIMEDOUT; i++) {
    ret = prv_timeslot_once(length_us);
  }
  pbl_mutex_unlock(&s_ts.lock);

  return ret;
}

void mpsl_pebble_lock(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
}

void mpsl_pebble_unlock(void) {
  pbl_mutex_unlock(&s_lock);
}

int mpsl_pebble_hfxo(bool on) {
  uint32_t running = 0U;
  int32_t ret;
  int err;

  mpsl_pebble_lock();
  ret = on ? mpsl_clock_hfclk_src_request(MPSL_CLOCK_HF_SRC_XO, prv_hfxo_started)
           : mpsl_clock_hfclk_src_release(MPSL_CLOCK_HF_SRC_XO);
  mpsl_pebble_unlock();
  if (ret != 0) {
    return -EIO;
  }
  if (!on) {
    return 0;
  }

  err = PBL_POLL_BUSY_US(
      (mpsl_clock_hfclk_src_is_running(MPSL_CLOCK_HF_SRC_XO, &running) == 0) && (running != 0U),
      10U, HFXO_TIMEOUT_US);

  return err;
}

int mpsl_pebble_init(void) {
  static const mpsl_clock_lfclk_cfg_t lfclk = {
    .source = MPSL_CLOCK_LF_SRC_XTAL,
    .accuracy_ppm = LFXO_ACCURACY_PPM,
  };
  const struct pbl_thread_attr attr = {
    .name = "MPSL",
    .entry = pbl_workq_thread,
    .arg = &s_workq,
    .prio = CONFIG_MPSL_WORKQ_PRIO,
    .privileged = true,
    .stack = s_stack,
    .stack_size = sizeof(s_stack),
  };

  if (pbl_thread_create(&s_thread, &attr) != 0) {
    return -EIO;
  }

  pbl_irq_enable(PBL_IRQN(SWI00));
  pbl_irq_enable(PBL_IRQN(CLOCK_POWER));
  pbl_irq_enable(PBL_IRQN(RADIO_0));
  pbl_irq_enable(PBL_IRQN(TIMER10));
  pbl_irq_enable(PBL_IRQN(GRTC_3));

  if (mpsl_init(&lfclk, SWI00_IRQn, prv_assert) != 0) {
    return -EIO;
  }

  if (mpsl_clock_hfclk_latency_set(HFXO_STARTUP_US) != 0) {
    return -EIO;
  }

  pbl_mutex_init(&s_ts.lock);
  pbl_sem_init(&s_ts.idle, 0U, 1U);
  if (mpsl_timeslot_session_count_set(s_ts.mem, 1U) != 0) {
    return -EIO;
  }

  s_started = true;

  return 0;
}
