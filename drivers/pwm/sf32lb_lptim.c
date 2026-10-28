/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/pwm/sf32lb_lptim.h>
#include <pbl/drivers/sf32lb52/rc10k.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/units.h>

#include <hardware/sf32lb52_lptim.h>

// A few RC10K cycles
#define COUNTRST_TIMEOUT_US 1000U

static const uintptr_t s_bases[] = {
  SF32LB52_LPTIM1_BASE,
  SF32LB52_LPTIM2_BASE,
};

static const struct pbl_pwm_sf32lb_lptim *prv_lptim(const struct pbl_pwm_controller *ctrl) {
  return container_of(ctrl, const struct pbl_pwm_sf32lb_lptim, ctrl);
}

static inline uint32_t prv_getreg(const struct pbl_pwm_sf32lb_lptim *lptim, uint32_t offset) {
  return pbl_readl(s_bases[lptim->index - 1U] + offset);
}

static inline void prv_putreg(const struct pbl_pwm_sf32lb_lptim *lptim, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, s_bases[lptim->index - 1U] + offset);
}

static int prv_apply(const struct pbl_pwm_controller *ctrl, uint8_t channel,
                     const struct pbl_pwm_state *state) {
  const struct pbl_pwm_sf32lb_lptim *lptim = prv_lptim(ctrl);
  uint64_t freq = rc10k_get_freq_hz();
  uint32_t period;
  uint32_t duty;
  uint32_t cfgr;
  int ret;

  period = (uint32_t)MIN((state->period * freq) / PBL_NSEC_PER_SEC, LPTIM_ARR_MASK + 1ULL);
  if (period < 2U) {
    return -EINVAL;
  }

  duty = (uint32_t)MIN((state->duty_cycle * freq) / PBL_NSEC_PER_SEC, period);

  // Running, the output is high up to the compare value with WAVPOL set and low with it
  // cleared; stopped, it is at the WAVPOL level
  if (!state->enabled) {
    prv_putreg(lptim, SF32LB52_LPTIM_CR_OFFSET, 0U);
    prv_putreg(lptim, SF32LB52_LPTIM_CFGR_OFFSET,
               (state->polarity == PBL_PWM_POLARITY_INVERSED) ? LPTIM_CFGR_WAVPOL : 0U);
    return 0;
  }

  cfgr = (state->polarity == PBL_PWM_POLARITY_NORMAL) ? LPTIM_CFGR_WAVPOL : 0U;

  if ((prv_getreg(lptim, SF32LB52_LPTIM_CR_OFFSET) & LPTIM_CR_ENABLE) != 0U &&
      prv_getreg(lptim, SF32LB52_LPTIM_CFGR_OFFSET) == cfgr &&
      prv_getreg(lptim, SF32LB52_LPTIM_ARR_OFFSET) == period - 1U) {
    prv_putreg(lptim, SF32LB52_LPTIM_CMP_OFFSET, duty);
    return 0;
  }

  // Restart from 0, as a counter past a smaller period would run to its maximum first
  prv_putreg(lptim, SF32LB52_LPTIM_CR_OFFSET, 0U);
  prv_putreg(lptim, SF32LB52_LPTIM_CFGR_OFFSET, cfgr);
  prv_putreg(lptim, SF32LB52_LPTIM_CR_OFFSET, LPTIM_CR_COUNTRST);
  ret = PBL_POLL_BUSY_US((prv_getreg(lptim, SF32LB52_LPTIM_CR_OFFSET) & LPTIM_CR_COUNTRST) == 0U,
                         10U, COUNTRST_TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  prv_putreg(lptim, SF32LB52_LPTIM_CR_OFFSET, LPTIM_CR_ENABLE);
  prv_putreg(lptim, SF32LB52_LPTIM_ARR_OFFSET, period - 1U);
  prv_putreg(lptim, SF32LB52_LPTIM_CMP_OFFSET, duty);
  prv_putreg(lptim, SF32LB52_LPTIM_CR_OFFSET, LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT);

  return 0;
}

int pbl_pwm_sf32lb_lptim_init(const struct pbl_device *dev) {
  const struct pbl_pwm_sf32lb_lptim *lptim =
      container_of(dev, const struct pbl_pwm_sf32lb_lptim, ctrl.dev);

  if (lptim->index == 0U || lptim->index > ARRAY_LENGTH(s_bases)) {
    return -EINVAL;
  }

  prv_putreg(lptim, SF32LB52_LPTIM_CR_OFFSET, 0U);
  prv_putreg(lptim, SF32LB52_LPTIM_CFGR_OFFSET, 0U);

  return pbl_pinctrl_select_state(&lptim->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
}

const struct pbl_pwm_controller_ops pbl_pwm_sf32lb_lptim_ops = {
  .apply = prv_apply,
};
