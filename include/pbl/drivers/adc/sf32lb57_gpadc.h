/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/adc.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/nvmem.h>

/**
 * @defgroup drivers_adc_sf32lb57_gpadc SF32LB57 GPADC
 * @ingroup drivers_adc
 * @brief @ref drivers_adc driver for the GPADC of the SF32LB57.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#define PBL_ADC_SF32LB57_GPADC__NUM_CHANNELS 12U

// The transfer, as two points: a raw value and its voltage
struct pbl_adc_sf32lb57_gpadc_state {
  int32_t raw_lo;
  int32_t uv_lo;
  int32_t raw_hi;
  int32_t uv_hi;
};

struct pbl_adc_sf32lb57_gpadc {
  struct pbl_adc adc;
  uintptr_t base;
  uintptr_t cfg;
  struct pbl_clk clk;
  struct pbl_nvmem_cell cal;
  struct pbl_adc_sf32lb57_gpadc_state *state;
};

extern const struct pbl_adc_ops pbl_adc_sf32lb57_gpadc_ops;
/** @endcond */

/**
 * @brief Define the SF32LB57 GPADC.
 *
 * @param sym Symbol of the ADC.
 * @param _name Name.
 * @param _base Registers.
 * @param _cfg HPSYS_CFG registers.
 * @param _clk Clock, from PBL_CLK().
 * @param _cal Calibration cell, from PBL_NVMEM_CELL(), or {} without one.
 */
#define PBL_ADC_SF32LB57_GPADC_DEFINE(sym, _name, _base, _cfg, _clk, _cal)                 \
  PBL_ADC_STATE_DEFINE(sym);                                                               \
  static struct pbl_adc_sf32lb57_gpadc_state sym##_gpadc_state;                            \
  PBL_DEVICE_INSTANCE(sym, struct pbl_adc_sf32lb57_gpadc, struct pbl_adc, adc) {           \
    .adc = PBL_ADC_INIT(sym, _name, nullptr, &pbl_adc_sf32lb57_gpadc_ops,                  \
                        PBL_ADC_SF32LB57_GPADC__NUM_CHANNELS),                             \
    .base = (_base), .cfg = (_cfg), .clk = _clk, .cal = _cal, .state = &sym##_gpadc_state, \
  }

/** @} */
