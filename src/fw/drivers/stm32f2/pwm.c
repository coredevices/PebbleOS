/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "drivers/pwm.h"
#include "drivers/timer.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#include <mcu.h>

void pwm_init(const PwmConfig *pwm, uint32_t resolution, uint32_t frequency) {
  periph_config_enable(pwm->timer.peripheral, pwm->timer.config_clock);

  // Initialize PWM Timer
  TIM_TimeBaseInitTypeDef tim_config;
  TIM_TimeBaseStructInit(&tim_config);
  tim_config.TIM_Period = resolution - 1;
  tim_config.TIM_Prescaler = timer_find_prescaler(&pwm->timer, frequency);
  tim_config.TIM_CounterMode = TIM_CounterMode_Up;
  tim_config.TIM_ClockDivision = 0;
  TIM_TimeBaseInit(pwm->timer.peripheral, &tim_config);

  // PWM Mode configuration
  TIM_OCInitTypeDef tim_oc_init;
  TIM_OCStructInit(&tim_oc_init);
  tim_oc_init.TIM_OCMode = TIM_OCMode_PWM1;
  tim_oc_init.TIM_OutputState = TIM_OutputState_Enable;
  tim_oc_init.TIM_Pulse = 0;
  tim_oc_init.TIM_OCPolarity = TIM_OCPolarity_High;
  pwm->timer.init(pwm->timer.peripheral, &tim_oc_init);

  pwm->timer.preload(pwm->timer.peripheral, TIM_OCPreload_Enable);
  TIM_ARRPreloadConfig(pwm->timer.peripheral, ENABLE);

  periph_config_disable(pwm->timer.peripheral, pwm->timer.config_clock);
}

void pwm_set_duty_cycle(const PwmConfig *pwm, uint32_t duty_cycle) {
  TIM_OCInitTypeDef  tim_oc_init;
  TIM_OCStructInit(&tim_oc_init);
  tim_oc_init.TIM_OCMode = TIM_OCMode_PWM1;
  tim_oc_init.TIM_OutputState = TIM_OutputState_Enable;
  tim_oc_init.TIM_Pulse = duty_cycle;
  tim_oc_init.TIM_OCPolarity = TIM_OCPolarity_High;
  pwm->timer.init(pwm->timer.peripheral, &tim_oc_init);
}

void pwm_enable(const PwmConfig *pwm, bool enable) {
  if (enable) {
    gpio_af_init(&pwm->afcfg, GPIO_OType_PP, GPIO_Speed_100MHz, GPIO_PuPd_DOWN);
    periph_config_enable(pwm->timer.peripheral, pwm->timer.config_clock);
  } else {
    periph_config_disable(pwm->timer.peripheral, pwm->timer.config_clock);
    gpio_output_init(&pwm->output, GPIO_OType_PP, GPIO_Speed_100MHz);
    gpio_output_set(&pwm->output, false /* force low */);
  }

  const FunctionalState state = (enable) ? ENABLE : DISABLE;
  TIM_Cmd(pwm->timer.peripheral, state);
}
