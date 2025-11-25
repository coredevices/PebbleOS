/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/pmic.h"

#include "board/board.h"
#include "console/prompt.h"
#include "drivers/battery.h"
#include "drivers/exti.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "drivers/periph_config.h"
#include "kernel/events.h"
#include "services/common/system_task.h"
#include "system/logging.h"
#include "system/passert.h"

#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_adc.h"

static TimerID s_debounce_charger_timer = TIMER_INVALID_ID;
#define CHARGER_DEBOUNCE_MS 400

//! Remember GPIO output states so we can change the state of individual GPIOs
//! without having to do a read-modify-write.
static uint8_t s_pmic_gpio_output_state = 0;

typedef enum PmicGpio {
  PmicGpio1 = (1 << 0),
  PmicGpio2 = (1 << 1),
  PmicGpio3 = (1 << 2),
  PmicGpio4 = (1 << 3),
  PmicGpio5 = (1 << 4),
} PmicGpio;

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

static const PmicRegisters s_registers[] = {
  0x01, 0x02, 0x03, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x20, 0x21,
  0x22, 0x23, 0x24, 0x25, 0x26, 0x30, 0x32, 0x33, 0x35, 0x36,
  0x37, 0x39, 0x41, 0x42, 0x43, 0x44, 0x73, 0x74, 0x75, 0x77,
  0x78, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x8e,
  0x90, 0x91, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac,
  0xad, 0xae, 0xaf,
};

static bool prv_init_gpio(void) {
  periph_config_acquire_lock();

  // Init PMIC_INTN
  gpio_input_init_pull_up_down(&BOARD_CONFIG_POWER.pmic_int_gpio, GPIO_PuPd_UP);

  periph_config_release_lock();
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

static bool prv_read_register(uint8_t register_address, uint8_t *result) {
  i2c_use(I2C_AS3701B);
  bool rv = i2c_read_register(I2C_AS3701B, register_address, result);
  i2c_release(I2C_AS3701B);
  return rv;
}

static bool prv_write_register(uint8_t register_address, uint8_t value) {
  i2c_use(I2C_AS3701B);
  bool rv = i2c_write_register(I2C_AS3701B, register_address, value);
  i2c_release(I2C_AS3701B);
  return rv;
}

static bool prv_set_register_bit(uint8_t register_address, uint8_t bit, bool enable) {
  uint8_t val;
  if (!prv_read_register(register_address, &val)) {
    return false;
  }
  if (enable) {
    val |= (1 << bit);
  } else {
    val &= ~(1 << bit);
  }
  return prv_write_register(register_address, val);
}

static bool prv_set_pmic_gpio_outputs(PmicGpio set_mask, PmicGpio clear_mask) {
  PBL_ASSERTN((set_mask & clear_mask) == 0);
  uint8_t new_output_state = s_pmic_gpio_output_state;
  new_output_state |= set_mask;
  new_output_state &= ~clear_mask;
  if (prv_write_register(PmicRegisters_GPIO_SIG_OUT, new_output_state)) {
    s_pmic_gpio_output_state = new_output_state;
    return true;
  }
  return false;
}

static void prv_init_pmic_gpio_outputs(void) {
  // Sync the state of the PMIC GPIO output register with the value that we
  // think it has.
  if (!prv_set_pmic_gpio_outputs(0, 0)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not initialize PMIC GPIO outputs");
  }
}

static void prv_handle_charge_state_change(void *null) {
  const bool is_charging = pmic_is_charging();
  const bool is_connected = pmic_is_usb_connected();
  PBL_LOG(LOG_LEVEL_DEBUG, "AS3701b Interrupt: Charging? %s Plugged? %s",
      is_charging ? "YES" : "NO", is_connected ? "YES" : "NO");

  PebbleEvent event = {
    .type = PEBBLE_BATTERY_CONNECTION_EVENT,
    .battery_connection = {
      .is_connected = battery_is_usb_connected(),
    },
  };
  event_put(&event);
}

// Read the interrupt status registers to clear pending bits.
static void prv_clear_pending_interrupts(void) {
  uint8_t throwaway_read_result;
  prv_read_register(PmicRegisters_INT_STATUS_1, &throwaway_read_result);
  prv_read_register(PmicRegisters_INT_STATUS_2, &throwaway_read_result);
}

static void prv_pmic_state_change_cb(void *null) {
  prv_clear_pending_interrupts();
  new_timer_start(s_debounce_charger_timer, CHARGER_DEBOUNCE_MS,
                  prv_handle_charge_state_change, NULL, 0 /*flags*/);
}

static void prv_as3701b_interrupt_handler(bool *should_context_switch) {
  system_task_add_callback_from_isr(prv_pmic_state_change_cb, NULL, should_context_switch);
}

static void prv_configure_interrupts(void) {
  // Clear pending interrupts in case we were woken from standby
  prv_clear_pending_interrupts();

  exti_configure_pin(
      BOARD_CONFIG_POWER.pmic_int, ExtiTrigger_Falling, prv_as3701b_interrupt_handler);
  exti_enable(BOARD_CONFIG_POWER.pmic_int);

  const uint8_t mask = (uint8_t) ~(PmicInt1_LowBat | PmicInt1_ChDet | PmicInt1_EOC);
  prv_write_register(PmicRegisters_INT_MASK_1, mask);
  prv_write_register(PmicRegisters_INT_MASK_2, ~0);
}

// Set up 160Hz clock which is used for VCOM.
// This setting is a divisor of 16 and a high/low duration of 195us, as
// given in the following: 1000000 / (16 * 195 * 2) = ~160Hz
static void prv_start_160hz_clock(void) {
  const uint8_t pwm_high_low_time_us = (195 - 1);
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

static void prv_configure_charging(void) {
  // Set charge control to low current range, constant current ctl to 118mA.
  bool success = false;
  if (prv_set_register_bit(PmicRegisters_CHARGE_CNTL, 7, true)) {
    uint8_t cntl;
    if (prv_read_register(PmicRegisters_CHARGE_CURRENT_CNTL, &cntl)) {
      cntl = (cntl & 0xf0) | 0x09; // 118mA when cc_range_select = 1
      success = prv_write_register(PmicRegisters_CHARGE_CURRENT_CNTL, cntl);
    }
  }
  if (!success) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not set pmic charge current.");
  }

  // Set EOC current to 5% of ConstantCurrent
  prv_set_register_bit(PmicRegisters_CHARGE_CONFIG_2, 5, false);

  if (BOARD_CONFIG_POWER.charging_cutoff_voltage == 4300) {
    // Set EOC to 4.30V, keep Vsup_min at 4.20V
    //   EOC = 3.82V + 0.02V * N
    prv_write_register(PmicRegisters_CHARGE_VOLTAGE_CNTL, 0x18 | (1 << 6));
  }

  pmic_set_charger_state(true);

  // Enable AutoResume: Resumes charging on voltage drop after EOC
  prv_set_register_bit(PmicRegisters_CHARGE_CNTL, 6, true);
}

static void prv_configure_battery_measure(void) {
  // Set PMIC GPIO5 (the battery measure enable pin) as an open-drain output
  // with no pull and inverted output. Setting the output to 1 will drive GPIO5
  // low, and setting it to 0 will cause it to float.
  bool success = prv_write_register(PmicRegisters_GPIO5_CNTL, 0b10100000) &&
                 prv_set_pmic_gpio_outputs(0, PmicGpio5);
  if (!success) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not configure the battery measure control GPIO");
  }
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

static void prv_set_sd1_voltage(void) {
  // STM32F4 running at 1.76V may trigger a Power Down Reset (PDR). The power supply has a
  // tolerance of 3%. Set the voltage rail to 1.825V so our theoretical minimum should be 1.77V
  uint8_t sd1_vsel;
  if (prv_read_register(PmicRegisters_SD1_VOLTAGE, &sd1_vsel)) {
    const uint8_t sd1_vsel_mask = 0x3f; // sd1_vsel is in first 6 bits
    sd1_vsel &= ~sd1_vsel_mask;
    // V_SD1 = 1.4V + (sd1_vsel - 0x40) * 25mV = 1.4V + (0x51 - 0x40) * 25mV = 1.825V
    sd1_vsel |= (0x51 & sd1_vsel_mask);
    prv_write_register(PmicRegisters_SD1_VOLTAGE, sd1_vsel);
  }
}

static uint8_t s_last_reset_reason = 0;
static void prv_stash_last_reset_reason(void) {
  uint8_t reset_cntl;
  if (!prv_read_register(PmicRegisters_RESET_CNTL, &reset_cntl)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to read the RESET_CNTL register");
    return;
  }

  s_last_reset_reason = reset_cntl >> 4;
}

uint32_t pmic_get_last_reset_reason(void) {
  return s_last_reset_reason;
}

bool pmic_init(void) {
  s_debounce_charger_timer = new_timer_create();

  prv_init_gpio();
  if (!prv_is_alive()) {
    return false;
  }
  prv_stash_last_reset_reason();
  prv_init_pmic_gpio_outputs();
  prv_set_sd1_voltage();

  prv_start_160hz_clock();

  prv_configure_battery_measure();
  prv_configure_interrupts();
  prv_configure_charging();

  // Override OTP setting for 'onkey_lpress_ reset=1' so that we shutdown instead of triggering a
  // reset on a long button hold
  prv_set_register_bit(PmicRegisters_REFERENCE_CNTL, 5, false);

  return true;
}

// On the as3701b, a power_off will cut power to all rails. We want to keep the
// RTC alive, so rather than performing a sw_power_off, enter the pmic's standby
// mode, powering down all but LDO2.
bool pmic_power_off(void) {
  // Only enable interrupts that should be able to wake us out of standby
  //   - Wake on charger detect
  const uint8_t int_mask = (uint8_t)~(PmicInt1_ChDet);
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
  if (prv_set_register_bit(PmicRegisters_REFERENCE_CNTL, 4, true)) {
    while (1) {}
    __builtin_unreachable();
  }
  return false;
}

// This is a hard power off, resulting in all rails being disabled.
// Generally, this is not desirable since we'll lose the backup domain.
// You're *probably* looking for pmic_power_off.
bool pmic_full_power_off(void) {
  // ResetControl (Fig. 79)
  // Bit 1: power_off - Start a reset cycle, and wait for ON or charger to complete the reset.
  if (prv_set_register_bit(PmicRegisters_RESET_CNTL, 1, true)) {
    while (1) {}
    __builtin_unreachable();
  }
  return false;
}

// We have no way of directly reading Vsup with as3701b on Silk. Just assume
// that we are getting what we've configured as regulated Vsup.
uint16_t pmic_get_vsys(void) {
  uint8_t cfg;
  prv_read_register(PmicRegisters_CHARGE_CONFIG_1, &cfg);
  const uint8_t vsup_voltage = (cfg & 0x6) >> 1;
  switch (vsup_voltage) {
    case 0: return 4400;
    case 1: return 4500;
    case 2: return 4600;
    case 3: return 4700;
    case 4: return 4800;
    case 5: return 4900;
    case 6: return 5000;
    case 7: return 5500;
  }
  WTF;
}

bool pmic_set_charger_state(bool enable) {
  // ChargerControl (Fig. 91)
  // Bit 5: Enable battery charging from USB charger.
  return prv_set_register_bit(PmicRegisters_CHARGE_CNTL, 5, enable);
}

bool pmic_is_charging(void) {
  uint8_t status;
  if (!prv_read_register(PmicRegisters_CHARGE_STATUS_1, &status)) {
#if defined (TARGET_QEMU)
    // NOTE: When running on QEMU, i2c reads return false. For now, just assume a failed
    // i2c read means we are charging
    return true;
#else
    PBL_LOG(LOG_LEVEL_DEBUG, "Failed to read charging status 1 register.");
    return false;
#endif
  }
  // ChargerStatus1 (Fig. 97)
  // Bit 0: CC
  //     1: Maintain / Resume charge
  //     2: Trickle charge
  //     3: CV
  return (status & 0x0f);
}

bool pmic_is_usb_connected(void) {
  uint8_t status;
  if (!prv_read_register(PmicRegisters_CHARGE_STATUS_2, &status)) {
#if TARGET_QEMU
    // NOTE: When running on QEMU, i2c reads return false. For now, just assume a failed
    // i2c read means we are connected to a USB cable
    return true;
#endif
    PBL_LOG(LOG_LEVEL_WARNING, "Failed to read charging status 2 register.");
    return false;
  }
  // ChargerStatus2 (Fig. 98)
  // Bit 2: Charger detected
  return status & (1 << 2);
}

void pmic_read_chip_info(uint8_t *chip_id, uint8_t *chip_revision, uint8_t *buck1_vset) {
  prv_read_register(PmicRegisters_CHIP_ID, chip_id);
  prv_read_register(PmicRegisters_CHIP_REV, chip_revision);
  prv_read_register(PmicRegisters_SD1_VOLTAGE, buck1_vset);
}

bool pmic_enable_battery_measure(void) {
  // GPIO 5 on the pmic driven low is battery measure enable.
  return prv_set_pmic_gpio_outputs(PmicGpio5, 0);
}

bool pmic_disable_battery_measure(void) {
  // Set GPIO5 floating to disable battery measure.
  return prv_set_pmic_gpio_outputs(0, PmicGpio5);
}

void set_ldo3_power_state(bool enabled) {
}

void set_4V5_power_state(bool enabled) {
}

void set_6V6_power_state(bool enabled) {
}


void command_pmic_read_registers(void) {
  int reg;
  uint8_t val;
  char buffer[16];
  for (uint8_t i = 0; i < ARRAY_LENGTH(s_registers); ++i) {
    reg = s_registers[i];
    prv_read_register(reg, &val);
    prompt_send_response_fmt(buffer, sizeof(buffer), "Reg 0x%02X: 0x%02X", reg, val);
  }
}

void command_pmic_status(void) {
  uint8_t id, rev, buck1;
  pmic_read_chip_info(&id, &rev, &buck1);
  PBL_LOG(LOG_LEVEL_DEBUG, "ID: 0x%"PRIx8" REV: 0x%"PRIx8" BUCK1: 0x%"PRIx8, id, rev, buck1);
  bool connected = pmic_is_usb_connected();
  PBL_LOG(LOG_LEVEL_DEBUG, "USB Status: %s", (connected) ? "Connected" : "Disconnected");
  bool charging = pmic_is_charging();
  PBL_LOG(LOG_LEVEL_DEBUG, "Charging? %s", (charging) ? "true" : "false");
}

void command_pmic_rails(void) {
  // TODO: Implement.
}
