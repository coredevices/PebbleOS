/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "drivers/pwm.h"
#include "drivers/timer.h"

#define STM32F7_COMPATIBLE
#include <mcu.h>

#define IS_LPTIM(periph) IS_LPTIM_ALL_PERIPH((LPTIM_TypeDef *)(periph))

void pwm_init(const PwmConfig *pwm, uint32_t resolution, uint32_t frequency) {
  periph_config_enable(pwm->timer.peripheral, pwm->timer.config_clock);

  if (IS_LPTIM(pwm->timer.peripheral)) {
    // Initialize low power timer
    LPTIM_InitTypeDef config = {
      .LPTIM_ClockSource = LPTIM_ClockSource_APBClock_LPosc,
      .LPTIM_Prescaler = LPTIM_Prescaler_DIV128,
      .LPTIM_Waveform = LPTIM_Waveform_PWM_OnePulse,
      // low polarity means it'll be high for the specified duty cycle
      .LPTIM_OutputPolarity = LPTIM_OutputPolarity_Low
    };
    LPTIM_Init(pwm->timer.lp_peripheral, &config);
    LPTIM_SelectSoftwareStart(pwm->timer.lp_peripheral);
    // The timer must be enabled before setting the auto-reload value.
    LPTIM_Cmd(pwm->timer.lp_peripheral, ENABLE);
    LPTIM_SetAutoreloadValue(pwm->timer.lp_peripheral, resolution);
    // Wait for the Auto-Reload value to be applied before disabling the timer.
#if !TARGET_QEMU
    while (LPTIM_GetFlagStatus(pwm->timer.lp_peripheral,
                               LPTIM_FLAG_ARROK) == RESET) continue;
#endif
    LPTIM_Cmd(pwm->timer.lp_peripheral, DISABLE);
  } else {
    // Initialize regular timer
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
  }

  periph_config_disable(pwm->timer.peripheral, pwm->timer.config_clock);
}

void pwm_set_duty_cycle(const PwmConfig *pwm, uint32_t duty_cycle) {
  if (IS_LPTIM(pwm->timer.peripheral)) {
    LPTIM_SetCompareValue(pwm->timer.lp_peripheral, duty_cycle);
    LPTIM_SelectOperatingMode(pwm->timer.lp_peripheral, LPTIM_Mode_Continuous);
  } else {
    TIM_OCInitTypeDef tim_oc_init;
    TIM_OCStructInit(&tim_oc_init);
    tim_oc_init.TIM_OCMode = TIM_OCMode_PWM1;
    tim_oc_init.TIM_OutputState = TIM_OutputState_Enable;
    tim_oc_init.TIM_Pulse = duty_cycle;
    tim_oc_init.TIM_OCPolarity = TIM_OCPolarity_High;
    pwm->timer.init(pwm->timer.peripheral, &tim_oc_init);
  }
}

void pwm_enable(const PwmConfig *pwm, bool enable) {
  if (enable) {
    gpio_af_init(&pwm->afcfg, GPIO_OType_PP, GPIO_Speed_100MHz, GPIO_PuPd_DOWN);
    periph_config_enable(pwm->timer.peripheral, pwm->timer.config_clock);
  } else {
    periph_config_disable(pwm->timer.peripheral, pwm->timer.config_clock);
    gpio_output_init(&pwm->output, GPIO_OType_PP, GPIO_Speed_100MHz);
    // The ".active_high" attribute determines the idle state of the PWM output
    gpio_output_set(&pwm->output, false /* force low */);
  }

  const FunctionalState state = (enable) ? ENABLE : DISABLE;
  if (IS_LPTIM(pwm->timer.peripheral)) {
    LPTIM_Cmd(pwm->timer.lp_peripheral, state);
  } else {
    TIM_Cmd(pwm->timer.peripheral, state);
  }
}
