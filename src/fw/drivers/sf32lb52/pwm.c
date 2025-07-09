/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "drivers/pwm.h"

#include "bf0_hal.h"
#include "board/board.h"
#include "system/passert.h"

#define MAX_PERIOD_GPT (0xFFFF)
#define MAX_PERIOD_ATM (0xFFFFFFFF)
#define MIN_PERIOD 3
#define MIN_PULSE 1

void pwm_set_duty_cycle(const PwmConfig *pwm, uint32_t duty_cycle) {
  PBL_ASSERTN(pwm != NULL);
  GPT_HandleTypeDef *htim = (GPT_HandleTypeDef *)&(pwm->handle);
  uint32_t period, pulse;
  uint32_t GPT_clock, psc;
  /* Converts the channel number to the channel number of Hal library */
  uint32_t channel = (pwm->state->channel - 1) << 2;
  uint32_t max_period = MAX_PERIOD_GPT;

#ifdef HAL_ATIM_MODULE_ENABLED
  if (IS_GPT_ADVANCED_INSTANCE(htim->Instance) != RESET) max_period = MAX_PERIOD_ATM;
#endif

#ifdef SF32LB52X
  if (htim->Instance == hwp_gptim2)
    GPT_clock = 24000000;
  else
#endif
    GPT_clock = HAL_RCC_GetPCLKFreq(htim->core, 1);

  /* Convert nanosecond to frequency and duty cycle. 1s = 1 * 1000 * 1000 * 1000 ns */
  GPT_clock /= 1000000UL;
  period = (unsigned long long)pwm->state->value * GPT_clock / 1000ULL;
  psc = period / max_period + 1;
  period = period / psc;
  __HAL_GPT_SET_PRESCALER(htim, psc - 1);

  if (period < MIN_PERIOD) {
    period = MIN_PERIOD;
  }
  __HAL_GPT_SET_AUTORELOAD(htim, period - 1);
  /* transfer cycle to ns*/
  pulse = duty_cycle * pwm->state->value / pwm->state->resolution;
  pulse = (unsigned long long)pulse * GPT_clock / psc / 1000ULL;

  if (pulse < MIN_PULSE) {
    pulse = MIN_PULSE;
  } else if (pulse >= period) {
  /*if pulse reach to 100%, need set pulse = period + 1, because pulse =
                                 period, the real percentage = 99.9983%  */
    pulse = period + 1;
  }
  __HAL_GPT_SET_COMPARE(htim, channel, pulse - 1);

  /* Update frequency value */
  HAL_GPT_GenerateEvent(htim, GPT_EVENTSOURCE_UPDATE);

  return;
}

void pwm_enable(const PwmConfig *pwm, bool enable) {
  PBL_ASSERTN(pwm != NULL);
  GPT_HandleTypeDef *htim = (GPT_HandleTypeDef *)&(pwm->handle);

  /* Converts the channel number to the channel number of Hal library */
  uint32_t channel = (pwm->state->channel - 1) << 2;

  if (enable) {
    GPT_OC_InitTypeDef oc_config = {0};
    oc_config.OCMode = GPT_OCMODE_PWM1;
    oc_config.Pulse = __HAL_GPT_GET_COMPARE(htim, channel);
    oc_config.OCPolarity = GPT_OCPOLARITY_HIGH;
    oc_config.OCFastMode = GPT_OCFAST_DISABLE;
    if (HAL_GPT_PWM_ConfigChannel(htim, &oc_config, channel) != HAL_OK) {
      PBL_LOG_D(LOG_DOMAIN_PWM, LOG_LEVEL_ERROR, "%x channel %d config failed", (unsigned int)htim,
                (int)pwm->state->channel);
      PBL_ASSERTN(0);
      return;
    }
    HAL_GPT_PWM_Start(htim, channel);
  } else {
      HAL_GPT_PWM_Stop(htim, channel);
  }

  return;
}

static int pwm_hw_init(const PwmConfig *pwm) {
  PBL_ASSERTN(pwm != NULL);
  GPT_HandleTypeDef *htim = (GPT_HandleTypeDef *)&(pwm->handle);
  GPT_ClockConfigTypeDef *clock_config = (GPT_ClockConfigTypeDef *)&pwm->clock_config;

  if (HAL_GPT_Base_Init(htim) != HAL_OK) {
    return -1;
  }

  if (HAL_GPT_ConfigClockSource(htim, clock_config) != HAL_OK) {
    return -1;
  }

  if (HAL_GPT_PWM_Init(htim) != HAL_OK) {
    return -1;
  }

  __HAL_GPT_URS_ENABLE(htim);
  
  return 0;
}

void pwm_hal_pins_set_gpio(const PwmConfig *pwm) {
  HAL_PIN_Set(pwm->pwm_pin.pad, pwm->pwm_pin.func, pwm->pwm_pin.flags, 1);
}

void pwm_init(const PwmConfig *pwm, uint32_t resolution, uint32_t frequency) {
  PBL_ASSERTN(pwm != NULL);

  if ((resolution == 0) || (frequency == 0)) return;

  pwm->state->resolution = resolution;
  pwm->state->value = 1000000000UL / (frequency);
  pwm_hal_pins_set_gpio(pwm);
  if (pwm_hw_init(pwm) != 0) {
    PBL_LOG_D(LOG_DOMAIN_PWM, LOG_LEVEL_ERROR, "PWM init failed");
    PBL_ASSERTN(0);
    return;
  }
  return;
}


