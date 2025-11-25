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
#include <mcu.h>

static PebbleMutex *s_adc_mutex;

void voltage_monitor_init(void) {
  s_adc_mutex = mutex_create();
}

void voltage_monitor_device_init(const VoltageMonitorDevice *device) {
  gpio_analog_init(&device->input);
}

//! It takes ~12µs to get our ADC readings. From time to time, we're busy
//! processing elsewhere for upwards of 25µs and end up getting overrun issues.
//!
//! When OVR occurs, we clear both the OVR flag and the EOC flag.
//! The OVR flag always needs to be cleared so that conversion can be restarted.
//!
//!
//! For the first conversion, it is possible that OVR can occur between
//! seeing EOC being set and then actually reading the conversion value. When
//! that occurs, we will catch the OVR when waiting for the next conversion, and
//! restart the group. In this case, it is mandatory to clear the EOC, so that
//! we can restart the conversion group. Clearing EOC on OVR is always safe when
//! using only two channels since clearing EOC will not start a new conversion.
//!
//! If we make it to the last conversion without seeing OVR, then we know that
//! no OVR will occur and we don't need to worry about overrun before reading
//! the data back.
static bool prv_wait_for_conversion(void) {
  while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET) {
    if (ADC_GetFlagStatus(ADC1, ADC_FLAG_OVR) == SET) {
      ADC_ClearFlag(ADC1, ADC_FLAG_OVR);
      ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
      return false;
    }
  }
  return true;
}

void voltage_monitor_read(const VoltageMonitorDevice *device, VoltageReading *reading_out) {
  PBL_ASSERTN(device->adc == ADC1);
  mutex_lock(s_adc_mutex);

  periph_config_enable(ADC1, RCC_APB2Periph_ADC1);
  ADC_TempSensorVrefintCmd(ENABLE);

  ADC_CommonInitTypeDef ADC_CommonInitStruct;
  // Single ADC mode
  ADC_CommonStructInit(&ADC_CommonInitStruct);
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
  // Scan multiple channels on ADC1
  ADC_InitStruct.ADC_ScanConvMode = ENABLE;
  ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;
  ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
  ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;
  ADC_InitStruct.ADC_NbrOfConversion = 2;

  ADC_Init(ADC1, &ADC_InitStruct);
  ADC_RegularChannelConfig(ADC1, ADC_Channel_Vrefint, 1, ADC_SampleTime_144Cycles);
  ADC_RegularChannelConfig(ADC1, device->adc_channel, 2, ADC_SampleTime_144Cycles);
  // ScanConvMode enabled, so need to request EOC on each channel conversion
  ADC_EOCOnEachRegularChannelCmd(ADC1, ENABLE);

  ADC_Cmd(ADC1, ENABLE);

  delay_us(3); // Wait Tstab = 3us for ADC to stabilize

  *reading_out = (VoltageReading) {};

  int i = 0;
  while (i < NUM_CONVERSIONS) {
    ADC_SoftwareStartConv(ADC1); // Restart the conversion group
    if (!prv_wait_for_conversion()) {
      continue;
    }
    const uint16_t vref = ADC_GetConversionValue(ADC1);

    if (!prv_wait_for_conversion()) {
      continue;
    }
    const uint16_t vmon = ADC_GetConversionValue(ADC1);

    // Only save values and increment counter if both reads were successful
    reading_out->vref_total += vref;
    reading_out->vmon_total += vmon;
    ++i;
  }

  ADC_Cmd(ADC1, DISABLE);
  ADC_TempSensorVrefintCmd(DISABLE);
  periph_config_disable(ADC1, RCC_APB2Periph_ADC1);

  mutex_unlock(s_adc_mutex);
}
