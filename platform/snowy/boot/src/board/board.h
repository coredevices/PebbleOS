/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/button_id.h"

#if defined(MICRO_FAMILY_STM32F2)
#include "stm32f2xx_gpio.h"
#elif defined(MICRO_FAMILY_STM32F4)
#include "stm32f4xx_gpio.h"
#endif

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
} InputConfig;

typedef struct {
  GPIO_TypeDef* const gpio; ///< One of GPIOX. For example, GPIOA.
  const uint16_t gpio_pin; ///< One of GPIO_Pin_*
  const uint8_t adc_channel; ///< One of ADC_Channel_*
} ADCInputConfig;

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
  int i2c_address;
  int axes_offsets[3];
  bool axes_inverts[3];
} AccelConfig;

typedef struct {
  int i2c_address;
  int axes_offsets[3];
  bool axes_inverts[3];
} MagConfig;

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
  I2C_DEVICE_LIS3DH = 0,
  I2C_DEVICE_MAG3110,
  I2C_DEVICE_MFI,
  I2C_DEVICE_LED_CONTROLLER,
  I2C_DEVICE_MAX14690,
} I2cDevice;

typedef struct {
  AfConfig i2s_ck;
  AfConfig i2s_sd;
  DMA_Stream_TypeDef *dma_stream;
  uint32_t dma_channel;
  uint32_t dma_channel_irq;
  uint32_t dma_clock_ctrl;
  SPI_TypeDef *spi;
  uint32_t spi_clock_ctrl;

  //! Pin we use to control power to the microphone. Only used on certain boards.
  OutputConfig mic_gpio_power;
} MicConfig;

typedef enum {
  OptionNotPresent = 0, // FIXME
  OptionActiveLowOpenDrain,
  OptionActiveHigh
} PowerCtl5VOptions;

typedef enum {
  BacklightPinNoPwm = 0,
  BacklightPinPwm,
  BacklightIssiI2C
} BacklightOptions;

typedef enum {
  VibePinNoPwm = 0,
  VibePinPwm,
} VibeOptions;

typedef struct {
  TIM_TypeDef* const peripheral; ///< A TIMx peripheral
  const uint32_t config_clock;   ///< One of RCC_APB1Periph_TIMx. For example, RCC_APB1Periph_TIM3.
  void (* const init)(TIM_TypeDef* TIMx, TIM_OCInitTypeDef* TIM_OCInitStruct); ///< One of TIM_OCxInit
  void (* const preload)(TIM_TypeDef* TIMx, uint16_t TIM_OCPreload); ///< One of TIM_OCxPreloadConfig
} TimerConfig;

typedef enum {
  CC2564A = 0,
  CC2564B,
} BluetoothController;

typedef struct {
  // I2C Configuration
  /////////////////////////////////////////////////////////////////////////////
  const I2cBusConfig *i2c_bus_configs;
  const uint8_t i2c_bus_count;
  const uint8_t *i2c_device_map;
  const uint8_t i2c_device_count;

  // Audio Configuration
  /////////////////////////////////////////////////////////////////////////////
  const bool has_mic;
  const MicConfig mic_config;

  // Ambient Light Configuration
  /////////////////////////////////////////////////////////////////////////////
  const bool has_ambient_light_sensor;
  const uint32_t ambient_light_dark_threshold;
  const OutputConfig photo_en;
  const ADCInputConfig light_level;

  // Debug Serial Configuration
  /////////////////////////////////////////////////////////////////////////////
  const ExtiConfig dbgserial_int;

  // Accessory Configuration
  /////////////////////////////////////////////////////////////////////////////
  //! Enable power supply to the accessory connector.
  const OutputConfig accessory_power_en;
  const AfConfig accessory_rxtx_afcfg;
  USART_TypeDef* const accessory_uart;
  const ExtiConfig accessory_exti;

  // Bluetooth Configuration
  /////////////////////////////////////////////////////////////////////////////
  const BluetoothController bt_controller;
  const OutputConfig bt_shutdown;
  const OutputConfig bt_cts_int;
  const ExtiConfig bt_cts_exti;

  const OutputConfig mfi_reset_pin;

  // Display Configuration
  /////////////////////////////////////////////////////////////////////////////
  const OutputConfig lcd_com; //!< This needs to be pulsed regularly to keep the sharp display fresh.

  const ExtiConfig cdone_int;
  const ExtiConfig intn_int;

  //! Controls power to the sharp display
  const PowerCtl5VOptions power_5v0_options;
  const OutputConfig power_ctl_5v0;

  const BacklightOptions backlight_options;
  const OutputConfig backlight_ctl;
  const TimerConfig backlight_timer;
  const AfConfig backlight_afcfg;

} BoardConfig;

// Button Configuration
/////////////////////////////////////////////////////////////////////////////
typedef struct {
  const ButtonConfig buttons[NUM_BUTTONS];
  const ButtonComConfig button_com;
} BoardConfigButton;

// Power Configuration
/////////////////////////////////////////////////////////////////////////////
typedef struct {
  const ExtiConfig pmic_int;

  //! Analog voltage of the battery read through an ADC.
  const ADCInputConfig battery_vmon;
  //! Tells us if the USB cable plugged in.
  const InputConfig vusb_stat;
  const ExtiConfig vusb_exti;
  //! Tells us whether the charger thinks we're charging or not.
  const InputConfig chg_stat;
  //! Tell the charger to use 2x current to charge faster (MFG only).
  const OutputConfig chg_fast;
  //! Enable the charger. We may want to disable this in MFG, normally it's always on.
  const OutputConfig chg_en;

  //! Interrupt that fires when the USB cable is plugged in
  const bool has_vusb_interrupt;

  const bool wake_on_usb_power;

  const int charging_status_led_voltage_compensation;

  //! Percentage for watch only mode
  const uint8_t low_power_threshold;
} BoardConfigPower;

typedef struct {
  const AccelConfig accel_config;
  const ExtiConfig accel_ints[2];
} BoardConfigAccel;

typedef struct {
  const MagConfig mag_config;
  const ExtiConfig mag_int;
} BoardConfigMag;

typedef struct {
  const VibeOptions vibe_options;
  const OutputConfig vibe_ctl;
  const OutputConfig vibe_pwm;
  const TimerConfig vibe_timer;
  const AfConfig vibe_afcfg;
} BoardConfigVibe;


#include "board_definitions.h"
