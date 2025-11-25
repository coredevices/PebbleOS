/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "display.h"

#include "drivers/button_id.h"

#include "stm32f4xx_gpio.h"

#include <stdint.h>
#include <stdbool.h>

#define GPIO_Port_NULL ((GPIO_TypeDef *) 0)
#define GPIO_Pin_NULL ((uint16_t)0x0000)


typedef struct {
  //! One of EXTI_PortSourceGPIOX
  uint8_t exti_port_source;

  //! Value between 0-15
  uint8_t exti_line;
} ExtiConfig;

typedef struct {
  const char* const name; ///< Name for debugging purposes.
  GPIO_TypeDef* const gpio; ///< One of GPIOX. For example, GPIOA.
  const uint32_t gpio_pin; ///< One of GPIO_Pin_X.
  ExtiConfig exti;
  GPIOPuPd_TypeDef pull;
} ButtonConfig;

typedef struct {
  GPIO_TypeDef* const gpio; ///< One of GPIOX. For example, GPIOA.
  const uint32_t gpio_pin; ///< One of GPIO_Pin_X.
} ButtonComConfig;

typedef struct {
  GPIO_TypeDef* const gpio; ///< One of GPIOX. For example, GPIOA.
  const uint32_t gpio_pin; ///< One of GPIO_Pin_X.
  bool active_high; ///< Pin is active high or active low
} OutputConfig;

//! Alternate function pin configuration
//! Used to configure a pin for use by a peripheral
typedef struct {
  GPIO_TypeDef* const gpio; ///< One of GPIOX. For example, GPIOA.
  const uint32_t gpio_pin; ///< One of GPIO_Pin_X.
  const uint16_t gpio_pin_source; ///< One of GPIO_PinSourceX.
  const uint8_t  gpio_af; ///< One of GPIO_AF_X
} AfConfig;

typedef struct {
  I2C_TypeDef *const i2c;
  AfConfig i2c_scl; ///< Alternate Function configuration for SCL pin
  AfConfig i2c_sda; ///< Alternate Function configuration for SDA pin
  uint32_t clock_ctrl;  ///< Peripheral clock control flag
  uint32_t clock_speed; ///< Bus clock speed
  uint32_t duty_cycle;  ///< Bus clock duty cycle in fast mode
  const uint8_t ev_irq_channel; ///< I2C Event interrupt (One of X_IRQn). For example, I2C1_EV_IRQn.
  const uint8_t er_irq_channel; ///< I2C Error interrupt (One of X_IRQn). For example, I2C1_ER_IRQn.
  void (* const rail_cfg_fn)(void); //! Configure function for pins on this rail.
  void (* const rail_ctl_fn)(bool enabled); //! Control function for this rail.
} I2cBusConfig;

typedef enum I2cDevice {
  I2C_DEVICE_AS3701B,
} I2cDevice;

typedef struct {
  // I2C Configuration
  /////////////////////////////////////////////////////////////////////////////
  const I2cBusConfig *i2c_bus_configs;
  const uint8_t i2c_bus_count;
  const uint8_t *i2c_device_map;
  const uint8_t i2c_device_count;
} BoardConfig;

// Button Configuration
/////////////////////////////////////////////////////////////////////////////
typedef struct {
  const ButtonConfig buttons[NUM_BUTTONS];
  const ButtonComConfig button_com;
} BoardConfigButton;

#include "board_definitions.h"
