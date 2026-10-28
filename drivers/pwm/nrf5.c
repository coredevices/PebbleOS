/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/drivers/pwm/nrf5.h>
#include <pbl/kernel/barrier.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/units.h>

#include <hardware/nrf52_pwm.h>

#define CLOCK_HZ 16000000ULL

// Longest period (COUNTERTOP_MAX counts at the largest prescaler), plus margin
#define STOP_TIMEOUT_US 300000U

static const uintptr_t s_bases[] = {
  NRF52_PWM0_BASE,
  NRF52_PWM1_BASE,
  NRF52_PWM2_BASE,
  NRF52_PWM3_BASE,
};

static const struct pbl_pwm_nrf5 *prv_pwm(const struct pbl_pwm_controller *ctrl) {
  return container_of(ctrl, const struct pbl_pwm_nrf5, ctrl);
}

static inline uint32_t prv_getreg(const struct pbl_pwm_nrf5 *pwm, uint32_t offset) {
  return pbl_readl(s_bases[pwm->index] + offset);
}

static inline void prv_putreg(const struct pbl_pwm_nrf5 *pwm, uint32_t offset, uint32_t value) {
  pbl_writel(value, s_bases[pwm->index] + offset);
}

static uint32_t prv_counts(uint32_t ns, uint32_t prescaler) {
  return (uint32_t)(((uint64_t)ns * (CLOCK_HZ >> prescaler)) / PBL_NSEC_PER_SEC);
}

static int prv_timing(uint32_t period, uint32_t *prescaler, uint32_t *top) {
  uint32_t p = 0U;

  while (p < PWM_PRESCALER_MAX && prv_counts(period, p) > PWM_COUNTERTOP_MAX) {
    p++;
  }

  *top = MIN(prv_counts(period, p), PWM_COUNTERTOP_MAX);
  if (*top < PWM_COUNTERTOP_MIN) {
    return -EINVAL;
  }

  *prescaler = p;

  return 0;
}

// The counter runs until the end of the period in which it is stopped
static int prv_wait_stopped(const struct pbl_pwm_nrf5 *pwm) {
  int ret;

  if (!pwm->state->stopping) {
    return 0;
  }

  ret = PBL_POLL_BUSY_US(prv_getreg(pwm, NRF52_PWM_EVENTS_STOPPED_OFFSET) != 0U, 10U,
                         STOP_TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  pwm->state->stopping = false;

  return 0;
}

static int prv_apply(const struct pbl_pwm_controller *ctrl, uint8_t channel,
                     const struct pbl_pwm_state *state) {
  const struct pbl_pwm_nrf5 *pwm = prv_pwm(ctrl);
  struct pbl_pwm_nrf5_state *st = pwm->state;
  bool inversed = state->polarity == PBL_PWM_POLARITY_INVERSED;
  uint32_t prescaler;
  uint32_t top;
  uint8_t others;
  int ret;

  if (!pbl_gpio_is_connected(&pwm->out[channel])) {
    return -EINVAL;
  }

  ret = prv_timing(state->period, &prescaler, &top);
  if (ret != 0) {
    return ret;
  }

  if (state->enabled) {
    ret = prv_wait_stopped(pwm);
    if (ret != 0) {
      return ret;
    }
  }

  // Level of the output while the PWM is stopped
  ret =
      pbl_gpio_configure(&pwm->out[channel], inversed ? PBL_GPIO_OUTPUT_HIGH : PBL_GPIO_OUTPUT_LOW);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_lock();

  others = st->running & ~PBL_BIT(channel);

  if (!state->enabled) {
    // A compare value of 0 keeps the output inactive for the whole period
    st->seq[channel] = inversed ? 0U : PWM_SEQ_FALLING_EDGE;
    if (others != 0U) {
      pbl_wmb();
      prv_putreg(pwm, NRF52_PWM_TASKS_SEQSTART0_OFFSET, 1U);
    } else if (st->running != 0U) {
      prv_putreg(pwm, NRF52_PWM_EVENTS_STOPPED_OFFSET, 0U);
      prv_putreg(pwm, NRF52_PWM_TASKS_STOP_OFFSET, 1U);
      st->stopping = true;
    }

    st->running = others;

    pbl_irq_unlock();

    return 0;
  }

  if (others != 0U && (prescaler != st->prescaler || top != st->top)) {
    pbl_irq_unlock();
    return -EBUSY;
  }

  if (others == 0U) {
    st->prescaler = prescaler;
    st->top = top;
    prv_putreg(pwm, NRF52_PWM_PRESCALER_OFFSET, prescaler);
    prv_putreg(pwm, NRF52_PWM_COUNTERTOP_OFFSET, top);
  }

  st->seq[channel] = (uint16_t)MIN(prv_counts(state->duty_cycle, prescaler), top);
  if (!inversed) {
    st->seq[channel] |= PWM_SEQ_FALLING_EDGE;
  }

  st->running = others | PBL_BIT(channel);
  // The sequence is read from RAM by EasyDMA
  pbl_wmb();
  prv_putreg(pwm, NRF52_PWM_TASKS_SEQSTART0_OFFSET, 1U);

  pbl_irq_unlock();

  return 0;
}

int pbl_pwm_nrf5_init(const struct pbl_device *dev) {
  const struct pbl_pwm_nrf5 *pwm = container_of(dev, const struct pbl_pwm_nrf5, ctrl.dev);
  int ret;

  if (pwm->index >= ARRAY_LENGTH(s_bases)) {
    return -EINVAL;
  }

  for (uint8_t i = 0U; i < NRF52_PWM_NUM_CHANNELS; i++) {
    uint32_t psel = PWM_PSEL_DISCONNECTED;

    pwm->state->seq[i] = PWM_SEQ_FALLING_EDGE;

    if (pbl_gpio_is_connected(&pwm->out[i])) {
      if (pbl_device_init(&pwm->out[i].port->dev) != 0) {
        return -ENODEV;
      }

      ret = pbl_gpio_configure(&pwm->out[i], PBL_GPIO_OUTPUT_LOW);
      if (ret != 0) {
        return ret;
      }

      psel = pbl_gpio_nrf5_pin(&pwm->out[i]);
    }

    prv_putreg(pwm, NRF52_PWM_PSEL_OUT_OFFSET(i), psel);
  }

  prv_putreg(pwm, NRF52_PWM_SHORTS_OFFSET, 0U);
  prv_putreg(pwm, NRF52_PWM_INTEN_OFFSET, 0U);
  prv_putreg(pwm, NRF52_PWM_MODE_OFFSET, PWM_MODE_UP);
  prv_putreg(pwm, NRF52_PWM_DECODER_OFFSET, PWM_DECODER_LOAD_INDIVIDUAL);
  prv_putreg(pwm, NRF52_PWM_LOOP_OFFSET, 0U);
  prv_putreg(pwm, NRF52_PWM_SEQ0_PTR_OFFSET, (uint32_t)(uintptr_t)pwm->state->seq);
  prv_putreg(pwm, NRF52_PWM_SEQ0_CNT_OFFSET, ARRAY_LENGTH(pwm->state->seq));
  prv_putreg(pwm, NRF52_PWM_SEQ0_REFRESH_OFFSET, 0U);
  prv_putreg(pwm, NRF52_PWM_SEQ0_ENDDELAY_OFFSET, 0U);
  prv_putreg(pwm, NRF52_PWM_ENABLE_OFFSET, PWM_ENABLE_ENABLED);

  return 0;
}

const struct pbl_pwm_controller_ops pbl_pwm_nrf5_ops = {
  .apply = prv_apply,
};
