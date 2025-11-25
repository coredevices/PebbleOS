/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include "board/board.h"
#include "drivers/pwm.h"
#include "system/passert.h"
#include "kernel/util/stop.h"

#include "bf0_hal_tim.h"

#define MAX_PERIOD_GPT 0xFFFFU
#define MAX_PERIOD_ATM 0xFFFFFFFFU
#define MIN_PERIOD 3U
#define MIN_PULSE 1U

void pwm_set_duty_cycle(const PwmConfig *pwm, uint32_t duty_cycle) {
  GPT_HandleTypeDef *htim = &pwm->state->handle;
  uint32_t period, pulse;
  uint32_t gpt_clock, psc;
  uint32_t channel;
  uint32_t max_period;
  HAL_StatusTypeDef ret;

  // converts the channel number to the channel number of HAL library
  channel = (pwm->state->channel - 1U) << 2U;

  if (IS_GPT_ADVANCED_INSTANCE(htim->Instance) != RESET) {
    max_period = MAX_PERIOD_ATM;
  } else {
    max_period = MAX_PERIOD_GPT;
  }

  if (htim->Instance == hwp_gptim2) {
    gpt_clock = 24000000U;
  } else {
    gpt_clock = HAL_RCC_GetPCLKFreq(htim->core, 1);
  }

  // Convert nanosecond to frequency and duty cycle. 1s = 1 * 1000 * 1000 * 1000 ns
  gpt_clock /= 1000000UL;
  period = (uint64_t)pwm->state->value * gpt_clock / 1000ULL;
  psc = period / max_period + 1U;
  period = period / psc;
  __HAL_GPT_SET_PRESCALER(htim, psc - 1U);

  if (period < MIN_PERIOD) {
    period = MIN_PERIOD;
  }
  __HAL_GPT_SET_AUTORELOAD(htim, period - 1U);

  // transfer cycle to ns
  pulse = duty_cycle * pwm->state->value / pwm->state->resolution;
  pulse = (uint64_t)pulse * gpt_clock / psc / 1000ULL;

  if (pulse < MIN_PULSE) {
    pulse = MIN_PULSE;
  } else if (pulse >= period) {
    // if pulse reach to 100%, need set pulse = period + 1, because pulse =
    // period, the real percentage = 99.9983%
    pulse = period + 1U;
  }
  __HAL_GPT_SET_COMPARE(htim, channel, pulse - 1);

  // Update frequency value
  ret = HAL_GPT_GenerateEvent(htim, GPT_EVENTSOURCE_UPDATE);
  PBL_ASSERTN(ret == HAL_OK);
}

void pwm_enable(const PwmConfig *pwm, bool enable) {
  GPT_HandleTypeDef *htim = &pwm->state->handle;
  HAL_StatusTypeDef ret;
  uint32_t channel;

  /* Converts the channel number to the channel number of Hal library */
  channel = (pwm->state->channel - 1U) << 2U;

  if (enable) {
    GPT_OC_InitTypeDef oc_config = {0};

    oc_config.OCMode = GPT_OCMODE_PWM1;
    oc_config.Pulse = __HAL_GPT_GET_COMPARE(htim, channel);
    oc_config.OCPolarity = GPT_OCPOLARITY_HIGH;
    oc_config.OCFastMode = GPT_OCFAST_DISABLE;

    ret = HAL_GPT_PWM_ConfigChannel(htim, &oc_config, channel);
    PBL_ASSERTN(ret == HAL_OK);

    ret = HAL_GPT_PWM_Start(htim, channel);
    PBL_ASSERTN(ret == HAL_OK);

    if (!pwm->state->enabled) {
      pwm->state->enabled = true;
      stop_mode_disable(InhibitorPWM);
    }
  } else {
    ret = HAL_GPT_PWM_Stop(htim, channel);
    PBL_ASSERTN(ret == HAL_OK);

    if (pwm->state->enabled) {
      pwm->state->enabled = false;
      stop_mode_enable(InhibitorPWM);
    }
  }
}

void pwm_init(const PwmConfig *pwm, uint32_t resolution, uint32_t frequency) {
  GPT_HandleTypeDef *htim = &pwm->state->handle;
  GPT_ClockConfigTypeDef *clock_config = &pwm->state->clock_config;
  HAL_StatusTypeDef ret;

  PBL_ASSERTN((resolution != 0U) && (frequency != 0U));

  pwm->state->resolution = resolution;
  pwm->state->value = 1000000000UL / (frequency);
  
  HAL_PIN_Set(pwm->pwm_pin.pad, pwm->pwm_pin.func, pwm->pwm_pin.flags, 1);

  ret = HAL_GPT_Base_Init(htim);
  PBL_ASSERTN(ret == HAL_OK);

  ret = HAL_GPT_ConfigClockSource(htim, clock_config);
  PBL_ASSERTN(ret == HAL_OK);

  ret = HAL_GPT_PWM_Init(htim);
  PBL_ASSERTN(ret == HAL_OK);

  __HAL_GPT_URS_ENABLE(htim);
}
