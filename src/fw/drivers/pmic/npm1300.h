/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Vterm setting
typedef enum {
  NPM1300_VTERM_4V00 = 0x4U,
  NPM1300_VTERM_4V20 = 0x8U,
  NPM1300_VTERM_4V35 = 0xBU,
  NPM1300_VTERM_4V45 = 0xDU,
} Npm1300Vterm_t;

//! LDO2 mode
typedef enum {
  NPM1300_LDO2_MODE_LDSW = 0,
  NPM1300_LDO2_MODE_LDO = 1,
} Npm1300Ldo2Mode_t;


//! nPM1300 configuration
typedef struct {
  
  //! Vterm setting
  Npm1300Vterm_t vterm_setting;

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

  //! Buck1 voltage (0 = disabled)
  uint8_t buck1_voltage_sel;
  //! Buck2 voltage (0 = disabled)
  uint8_t buck2_voltage_sel;
  //! Buck SW control selection
  uint8_t buck_sw_ctrl_sel;
  //! Configure Buck SW control (even if 0)
  bool configure_buck_sw_ctrl;
  //! Enable Buck1
  bool buck1_enable;
  //! Enable Buck2
  bool buck2_enable;

  //! Apply Erratum 27 workaround
  //: I assume this is a specific sequence on startup
  bool apply_erratum_27_workaround;

  //! LDSW1 mode (LDO or Load Switch)
  Npm1300Ldo2Mode_t ldsw1_mode;
  //! LDSW1 voltage selection
  uint8_t ldsw1_voltage_sel;
  //! Enable LDSW1
  bool ldsw1_enable;

  //! LDSW2 mode (LDO or Load Switch)
  Npm1300Ldo2Mode_t ldsw2_mode;
  //! LDSW2 voltage selection
  uint8_t ldsw2_voltage_sel;
  //! Enable LDSW2
  bool ldsw2_enable;
} Npm1300Config;

typedef enum {
  Npm1300_Gpio0,
  Npm1300_Gpio1,
  Npm1300_Gpio2,
  Npm1300_Gpio3,
  Npm1300_Gpio4,
}Npm1300GpioId_t;

//! Maximum discharge current in mA
#define NPM1300_DISCHG_LIMIT_MA_MAX 1000UL

//! nPM1300 ops
typedef struct {
  bool (*gpio_set)(Npm1300GpioId_t id, bool is_high);
  bool (*ldo2_set_enabled)(bool enabled);
  bool (*dischg_limit_ma_set)(uint32_t ilim_ma);
}Npm1300Ops_t;

extern Npm1300Ops_t NPM1300_OPS;