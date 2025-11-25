/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! nPM1300 configuration
typedef struct {
  //! Charge current (32-800mA, 2mA steps)
  uint16_t chg_current_ma;
  //! Discharge limit (200mA or 1000mA)
  uint16_t dischg_limit_ma;
  //! Termination current (% of charge current, 10 or 20%)
  uint8_t term_current_pct;
  //! Thermistor beta value
  uint16_t thermistor_beta;
  //! Vbus current limite0
  uint16_t vbus_current_lim0;
  //! Vbus current limite startup
  uint16_t vbus_current_startup;
} Npm1300Config;

typedef enum {
  Npm1300_Gpio0,
  Npm1300_Gpio1,
  Npm1300_Gpio2,
  Npm1300_Gpio3,
  Npm1300_Gpio4,
}Npm1300GpioId_t;

//! nPM1300 ops
typedef struct {
  bool (*gpio_set)(Npm1300GpioId_t id, bool is_high);
}Npm1300Ops_t;

extern Npm1300Ops_t NPM1300_OPS;