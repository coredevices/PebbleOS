/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/pwm.h>

PBL_DEVICE_CLASS_DEFINE(pbl_pwm_class, "pwm");

static int prv_check(const struct pbl_pwm *pwm) {
  if (!pbl_device_is_ready(&pwm->ctrl->dev)) {
    return -ENODEV;
  }

  if (pwm->channel >= pwm->ctrl->num_channels) {
    return -EINVAL;
  }

  return 0;
}

void pbl_pwm_get_state(const struct pbl_pwm *pwm, struct pbl_pwm_state *state) {
  *state = pwm->ctrl->states[pwm->channel];
}

void pbl_pwm_init_state(const struct pbl_pwm *pwm, struct pbl_pwm_state *state) {
  pbl_pwm_get_state(pwm, state);

  state->period = pwm->period;
  state->duty_cycle = 0U;
  state->polarity = ((pwm->flags & PBL_PWM_POLARITY_INVERTED) != 0U) ? PBL_PWM_POLARITY_INVERSED
                                                                     : PBL_PWM_POLARITY_NORMAL;
}

int pbl_pwm_set_relative_duty_cycle(struct pbl_pwm_state *state, uint32_t duty, uint32_t scale) {
  if (scale == 0U || duty > scale) {
    return -EINVAL;
  }

  state->duty_cycle = (uint32_t)(((uint64_t)state->period * duty + scale / 2U) / scale);

  return 0;
}

int pbl_pwm_apply(const struct pbl_pwm *pwm, const struct pbl_pwm_state *state) {
  struct pbl_pwm_state *cur;
  int ret = prv_check(pwm);

  if (ret != 0) {
    return ret;
  }

  if (state->period == 0U || state->duty_cycle > state->period ||
      (state->polarity != PBL_PWM_POLARITY_NORMAL &&
       state->polarity != PBL_PWM_POLARITY_INVERSED)) {
    return -EINVAL;
  }

  cur = &pwm->ctrl->states[pwm->channel];
  if (cur->period == state->period && cur->duty_cycle == state->duty_cycle &&
      cur->polarity == state->polarity && cur->enabled == state->enabled) {
    return 0;
  }

  ret = pwm->ctrl->ops->apply(pwm->ctrl, pwm->channel, state);
  if (ret != 0) {
    return ret;
  }

  *cur = *state;

  return 0;
}

int pbl_pwm_disable(const struct pbl_pwm *pwm) {
  struct pbl_pwm_state state;
  int ret = prv_check(pwm);

  if (ret != 0) {
    return ret;
  }

  pbl_pwm_get_state(pwm, &state);
  if (state.period == 0U) {
    return 0;
  }

  state.enabled = false;

  return pbl_pwm_apply(pwm, &state);
}
