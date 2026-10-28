/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/drivers/pwm/nrf5_rtc.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/nrf/ppi.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/units.h>

#include <hardware/nrf52_gpiote.h>
#include <hardware/nrf52_rtc.h>

#define CC_PERIOD 0U
#define CC_DUTY   1U

static const uintptr_t s_rtc_bases[] = {
  NRF52_RTC0_BASE,
  NRF52_RTC1_BASE,
  NRF52_RTC2_BASE,
};

static const struct pbl_pwm_nrf5_rtc *prv_pwm(const struct pbl_pwm_controller *ctrl) {
  return container_of(ctrl, const struct pbl_pwm_nrf5_rtc, ctrl);
}

static inline uintptr_t prv_rtc_reg(const struct pbl_pwm_nrf5_rtc *pwm, uint32_t offset) {
  return s_rtc_bases[pwm->rtc] + offset;
}

static inline uintptr_t prv_gpiote_reg(uint32_t offset) {
  return NRF52_GPIOTE_BASE + offset;
}

static void prv_stop(const struct pbl_pwm_nrf5_rtc *pwm) {
  const struct pbl_pwm_nrf5_rtc_state *st = pwm->state;

  pbl_writel(1U, prv_rtc_reg(pwm, NRF52_RTC_TASKS_STOP_OFFSET));
  soc_nrf_ppi_disable(PBL_BIT(st->ppi_ch[0]) | PBL_BIT(st->ppi_ch[1]));
  pbl_writel(1U, prv_rtc_reg(pwm, NRF52_RTC_TASKS_CLEAR_OFFSET));
  pbl_writel(0U, prv_rtc_reg(pwm, NRF52_RTC_EVENTS_COMPARE_OFFSET(CC_PERIOD)));
  pbl_writel(0U, prv_rtc_reg(pwm, NRF52_RTC_EVENTS_COMPARE_OFFSET(CC_DUTY)));
}

static void prv_hold(const struct pbl_pwm_nrf5_rtc *pwm, bool high) {
  uint8_t ch = pwm->state->gpiote_ch;

  pbl_writel(1U, prv_gpiote_reg(high ? NRF52_GPIOTE_TASKS_SET_OFFSET(ch)
                                     : NRF52_GPIOTE_TASKS_CLR_OFFSET(ch)));
}

static int prv_apply(const struct pbl_pwm_controller *ctrl, uint8_t channel,
                     const struct pbl_pwm_state *state) {
  const struct pbl_pwm_nrf5_rtc *pwm = prv_pwm(ctrl);
  const struct pbl_pwm_nrf5_rtc_state *st = pwm->state;
  bool inversed = state->polarity == PBL_PWM_POLARITY_INVERSED;
  uint32_t set = NRF52_GPIOTE_TASKS_SET_OFFSET(st->gpiote_ch);
  uint32_t clr = NRF52_GPIOTE_TASKS_CLR_OFFSET(st->gpiote_ch);
  uint64_t period;
  uint64_t duty;

  period = ((uint64_t)state->period * NRF52_RTC_CLOCK_HZ) / PBL_NSEC_PER_SEC;
  if (period < 2U || period > RTC_COUNTER_MASK + 1ULL) {
    return -EINVAL;
  }

  duty = ((uint64_t)state->duty_cycle * NRF52_RTC_CLOCK_HZ) / PBL_NSEC_PER_SEC;

  prv_stop(pwm);

  if (!state->enabled || duty == 0U) {
    prv_hold(pwm, inversed);
    return 0;
  }

  if (duty >= period) {
    prv_hold(pwm, !inversed);
    return 0;
  }

  prv_hold(pwm, inversed);

  pbl_writel((uint32_t)period - 1U, prv_rtc_reg(pwm, NRF52_RTC_CC_OFFSET(CC_PERIOD)));
  pbl_writel((uint32_t)duty - 1U, prv_rtc_reg(pwm, NRF52_RTC_CC_OFFSET(CC_DUTY)));

  // The output turns active at the end of the period, as the counter is cleared, and inactive at
  // the end of the duty cycle
  soc_nrf_ppi_setup(st->ppi_ch[0], prv_rtc_reg(pwm, NRF52_RTC_EVENTS_COMPARE_OFFSET(CC_PERIOD)),
                    prv_gpiote_reg(inversed ? clr : set),
                    prv_rtc_reg(pwm, NRF52_RTC_TASKS_CLEAR_OFFSET));
  soc_nrf_ppi_setup(st->ppi_ch[1], prv_rtc_reg(pwm, NRF52_RTC_EVENTS_COMPARE_OFFSET(CC_DUTY)),
                    prv_gpiote_reg(inversed ? set : clr), 0U);
  soc_nrf_ppi_enable(PBL_BIT(st->ppi_ch[0]) | PBL_BIT(st->ppi_ch[1]));

  pbl_writel(1U, prv_rtc_reg(pwm, NRF52_RTC_TASKS_START_OFFSET));

  return 0;
}

int pbl_pwm_nrf5_rtc_init(const struct pbl_device *dev) {
  const struct pbl_pwm_nrf5_rtc *pwm = container_of(dev, const struct pbl_pwm_nrf5_rtc, ctrl.dev);
  struct pbl_pwm_nrf5_rtc_state *st = pwm->state;
  uint32_t pin;
  int ret;

  if (pwm->rtc >= ARRAY_LENGTH(s_rtc_bases)) {
    return -EINVAL;
  }

  if (pbl_device_init(&pwm->out.port->dev) != 0) {
    return -ENODEV;
  }

  ret = pbl_gpio_configure(&pwm->out, PBL_GPIO_OUTPUT_LOW);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_gpio_nrf5_gpiote_alloc(&st->gpiote_ch);
  if (ret != 0) {
    return ret;
  }

  ret = soc_nrf_ppi_alloc(&st->ppi_ch[0]);
  if (ret != 0) {
    return ret;
  }

  ret = soc_nrf_ppi_alloc(&st->ppi_ch[1]);
  if (ret != 0) {
    return ret;
  }

  pin = pbl_gpio_nrf5_pin(&pwm->out);
  pbl_writel(PBL_FIELD_PREP(GPIOTE_CONFIG_MODE_MASK, GPIOTE_CONFIG_MODE_TASK) |
                 PBL_FIELD_PREP(GPIOTE_CONFIG_PSEL_MASK, pin & 31U) |
                 ((pin >= 32U) ? GPIOTE_CONFIG_PORT : 0U),
             prv_gpiote_reg(NRF52_GPIOTE_CONFIG_OFFSET(st->gpiote_ch)));

  pbl_writel(1U, prv_rtc_reg(pwm, NRF52_RTC_TASKS_STOP_OFFSET));
  pbl_writel(0U, prv_rtc_reg(pwm, NRF52_RTC_PRESCALER_OFFSET));
  pbl_writel(UINT32_MAX, prv_rtc_reg(pwm, NRF52_RTC_INTENCLR_OFFSET));
  pbl_writel(RTC_EVT_COMPARE(CC_PERIOD) | RTC_EVT_COMPARE(CC_DUTY),
             prv_rtc_reg(pwm, NRF52_RTC_EVTEN_OFFSET));

  return 0;
}

const struct pbl_pwm_controller_ops pbl_pwm_nrf5_rtc_ops = {
  .apply = prv_apply,
};
