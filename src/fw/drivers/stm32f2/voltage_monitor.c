/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "drivers/voltage_monitor.h"
#include "kernel/util/delay.h"
#include "os/mutex.h"
#include "system/passert.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

// All boards use ADC1 solely for Vref, so we should never be using it for anything else.
#define VREF_ADC ADC1
#define VREF_ADC_CLOCK RCC_APB2Periph_ADC1

static PebbleMutex *s_adc_mutex;

void voltage_monitor_init(void) {
  s_adc_mutex = mutex_create();
}

void voltage_monitor_device_init(VoltageMonitorDevice *device) {
  gpio_analog_init(&device->input);
}

// It takes ~12µs to get our ADC readings. From time to time, we're busy
// processing elsewhere for upwards of 25µs and end up getting overrun issues.
// In the case that overrun occurs, clean the flag and return false so that we
// know to restart the sample group.
static bool prv_wait_for_conversion(ADC_TypeDef *ADCx) {
  while (ADC_GetFlagStatus(ADCx, ADC_FLAG_EOC) == RESET) {
    if (ADC_GetFlagStatus(ADCx, ADC_FLAG_OVR) == SET) {
      ADC_ClearFlag(ADCx, ADC_FLAG_OVR);
      return false;
    }
  }
  return true;
}

void voltage_monitor_read(VoltageMonitorDevice *device, VoltageReading *reading_out) {
  mutex_lock(s_adc_mutex);

  bool same_adc = (device->adc == VREF_ADC);

  // Enable ADC's APB interface clock
  periph_config_enable(VREF_ADC, VREF_ADC_CLOCK);
  if (!same_adc) {
    periph_config_enable(device->adc, device->clock_ctrl);
  }
  ADC_TempSensorVrefintCmd(ENABLE);

  // Common configuration (applicable for all ADCs)
  ADC_CommonInitTypeDef ADC_CommonInitStruct;
  ADC_CommonStructInit(&ADC_CommonInitStruct);
  // Single ADC mode
  ADC_CommonInitStruct.ADC_Mode = ADC_Mode_Independent;
  // ADCCLK = PCLK2/2
  ADC_CommonInitStruct.ADC_Prescaler = ADC_Prescaler_Div4;
  // Available only for multi ADC mode
  ADC_CommonInitStruct.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
  // Delay between 2 sampling phases
  ADC_CommonInitStruct.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
  ADC_CommonInit(&ADC_CommonInitStruct);

  ADC_InitTypeDef ADC_InitStruct;
  ADC_StructInit(&ADC_InitStruct);
  ADC_InitStruct.ADC_Resolution = ADC_Resolution_12b;
  ADC_InitStruct.ADC_ScanConvMode = same_adc ? ENABLE : DISABLE;
  ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;
  ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
  ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;
  ADC_InitStruct.ADC_NbrOfConversion = same_adc ? 2 : 1;

  ADC_Init(VREF_ADC, &ADC_InitStruct);
  if (!same_adc) {
    ADC_Init(device->adc, &ADC_InitStruct);
  }
  // Regular channel configuration
  ADC_RegularChannelConfig(VREF_ADC, ADC_Channel_Vrefint, 1, ADC_SampleTime_144Cycles);
  ADC_RegularChannelConfig(device->adc, device->adc_channel, same_adc ? 2 : 1,
                           ADC_SampleTime_144Cycles);
  if (same_adc) {
    // ScanConvMode enabled, so need to request EOC on each channel conversion
    ADC_EOCOnEachRegularChannelCmd(VREF_ADC, ENABLE);
  }

  ADC_Cmd(VREF_ADC, ENABLE);
  if (!same_adc) {
    ADC_Cmd(device->adc, ENABLE);
  }
  delay_us(10); // Tstab (ADC stabilization) needs 3us and temp sensor Tstart is 10us

  *reading_out = (VoltageReading) {};

  int i = 0;
  while (i < NUM_CONVERSIONS) {
    ADC_SoftwareStartConv(VREF_ADC);
    if (!prv_wait_for_conversion(VREF_ADC)) {
      continue;
    }
    uint32_t vref = ADC_GetConversionValue(VREF_ADC);

    ADC_SoftwareStartConv(device->adc);
    if (!prv_wait_for_conversion(device->adc)) {
      continue;
    }
    uint32_t vmon = ADC_GetConversionValue(device->adc);

    // Only save values and increment counter if both reads were successful
    reading_out->vref_total += vref;
    reading_out->vmon_total += vmon;
    ++i;
  }

  ADC_Cmd(VREF_ADC, DISABLE);
  if (!same_adc) {
    ADC_Cmd(device->adc, DISABLE);
  }
  ADC_TempSensorVrefintCmd(DISABLE);
  periph_config_disable(VREF_ADC, VREF_ADC_CLOCK);
  if (!same_adc) {
    periph_config_disable(device->adc, device->clock_ctrl);
  }

  mutex_unlock(s_adc_mutex);
}
