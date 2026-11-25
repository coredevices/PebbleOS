/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/pwm/sf32lb_gptim.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/sf32lb/idle.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/units.h>

#include <hardware/sf32lb52_gptim.h>

static const struct pbl_pwm_sf32lb_gptim *prv_gptim(const struct pbl_pwm_controller *ctrl) {
  return PBL_CONTAINER_OF(ctrl, const struct pbl_pwm_sf32lb_gptim, ctrl);
}

static inline uint32_t prv_getreg(const struct pbl_pwm_sf32lb_gptim *gptim, uint32_t offset) {
  return pbl_readl(gptim->base + offset);
}

static inline void prv_putreg(const struct pbl_pwm_sf32lb_gptim *gptim, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, gptim->base + offset);
}

static void prv_set_mode(const struct pbl_pwm_sf32lb_gptim *gptim, uint8_t ch, uint32_t ocm) {
  pbl_modifyl(GPTIM_CCMR_CCS_MASK(ch) | GPTIM_CCMR_OCM_MASK(ch) | GPTIM_CCMR_OCPE(ch),
              PBL_FIELD_PREP(GPTIM_CCMR_OCM_MASK(ch), ocm) | GPTIM_CCMR_OCPE(ch),
              gptim->base + SF32LB52_GPTIM_CCMR_OFFSET(ch));
}

static int prv_apply(const struct pbl_pwm_controller *ctrl, uint8_t channel,
                     const struct pbl_pwm_state *state) {
  const struct pbl_pwm_sf32lb_gptim *gptim = prv_gptim(ctrl);
  struct pbl_pwm_sf32lb_gptim_state *st = gptim->state;
  uint32_t ccp = (state->polarity == PBL_PWM_POLARITY_INVERSED) ? GPTIM_CCER_CCP(channel) : 0U;
  uint64_t cycles;
  uint64_t freq;
  uint32_t psc = 0U;
  uint32_t arr = 0U;
  uint32_t ccr = 0U;
  uint8_t others;
  int ret;

  if (channel >= GPTIM_NUM_CHANNELS) {
    return -EINVAL;
  }

  if (state->enabled) {
    ret = pbl_clk_get_rate(&gptim->clk, &freq);
    if (ret != 0) {
      return ret;
    }
    freq /= 1000U;

    cycles = ((uint64_t)state->period * freq) / PBL_NSEC_PER_SEC;
    psc = (uint32_t)PBL_DIV_ROUND_UP(cycles, GPTIM_ARR_MASK);
    if (psc == 0U) {
      psc = 1U;
    }
    if (psc > GPTIM_PSC_MASK + 1U) {
      return -EINVAL;
    }

    arr = (uint32_t)(cycles / psc);
    if (arr < 2U) {
      return -EINVAL;
    }

    ccr = (uint32_t)PBL_MIN((((uint64_t)state->duty_cycle * freq) / PBL_NSEC_PER_SEC) / psc, arr);
  }

  pbl_irq_lock();

  others = st->running & ~PBL_BIT(channel);

  if (!state->enabled) {
    prv_set_mode(gptim, channel, GPTIM_CCMR_OCM_FORCE_LOW);
    pbl_modifyl(GPTIM_CCER_CCP(channel), ccp, gptim->base + SF32LB52_GPTIM_CCER_OFFSET);
    pbl_modifyl(0U, GPTIM_CCER_CCE(channel), gptim->base + SF32LB52_GPTIM_CCER_OFFSET);
    if (others == 0U && st->running != 0U) {
      prv_putreg(gptim, SF32LB52_GPTIM_CR1_OFFSET, 0U);
      pbl_idle_state_release(SOC_SF32LB_IDLE_DEEPSLEEP);
    }
    st->running = others;
    pbl_irq_unlock();
    return 0;
  }

  if (others != 0U && (psc != st->psc || arr != st->arr)) {
    pbl_irq_unlock();
    return -EBUSY;
  }

  pbl_modifyl(GPTIM_CCER_CCP(channel), ccp, gptim->base + SF32LB52_GPTIM_CCER_OFFSET);
  prv_putreg(gptim, SF32LB52_GPTIM_CCR_OFFSET(channel), ccr);
  prv_set_mode(gptim, channel, GPTIM_CCMR_OCM_PWM1);
  pbl_modifyl(0U, GPTIM_CCER_CCE(channel), gptim->base + SF32LB52_GPTIM_CCER_OFFSET);

  // A new period restarts the count, a new duty cycle waits for the end of the period
  if (st->running == 0U || psc != st->psc || arr != st->arr) {
    prv_putreg(gptim, SF32LB52_GPTIM_CR1_OFFSET, GPTIM_CR1_ARPE);
    prv_putreg(gptim, SF32LB52_GPTIM_PSC_OFFSET, psc - 1U);
    prv_putreg(gptim, SF32LB52_GPTIM_ARR_OFFSET, arr - 1U);
    prv_putreg(gptim, SF32LB52_GPTIM_EGR_OFFSET, GPTIM_EGR_UG);
    prv_putreg(gptim, SF32LB52_GPTIM_CR1_OFFSET, GPTIM_CR1_ARPE | GPTIM_CR1_CEN);
  }

  // PCLK stops in deep sleep, which would freeze the outputs
  if (st->running == 0U) {
    pbl_idle_state_block(SOC_SF32LB_IDLE_DEEPSLEEP);
  }

  st->running = others | PBL_BIT(channel);
  st->psc = psc;
  st->arr = arr;

  pbl_irq_unlock();

  return 0;
}

int pbl_pwm_sf32lb_gptim_init(const struct pbl_device *dev) {
  const struct pbl_pwm_sf32lb_gptim *gptim =
      PBL_CONTAINER_OF(dev, const struct pbl_pwm_sf32lb_gptim, ctrl.dev);
  int ret;

  ret = pbl_device_init(&gptim->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&gptim->clk);
  if (ret != 0) {
    return ret;
  }

  prv_putreg(gptim, SF32LB52_GPTIM_CR1_OFFSET, 0U);
  prv_putreg(gptim, SF32LB52_GPTIM_CCER_OFFSET, 0U);

  // Outputs start at their inactive level
  for (uint8_t ch = 0U; ch < GPTIM_NUM_CHANNELS; ch++) {
    prv_set_mode(gptim, ch, GPTIM_CCMR_OCM_FORCE_LOW);
  }

  return pbl_pinctrl_select_state(&gptim->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
}

const struct pbl_pwm_controller_ops pbl_pwm_sf32lb_gptim_ops = {
  .apply = prv_apply,
};

#include PBL_DT_INSTANCES(sifli_sf32lb52_gptim_pwm)
