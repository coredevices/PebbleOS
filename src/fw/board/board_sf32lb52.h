/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "display.h"

#include <stdint.h>
#include <stdbool.h>

#include "bf0_hal_pinmux.h"
#include "drivers/button_id.h"

#define IRQ_PRIORITY_INVALID (1 << __NVIC_PRIO_BITS)

enum {
  #define IRQ_DEF(num, irq) IS_VALID_IRQ__##irq,
  #include "irq_sf32lb52.def"
  #undef IRQ_DEF
};

//! Creates a trampoline to the interrupt handler defined within the driver
#define IRQ_MAP(irq, handler, device) \
  void irq##_IRQHandler(void) { \
    handler(device); \
  } \
  _Static_assert(IS_VALID_IRQ__##irq || true, "(See comment below)")
/*
 * The above static assert checks that the requested IRQ is valid by checking that the enum
 * value (generated above) is declared. The static assert itself will not trip, but you will get
 * a compilation error from that line if the IRQ does not exist within irq_sf32lb.def.
 */

#define GPIO_Port_NULL NULL
#define GPIO_Pin_NULL 0U

typedef enum {
  GPIO_OType_PP,
  GPIO_OType_OD,
} GPIOOType_TypeDef;

typedef enum {
  GPIO_PuPd_NOPULL,
  GPIO_PuPd_UP,
  GPIO_PuPd_DOWN,
} GPIOPuPd_TypeDef;

typedef enum {
  GPIO_Speed_2MHz,
  GPIO_Speed_50MHz,
  GPIO_Speed_200MHz
} GPIOSpeed_TypeDef;

typedef struct {
  GPIO_TypeDef* const peripheral; ///< One of GPIOX. For example, GPIOA.
  const uint32_t gpio_pin; ///< One of GPIO_Pin_X.
} ExtiConfig;

typedef struct {
  void *gpio;
  uint8_t gpio_pin;
} InputConfig;

typedef struct {
  void *gpio;
  uint8_t gpio_pin;
  bool active_high;
} OutputConfig;

typedef struct {
  void *gpio;
  uint8_t gpio_pin;
} AfConfig;

typedef struct {
  int pad;
  pin_function func;
  int flags;
} Pinmux;

typedef struct {
  GPT_HandleTypeDef handle;
  GPT_ClockConfigTypeDef clock_config;
  uint16_t value;
  uint16_t resolution;
  int enabled;
  uint16_t channel;
  uint8_t  is_comp;
} PwmState;

typedef struct {
  Pinmux pwm_pin;
  PwmState *state;
} PwmConfig;

typedef struct {
} TimerConfig;

typedef enum {
  ActuatorOptions_Ctl = 1 << 0, ///< GPIO is used to enable / disable vibe
  ActuatorOptions_Pwm = 1 << 1, ///< PWM control
  ActuatorOptions_IssiI2C = 1 << 2, ///< I2C Device, currently used for V1_5 -> OG steel backlight
  ActuatorOptions_HBridge = 1 << 3, //< PWM actuates an H-Bridge, requires ActuatorOptions_PWM
} ActuatorOptions;

typedef struct {
  const ActuatorOptions options;
  const OutputConfig ctl;
  const PwmConfig pwm;
} BoardConfigActuator;

typedef struct {
  uint8_t backlight_on_percent;
  uint8_t backlight_max_duty_cycle_percent;
  ExtiConfig dbgserial_int;
  InputConfig dbgserial_int_gpio;
  OutputConfig lcd_com;
  //ambient light config
  uint32_t ambient_light_dark_threshold;
  uint32_t ambient_k_delta_threshold;
} BoardConfig;

typedef struct {
  const char* name;
  GPIO_TypeDef* const port;
  uint8_t pin;
  GPIOPuPd_TypeDef pull;
  bool active_high;
} ButtonConfig;

typedef struct {
  ButtonConfig buttons[NUM_BUTTONS];
  GPT_TypeDef *timer;
  IRQn_Type timer_irqn;
} BoardConfigButton;

typedef struct {
  ExtiConfig pmic_int;
  //! Percentage for watch only mode
  const uint8_t low_power_threshold;
  //! Approximate hours of battery life
  const uint8_t battery_capacity_hours;
} BoardConfigPower;

typedef enum {
  AccelThresholdLow, ///< A sensitive state used for stationary mode
  AccelThresholdHigh, ///< The accelerometer's default sensitivity
  AccelThreshold_Num,
} AccelThreshold;

typedef struct {
  int axes_offsets[3];
  bool axes_inverts[3];
  uint32_t shake_thresholds[AccelThreshold_Num];
  uint32_t double_tap_threshold;
  // LSM6DSO tap timing parameters (in register units):
  // tap_shock:   0..3   => maximum duration of an over-threshold acceleration for tap recognition.
  // tap_quiet:   0..3   => quiet time after a tap where acceleration must remain below threshold.
  // tap_dur:     0..15  => max time window between first and second tap for double tap detection.
  // Values of 0 use driver defaults if unsupported by the underlying IMU.
  uint8_t tap_shock;
  uint8_t tap_quiet;
  uint8_t tap_dur;
} AccelConfig;

typedef struct {
  const AccelConfig accel_config;
  const InputConfig accel_int_gpios[2];
  const ExtiConfig accel_ints[2];
} BoardConfigAccel;

typedef struct {
  int axes_offsets[3];
  bool axes_inverts[3];
} MagConfig;

typedef struct {
  const MagConfig mag_config;
} BoardConfigMag;

typedef enum {
  SpiPeriphClockAPB1,
  SpiPeriphClockAPB2
} SpiPeriphClock;

#include "drivers/dma.h"
#include "drivers/flash/qspi_flash.h"
#include "drivers/flash/qspi_flash_definitions.h"
#include "drivers/qspi_definitions.h"
#include "drivers/sf32lb52/uart_definitions.h"
#include "drivers/sf32lb52/jdi_lpm015m135a.h"
#include "drivers/mic/sf32lb52/pdm_definitions.h"
#include "drivers/sf32lb52/audio/audio_definitions.h"

typedef const struct DMARequest DMARequest;
typedef const struct UARTDevice UARTDevice;
typedef const struct SPIBus SPIBus;
typedef const struct SPISlavePort SPISlavePort;
typedef const struct I2CBus I2CBus;
typedef const struct I2CSlavePort I2CSlavePort;
typedef const struct HRMDevice HRMDevice;
typedef const struct MicDevice MicDevice;
typedef const struct QSPIPort QSPIPort;
typedef const struct QSPIFlash QSPIFlash;
typedef const struct DisplayJDIDevice DisplayJDIDevice;
typedef const struct AudioDevice AudioDevice;

#include "drivers/i2c_definitions.h"
#include "drivers/sf32lb52/i2c_hal_definitions.h"

void board_early_init(void);
void board_init(void);

#include "board_definitions.h"

