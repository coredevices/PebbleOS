/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include "board/board.h"

/**
 * @defgroup drivers_voltage_monitor Voltage monitor
 * @ingroup drivers
 * @brief ADC-based voltage monitoring.
 * @{
 */

/** @brief Number of ADC samples summed into each @ref VoltageReading total. */
#define NUM_CONVERSIONS 40

#if defined(CONFIG_SOC_NRF52)
#include <hal/nrf_saadc.h>

/** @brief Voltage monitor ADC input. */
typedef const struct VoltageMonitorDevice {
  /** SAADC instance. */
  NRF_SAADC_Type *const adc;
  /** ADC channel. */
  const uint8_t adc_channel;
  /** Analog input. */
  const nrf_saadc_input_t input;
} VoltageMonitorDevice;

#elif defined(CONFIG_SOC_SF32LB52) || defined(CONFIG_QEMU)

/** @brief Voltage monitor ADC input (unused on this platform). */
typedef const struct VoltageMonitorDevice {
} VoltageMonitorDevice;

#else

/** @brief Voltage monitor ADC input. */
typedef const struct VoltageMonitorDevice {
  /** ADC instance, for example ADC1. */
  ADC_TypeDef *const adc;
  /** ADC channel, one of ADC_Channel_*. */
  const uint8_t adc_channel;
  /** Peripheral clock control flag. */
  uint32_t clock_ctrl;
  /** Input pin. */
  const InputConfig input;
} VoltageMonitorDevice;

#endif

/**
 * @brief Voltage reading.
 *
 * Each total is the sum of @ref NUM_CONVERSIONS samples in the range [0, 4095].
 */
typedef struct {
  /** Sum of the monitored input samples. */
  uint32_t vmon_total;
  /** Sum of the internal reference samples. */
  uint32_t vref_total;
} VoltageReading;

/** @brief Initialize the voltage monitor. */
void voltage_monitor_init(void);
/**
 * @brief Initialize a voltage monitor input.
 *
 * @param device Input.
 */
void voltage_monitor_device_init(const VoltageMonitorDevice *device);

/**
 * @brief Read a voltage monitor input.
 *
 * @param device Input.
 * @param[out] reading_out Reading.
 */
void voltage_monitor_read(const VoltageMonitorDevice *device, VoltageReading *reading_out);
/**
 * @brief Read the internal temperature sensor through a voltage monitor ADC.
 *
 * @param device Input whose ADC is used.
 * @param[out] reading_out Reading.
 */
void voltage_monitor_read_temp(const VoltageMonitorDevice *device, VoltageReading *reading_out);

/** @} */
