/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup drivers_pmic_npm1300 nPM1300
 * @ingroup drivers_pmic
 * @brief Nordic nPM1300 PMIC configuration and extra operations.
 *
 * The board provides an @ref Npm1300Config named @c NPM1300_CONFIG.
 *
 * @code{.c}
 * NPM1300_OPS.ldo2_set_enabled(true);
 * @endcode
 * @{
 */

/** @brief nPM1300 board configuration. */
typedef struct {
  /** Charge current in mA, 32 to 800 in 2 mA steps. */
  uint16_t chg_current_ma;
  /** Discharge current limit in mA, 200 or 1000. */
  uint16_t dischg_limit_ma;
  /** Charge termination current, in percent of the charge current: 10 or 20. */
  uint8_t term_current_pct;
  /** Thermistor beta value. */
  uint16_t thermistor_beta;
  /** NTC hot threshold in degrees Celsius; charging stops above it. */
  uint8_t ntc_hot_celsius;
  /** VBUS current limit in mA applied when USB is connected, 0 to keep the default. */
  uint16_t vbus_current_lim0;
  /** VBUS current limit in mA applied at startup, 0 to keep the default. */
  uint16_t vbus_current_startup;
} Npm1300Config;

/** @brief nPM1300 GPIO pins. */
typedef enum {
  /** GPIO0. */
  Npm1300_Gpio0,
  /** GPIO1. */
  Npm1300_Gpio1,
  /** GPIO2. */
  Npm1300_Gpio2,
  /** GPIO3. */
  Npm1300_Gpio3,
  /** GPIO4. */
  Npm1300_Gpio4,
} Npm1300GpioId_t;

/** @brief Maximum discharge current limit in mA. */
#define NPM1300_DISCHG_LIMIT_MA_MAX 1000UL

/** @brief Extra nPM1300 operations for other drivers. */
typedef struct {
  /**
   * Drive a GPIO as an output. Only GPIO2 and GPIO3 are supported.
   * Returns true on success.
   */
  bool (*gpio_set)(Npm1300GpioId_t id, bool is_high);
  /** Switch load switch 2. Returns true on success. */
  bool (*ldo2_set_enabled)(bool enabled);
  /** Set the discharge current limit in mA, 200 or 1000. Returns true on success. */
  bool (*dischg_limit_ma_set)(uint32_t ilim_ma);
} Npm1300Ops_t;

/** @brief nPM1300 operations. */
extern Npm1300Ops_t NPM1300_OPS;

/** @} */
