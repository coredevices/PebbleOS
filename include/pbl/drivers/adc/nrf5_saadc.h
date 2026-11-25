/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/adc.h>

/**
 * @defgroup drivers_adc_nrf5_saadc nRF5 SAADC
 * @ingroup drivers_adc
 * @brief @ref drivers_adc driver for the SAADC of the Nordic nRF52 series.
 *
 * Channels are the inputs of dt-bindings/iio/adc/nordic,nrf52832-saadc.h.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#define PBL_ADC_NRF5_SAADC__NUM_CHANNELS 10U

struct pbl_adc_nrf5_saadc {
  struct pbl_adc adc;
  uintptr_t base;
  // Where EasyDMA writes the result
  int16_t *result;
};

extern const struct pbl_adc_ops pbl_adc_nrf5_saadc_ops;
/** @endcond */

/**
 * @brief Define the SAADC.
 *
 * @param sym Symbol of the ADC.
 * @param _name Name.
 * @param _base Registers.
 */
#define PBL_ADC_NRF5_SAADC_DEFINE(sym, _name, _base)                         \
  PBL_ADC_STATE_DEFINE(sym);                                                 \
  static int16_t sym##_saadc_result;                                         \
  PBL_DEVICE_INSTANCE(sym, struct pbl_adc_nrf5_saadc, struct pbl_adc, adc) { \
    .adc = PBL_ADC_INIT(sym, _name, nullptr, &pbl_adc_nrf5_saadc_ops,        \
                        PBL_ADC_NRF5_SAADC__NUM_CHANNELS),                   \
    .base = (_base), .result = &sym##_saadc_result,                          \
  }

/** @} */
