/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "display.h"

#include <cmsis_core.h>
#include <stdint.h>
#include <stdbool.h>

#include "drivers/button_id.h"

#define IRQ_PRIORITY_INVALID (1 << __NVIC_PRIO_BITS)

enum {
  #define IRQ_DEF(num, irq) IS_VALID_IRQ__##irq,
  #include "irq_qemu.def"
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
 * a compilation error from that line if the IRQ does not exist within irq_qemu.def.
 */

// Compatibility type for gpio.h (QEMU has no real GPIO peripheral struct)
typedef void GPIO_TypeDef;

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
  void *const peripheral;
  const uint32_t gpio_pin;
  GPIOPuPd_TypeDef pull;
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
  int func;
  int flags;
} Pinmux;

typedef struct {
  uint16_t value;
  uint16_t resolution;
  int enabled;
  uint16_t channel;
} PwmState;

typedef struct {
  Pinmux pwm_pin;
  PwmState *state;
} PwmConfig;

typedef struct {
} TimerConfig;

typedef enum {
  ActuatorOptions_Ctl = 1 << 0,
  ActuatorOptions_Pwm = 1 << 1,
  ActuatorOptions_HBridge = 1 << 3,
} ActuatorOptions;

typedef struct {
  const ActuatorOptions options;
  const OutputConfig ctl;
  const PwmConfig pwm;
} BoardConfigActuator;

typedef struct {
  uint8_t backlight_on_percent;
  uint32_t ambient_light_dark_threshold;
  uint32_t ambient_k_delta_threshold;
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
  uint32_t dynamic_backlight_min_threshold;
#endif
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  // Default RGB backlight color (packed 0x00RRGGBB), applied when no app
  // override is set. User-preference overrides this via backlight_set_color().
  uint32_t backlight_default_color;
#endif
  ExtiConfig dbgserial_int;
  InputConfig dbgserial_int_gpio;
} BoardConfig;

typedef struct {
  const char *name;
  void *const port;
  uint8_t pin;
  GPIOPuPd_TypeDef pull;
  bool active_high;
} ButtonConfig;

typedef struct {
  ButtonConfig buttons[NUM_BUTTONS];
  void *timer;
  int timer_irqn;
} BoardConfigButton;

typedef struct {
  ExtiConfig pmic_int;
  const uint8_t low_power_threshold;
  const uint16_t battery_capacity_hours;
} BoardConfigPower;

typedef enum {
  AccelThresholdLow,
  AccelThresholdHigh,
  AccelThreshold_Num,
} AccelThreshold;

typedef struct {
  int axes_offsets[3];
  bool axes_inverts[3];
  uint32_t shake_thresholds[AccelThreshold_Num];
  uint32_t double_tap_threshold;
  uint8_t tap_shock;
  uint8_t tap_quiet;
  uint8_t tap_dur;
  uint8_t default_motion_sensitivity;
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

// QEMU MMIO peripheral base addresses
#define QEMU_UART0_BASE     0x40000000
#define QEMU_UART1_BASE     0x40001000
#define QEMU_UART2_BASE     0x40002000
#define QEMU_TIMER0_BASE    0x40003000
#define QEMU_TIMER1_BASE    0x40004000
#define QEMU_RTC_BASE       0x40005000
#define QEMU_GPIO_BASE      0x40006000
#define QEMU_SYSCTRL_BASE   0x40007000
#define QEMU_DISPLAY_BASE   0x40008000
#define QEMU_DISPLAY_FB_BASE 0x50000000
#define QEMU_EXTFLASH_BASE  0x40010000
#define QEMU_TOUCH_BASE     0x40011000
#define QEMU_AUDIO_BASE     0x40012000

#define QEMU_EXTFLASH_XIP_BASE 0x10000000

// Forward-declare device types
typedef const struct UARTDevice UARTDevice;
typedef const struct SPIBus SPIBus;
typedef const struct SPISlavePort SPISlavePort;
typedef const struct I2CBus I2CBus;
typedef const struct I2CSlavePort I2CSlavePort;
typedef const struct QSPIPort QSPIPort;
typedef const struct QSPIFlash QSPIFlash;
typedef const struct HRMDevice HRMDevice;
typedef const struct MicDevice MicDevice;
typedef const struct AudioDevice AudioDevice;

// QEMU display device
typedef struct QemuDisplayDevice {
  uint32_t base_addr;
  uint32_t fb_addr;
  uint16_t width;
  uint16_t height;
  uint8_t bpp;
  int irqn;
  int irq_priority;
} QemuDisplayDevice;
typedef const QemuDisplayDevice DisplayDevice;

extern AudioDevice *const AUDIO;

void board_early_init(void);
void board_init(void);

// QEMU doesn't have newlib-nano sniprintf; use standard snprintf
#include <stdio.h>
#define sniprintf snprintf

#include "board_definitions.h"
