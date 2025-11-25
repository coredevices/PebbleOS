/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/pmic.h"

#include "board/board.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "system/passert.h"

#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_adc.h"

typedef enum {
  PmicRegisters_SD1_VOLTAGE = 0x01,
  PmicRegisters_LDO1_VOLTAGE = 0x02,
  PmicRegisters_LDO2_VOLTAGE = 0x03,

  PmicRegisters_GPIO1_CNTL = 0x09,
  PmicRegisters_GPIO2_CNTL = 0x0a,
  PmicRegisters_GPIO3_CNTL = 0x0b,
  PmicRegisters_GPIO4_CNTL = 0x0c,
  PmicRegisters_GPIO5_CNTL = 0x0d,
  PmicRegisters_GPIO_SIG_OUT = 0x20,
  PmicRegisters_GPIO_SIG_IN = 0x21,

  PmicRegisters_REG1_VOLTAGE = 0x22,
  PmicRegisters_REG2_VOLTAGE = 0x23,
  PmicRegisters_REG_CNTL = 0x24,

  PmicRegisters_GPIO_CNTL1 = 0x25,
  PmicRegisters_GPIO_CNTL2 = 0x26,
  PmicRegisters_SD_CNTL1 = 0x30,

  PmicRegisters_BATT_VOLTAGE_MON = 0x32,
  PmicRegisters_STARTUP_CNTL = 0x33,
  PmicRegisters_REFERENCE_CNTL = 0x35,
  PmicRegisters_RESET_CNTL = 0x36,
  PmicRegisters_OVERTEMP_CNTL = 0x37,
  PmicRegisters_REG_STANDBY_MOD1 = 0x39,

  PmicRegisters_PWM_CNTL_L = 0x41,
  PmicRegisters_PWM_CNTL_H = 0x42,

  PmicRegisters_CURR1_VAL = 0x43,
  PmicRegisters_CURR2_VAL = 0x44,

  PmicRegisters_REG_STATUS = 0x73,
  PmicRegisters_INT_MASK_1 = 0x74,
  PmicRegisters_INT_MASK_2 = 0x75,
  PmicRegisters_INT_STATUS_1 = 0x77,
  PmicRegisters_INT_STATUS_2 = 0x78,
  PmicRegisters_CHARGE_CNTL = 0x80,
  PmicRegisters_CHARGE_VOLTAGE_CNTL = 0x81,
  PmicRegisters_CHARGE_CURRENT_CNTL = 0x82,
  PmicRegisters_CHARGE_CONFIG_1 = 0x83,
  PmicRegisters_CHARGE_CONFIG_2 = 0x84,
  PmicRegisters_CHARGE_SUPERVISION = 0x85,
  PmicRegisters_CHARGE_STATUS_1 = 0x86,
  PmicRegisters_CHARGE_STATUS_2 = 0x87,

  PmicRegisters_LOCK_REG = 0x8e,

  PmicRegisters_CHIP_ID = 0x90,
  PmicRegisters_CHIP_REV = 0x91,

  PmicRegisters_FUSE_5 = 0xa5,
  PmicRegisters_FUSE_6 = 0xa6,
  PmicRegisters_FUSE_7 = 0xa7,
  PmicRegisters_FUSE_8 = 0xa8,
  PmicRegisters_FUSE_9 = 0xa9,
  PmicRegisters_FUSE_10 = 0xaa,
  PmicRegisters_FUSE_11 = 0xab,
  PmicRegisters_FUSE_12 = 0xac,
  PmicRegisters_FUSE_13 = 0xad,
  PmicRegisters_FUSE_14 = 0xae,
  PmicRegisters_FUSE_15 = 0xaf,
} PmicRegisters;

// These are values for the reset_reason field of the ResetControl register.
// None of these values should ever be changed, as conversions are done on
// readings dont directly out of the ResetControl register.
// See Figure 79 of the AS3701B datasheet for more information.
typedef enum {
  PmicResetReason_PowerUpFromScratch = 0x00, //!< Battery or charger insertion from scratch
  PmicResetReason_ResVoltFall = 0x01, //!< Battery voltage drop below 2.75V

  PmicResetReason_ForcedReset = 0x02, //!< sw force_reset

  PmicResetReason_OnPulledHigh = 0x03, //!< Force sw power_off, ON pulled high
  PmicResetReason_Charger = 0x04, //!< Forced sw power_off, Charger detected.

  PmicResetReason_XRES = 0x05, //!< External trigger through XRES
  PmicResetReason_OverTemperature = 0x06, //!< Reset caused by overtemperature

  PmicResetReason_OnKeyHold = 0x08, //!< Reset for holding down on key

  PmicResetReason_StandbyInterrupt = 0x0b, //!< Reset for interrupt in standby
  PmicResetReason_StandbyOnPulledHigh = 0x0c, //!< Reset for ON pulled high in standby

  PmicResetReason_Unknown,
} PmicResetReason;

static void prv_start_120hz_clock(void);

static bool prv_init_gpio(void) {
  return true;
}

//! Interrupt masks for InterruptStatus1 and InterruptMask1 registers
enum PmicInt1 {
  PmicInt1_Trickle = (1 << 0), //!< Trickle charge
  PmicInt1_NoBat = (1 << 1),   //!< Battery detached
  PmicInt1_Resume = (1 << 2),  //!< Resuming charge on drop after full
  PmicInt1_EOC = (1 << 3),     //!< End of charge
  PmicInt1_ChDet = (1 << 4),   //!< Charger detected
  PmicInt1_OnKey = (1 << 5),   //!< On Key held
  PmicInt1_OvTemp = (1 << 6),  //!< Set when 110deg is exceeded
  PmicInt1_LowBat = (1 << 7),  //!< Low Battery detected. Set when BSUP drops below ResVoltFall
};

enum PmicRail {
  PmicRail_SD1,  //!< 1.8V
  PmicRail_LDO1, //!< 3.0V
  PmicRail_LDO2, //!< 2.0V
};

#define AS3701B_CHIP_ID 0x11
#define AS3701B_WRITE_ADDR 0x80
#define AS3701B_READ_ADDR  0x81

static bool prv_read_register(uint8_t register_address, uint8_t *result) {
  i2c_use(I2C_DEVICE_AS3701B);
  bool rv = i2c_read_register(I2C_DEVICE_AS3701B, AS3701B_READ_ADDR, register_address, result);
  i2c_release(I2C_DEVICE_AS3701B);
  return rv;
}

static bool prv_write_register(uint8_t register_address, uint8_t value) {
  i2c_use(I2C_DEVICE_AS3701B);
  bool rv = i2c_write_register(I2C_DEVICE_AS3701B, AS3701B_WRITE_ADDR, register_address, value);
  i2c_release(I2C_DEVICE_AS3701B);
  return rv;
}

static bool prv_register_set_bit(uint8_t register_address, uint8_t bit) {
  uint8_t val;
  if (!prv_read_register(register_address, &val)) {
    return false;
  }

  val |= (1 << bit);
  return prv_write_register(register_address, val);
}

static bool prv_register_clear_bit(uint8_t register_address, uint8_t bit) {
  uint8_t val;
  if (!prv_read_register(register_address, &val)) {
    return false;
  }

  val &= ~(1 << bit);
  return prv_write_register(register_address, val);
}

// Read the interrupt status registers to clear pending bits.
static void prv_clear_pending_interrupts(void) {
  uint8_t throwaway_read_result;
  prv_read_register(PmicRegisters_INT_STATUS_1, &throwaway_read_result);
  prv_read_register(PmicRegisters_INT_STATUS_2, &throwaway_read_result);
}

// Set up 120Hz clock which is used for VCOM.
// Slowest possible setting, with divisor of 16 and high/low duration of 256us.
void prv_start_120hz_clock(void) {
  const uint8_t pwm_high_low_time_us = (256 - 1);
  prv_write_register(PmicRegisters_PWM_CNTL_H, pwm_high_low_time_us);
  prv_write_register(PmicRegisters_PWM_CNTL_L, pwm_high_low_time_us);

  bool success = false;
  uint8_t ref_cntl;
  if (prv_read_register(PmicRegisters_REFERENCE_CNTL, &ref_cntl)) {
    ref_cntl |= 0x3; // Divisor of 16
    prv_write_register(PmicRegisters_REFERENCE_CNTL, ref_cntl);

    // Enable PWM Output on GPIO2 (Fig. 64)
    // Bits 6-4: Mode, 0x1 = Output
    // Bits 0-3: iosf, 0xe = PWM
    uint8_t val = (1 << 4) | 0x0e;
    success = prv_write_register(PmicRegisters_GPIO2_CNTL, val);
  }
  PBL_ASSERT(success, "Failed to start PMIC 120Hz PWM");
}

static bool prv_is_alive(void) {
  uint8_t chip_id;
  if (!prv_read_register(PmicRegisters_CHIP_ID, &chip_id)) {
    return false;
  }
  const bool found = (chip_id == AS3701B_CHIP_ID);
  if (found) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Found the as3701b");
  } else {
    PBL_LOG(LOG_LEVEL_DEBUG, "Error: read as3701b whoami byte 0x%x, expecting 0x11", chip_id);
  }
  return found;
}

bool pmic_init(void) {
  prv_init_gpio();
  if (!prv_is_alive()) {
    return false;
  }
  prv_start_120hz_clock();
  return true;
}

bool pmic_is_usb_connected(void) {
  uint8_t status;
  if (!prv_read_register(PmicRegisters_CHARGE_STATUS_2, &status)) {
    return false;
  }
  // ChargerStatus2 (Fig. 98)
  // Bit 2: Charger detected
  return !!(status & (1 << 2));
}

static PmicResetReason prv_reset_reason(void) {
  uint8_t val;
  if (!prv_read_register(PmicRegisters_RESET_CNTL, &val)) {
    return PmicResetReason_Unknown;
  }
  return (val & 0xf0) >> 4;
}

// If the pmic indicates that we were reset due to a charger interrupt, but the
// charger is currently disconnected, then we know we were woken by a disconnect event.
bool pmic_boot_due_to_charger_disconnect(void) {
  if (prv_reset_reason() != PmicResetReason_StandbyInterrupt) {
    return false;
  }

  uint8_t int_status;
  if (!prv_read_register(PmicRegisters_INT_STATUS_1, &int_status)) {
    return false;
  }

  return ((int_status & PmicInt1_ChDet) && !pmic_is_usb_connected());
}

// This is a hard power off, resulting in all rails being disabled.
bool pmic_full_power_off(void) {
  // ResetControl (Fig. 79)
  // Bit 1: power_off - Start a reset cycle, and wait for ON or charger to complete the reset.
  if (prv_register_set_bit(PmicRegisters_RESET_CNTL, 1)) {
    while (1) {}
    __builtin_unreachable();
  }
  return false;
}


// On the as3701b, a power_off will cut power to all rails. We want to keep the
// RTC alive, so rather than performing a sw_power_off, enter the pmic's standby
// mode, powering down all but LDO2.
bool pmic_power_off(void) {
  // Only enable interrupts that should be able to wake us out of standby
  //   - Wake on charger detect
  const uint8_t int_mask = ~((uint8_t)PmicInt1_ChDet);
  prv_write_register(PmicRegisters_INT_MASK_1, int_mask);
  prv_write_register(PmicRegisters_INT_MASK_2, 0xff);

  // Clear interrupt status so we're not woken immediately (read the regs)
  prv_clear_pending_interrupts();

  // Set Reg_Standby_mod1 to specify which rails to turn off / keep on
  //   - SD1, LDO1 off
  //   - LDO2 on
  //   - Disable regulator pulldowns
  prv_write_register(PmicRegisters_REG_STANDBY_MOD1, 0xa);

  // Set standby_mode_on (bit 4) in ReferenceControl to 1 (See Fig. 78)
  if (prv_register_set_bit(PmicRegisters_REFERENCE_CNTL, 4)) {
    while (1) {}
    __builtin_unreachable();
  }
  return false;
}

void set_ldo3_power_state(bool enabled) {
}

void set_4V5_power_state(bool enabled) {
}

void set_6V6_power_state(bool enabled) {
}
