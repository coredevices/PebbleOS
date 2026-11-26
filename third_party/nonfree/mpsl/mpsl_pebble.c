/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/work.h>

#include <devicetree/regs.h>
#include <hardware/nrf54l_clock.h>
#include <hardware/nrf54l_rramc.h>
#include <mpsl_pebble.h>

#undef UNUSED
#include <mpsl.h>
#include <mpsl_clock.h>

#define HFXO_STARTUP_US   1650U
#define HFXO_TIMEOUT_US   (4U * HFXO_STARTUP_US)
#define LFXO_ACCURACY_PPM 50U

#define RRAMC_POWER_LOWPOWERCONFIG_STANDBY 1U

static PBL_MUTEX_DEFINE(s_lock);
static uint32_t s_rram_lowpower;

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

  return 0;
}
