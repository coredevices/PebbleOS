/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/* Because nPM1300 also has the battery monitor, we implement both the
 * pmic_* and the battery_* API here.  */

#include <math.h>

#include <pbl/drivers/pmic.h>
#include <pbl/drivers/battery.h>

#include <board/board.h>
#include <pbl/drivers/battery.h>
#include <pbl/drivers/exti.h>
#include <pbl/drivers/i2c.h>
#include <kernel/events.h>
#include <kernel/util/delay.h>
#include <kernel/util/sleep.h>
#include <pbl/services/system_task.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>

PBL_LOG_MODULE_DEFINE(driver_pmic_npm1300, CONFIG_DRIVER_PMIC_LOG_LEVEL);

#define CHARGER_DEBOUNCE_MS 400
#define ADC_POLL_DELAY_MS   5   // Delay between ADC poll iterations to reduce I2C traffic
#define ADC_POLL_TIMEOUT_MS 100 // Max time to wait for ADC measurement
static TimerID s_debounce_charger_timer = TIMER_INVALID_ID;
static uint32_t s_dischg_limit_ma;

// MAIN
#define NPM1300_EVENTSADCCLR                 0x0003U
#define NPM1300_EVENTSADC_VBATRDY            PBL_BIT(0)
#define NPM1300_EVENTSADC_NTCRDY             PBL_BIT(1)
#define NPM1300_EVENTSADC_VSYSRDY            PBL_BIT(3)
#define NPM1300_EVENTSADC_IBATRDY            PBL_BIT(6)
#define NPM1300_EVENTSBCHARGER1CLR           0x000BU
#define NPM1300_INTENEVENTSBCHARGER1SET      0x000CU
#define NPM1300_EVENTSBCHARGER1_CHGCOMPLETED PBL_BIT(4)
#define NPM1300_EVENTSVBUSIN0CLR             0x0017U
#define NPM1300_INTENEVENTSVBUSIN0SET        0x0018U
#define NPM1300_EVENTSVBUSIN0_VBUSDETECTED   PBL_BIT(0)
#define NPM1300_EVENTSVBUSIN0_VBUSREMOVED    PBL_BIT(1)

// SYSTEM
#define NPM1300_TESTACCESS      0x0123U
#define NPM1300_TESTACCESS_VAL0 0x44U
#define NPM1300_TESTACCESS_VAL1 0x90U
#define NPM1300_TESTACCESS_VAL2 0xFAU
#define NPM1300_TESTACCESS_VAL3 0xCEU

// VBUSIN
#define NPM1300_TASKUPDATELIMSW      0x0200U
#define NPM1300_TASKUPDATELIMSW_EN   PBL_BIT(0)
#define NPM1300_VBUSINILIM0          0x0201U
#define NPM1300_VBUSINILIMSTARTUP    0x0202U
#define NPM1300_VBUSINSTATUS         0x0207U
#define NPM1300_VBUSINSTATUS_PRESENT PBL_BIT(0)

// BCHARGER
#define NPM1300_TASKRELEASEERROR                 0x0300U
#define NPM1300_TASKCLEARCHGERR                  0x0301U
#define NPM1300_BCHGENABLESET                    0x0304U
#define NPM1300_BCHGENABLECLR                    0x0305U
#define NPM1300_BCHGISETMSB                      0x0308U
#define NPM1300_BCHGISETLSB                      0x0309U
#define NPM1300_BCHGISETDISCHARGEMSB             0x030AU
#define NPM1300_BCHGISETDISCHARGELSB             0x030BU
#define NPM1300_BCHGVTERM                        0x030CU
#define NPM1300_BCHGVTERM_4V20                   0x8U
#define NPM1300_BCHGVTERM_4V35                   0xBU
#define NPM1300_BCHGVTERM_4V45                   0xDU
#define NPM1300_BCHGVTERMR                       0x030DU
#define NPM1300_BCHGVTERMR_4V00                  0x4U
#define NPM1300_BCHGITERMSEL                     0x030FU
#define NPM1300_BCHGITERMSEL_SEL10               0U
#define NPM1300_BCHGITERMSEL_SEL20               1U
#define NPM1300_NTCHOT                           0x0316U
#define NPM1300_NTCHOTLSB                        0x0317U
#define NPM1300_BCHGCHARGESTATUS                 0x0334U
#define NPM1300_BCHGCHARGESTATUS_COMPLETED       PBL_BIT(1)
#define NPM1300_BCHGCHARGESTATUS_TRICKLECHARGE   PBL_BIT(2)
#define NPM1300_BCHGCHARGESTATUS_CONSTANTCURRENT PBL_BIT(3)
#define NPM1300_BCHGCHARGESTATUS_CONSTANTVOLTAGE PBL_BIT(4)
#define NPM1300_BCHGERRREASON                    0x0336U
#define NPM1300_BCHGDEBUG                        0x0346U
#define NPM1300_BCHGDEBUG_DISABLEBATTERYDETECT   PBL_BIT(2)
#define NPM1300_BCHGVBATLOWCHARGE                0x0350U

// BUCK
#define NPM1300_BUCK1ENACLR         0x0401U
#define NPM1300_BUCK1NORMVOUT       0x0408U
#define NPM1300_BUCK2NORMVOUT       0x040AU
#define NPM1300_BUCKSWCTRLSEL       0x040FU
#define NPM1300_BUCKSWCTRLSEL_BUCK1 PBL_BIT(0)
#define NPM1300_BUCKSWCTRLSEL_BUCK2 PBL_BIT(1)
#define NPM1300_BUCK1VOUTSTATUS     0x0410U
#define NPM1300_BUCK2VOUTSTATUS     0x0411U
#define NPM1300_BUCKSTATUS          0x0434U

// ADC
#define NPM1300_TASKVBATMEASURE                0x0500U
#define NPM1300_TASKNTCMEASURE                 0x0501U
#define NPM1300_TASKVSYSMEASURE                0x0503U
#define NPM1300_TASKIBATMEASURE                0x0506U
#define NPM1300_TASKVBUS7MEASURE               0x0507U
#define NPM1300_ADCNTCRSEL                     0x050AU
#define NPM1300_ADCNTCRSEL_HIZ                 0x0U
#define NPM1300_ADCNTCRSEL_10K                 0x1U
#define NPM1300_ADCIBATMEASSTATUS              0x0510U
#define NPM1300_ADCIBATMEASSTATUS_MODE_MASK    PBL_GENMASK(3, 2)
#define NPM1300_ADCIBATMEASSTATUS_MODE_DISCHRG 0x1U
#define NPM1300_ADCIBATMEASSTATUS_MODE_CHRG    0x3U
#define NPM1300_ADCVBATRESULTMSB               0x0511U
#define NPM1300_ADCNTCRESULTMSB                0x0512U
#define NPM1300_ADCVSYSRESULTMSB               0x0514U
#define NPM1300_ADCGP0RESULTLSBS               0x0515U
#define NPM1300_ADCGP0RESULTLSBS_VBAT_MASK     PBL_GENMASK(1, 0)
#define NPM1300_ADCGP0RESULTLSBS_NTC_MASK      PBL_GENMASK(3, 2)
#define NPM1300_ADCGP0RESULTLSBS_VSYS_MASK     PBL_GENMASK(7, 6)
#define NPM1300_ADCVBAT2RESULTMSB              0x0518U
#define NPM1300_ADCGP1RESULTLSBS               0x051AU
#define NPM1300_ADCGP1RESULTLSBS_VBAT2_MASK    PBL_GENMASK(5, 4)
#define NPM1300_ADCIBATMEASEN                  0x0524U

// GPIOS
#define NPM1300_GPIOMODE1            0x0601U
#define NPM1300_GPIOMODE2            0x0602U
#define NPM1300_GPIOMODE3            0x0603U
#define NPM1300_GPIOMODE_GPOIRQ      5U
#define NPM1300_GPIOMODE_OUTPUT_HIGH 8U
#define NPM1300_GPIOMODE_OUTPUT_LOW  9U
#define NPM1300_GPIOPUEN2            0x060CU
#define NPM1300_GPIOPUEN3            0x060DU
#define NPM1300_GPIOPUEN_DIS         0U
#define NPM1300_GPIOPUEN_EN          1U
#define NPM1300_GPIOOPENDRAIN1       0x0615U

// LDSW
#define NPM1300_TASKLDSW1SET             0x0800U
#define NPM1300_TASKLDSW1CLR             0x0801U
#define NPM1300_TASKLDSW2SET             0x0802U
#define NPM1300_TASKLDSW2CLR             0x0803U
#define NPM1300_LDSWSTATUS               0x0804U
#define NPM1300_LDSWSTATUS_LDSW2PWRUPLDO PBL_BIT(3)
#define NPM1300_LDSWCONFIG               0x0807U
#define NPM1300_LDSW1LDOSEL              0x0808U
#define NPM1300_LDSW2LDOSEL              0x0809U
#define NPM1300_LDSWLDOSEL_LDSW          0U
#define NPM1300_LDSWLDOSEL_LDO           1U
#define NPM1300_LDSW1VOUTSEL             0x080CU
#define NPM1300_LDSW2VOUTSEL             0x080DU
#define NPM1300_LDSWVOUTSEL_3V3          23U

// SHIP
#define NPM1300_TASKSHPHLDCFGSTROBE 0x0B01U
#define NPM1300_TASKENTERSHIPMODE   0x0B02U
#define NPM1300_SHPHLDCONFIG        0x0B04U
#define NPM1300_SHPHLDCONFIG_96MS   3U

// ERRLOG
#define NPM1300_SCRATCH0 0x0E01U
#define NPM1300_SCRATCH1 0x0E02U

#define NPM1300_BCHGISETDISCHARGEMSB_200MA  42U
#define NPM1300_BCHGISETDISCHARGELSB_200MA  0U
#define NPM1300_BCHGISETDISCHARGEMSB_1000MA 207U
#define NPM1300_BCHGISETDISCHARGELSB_1000MA 1U

#define NPM1300_BCHARGER_ADC_BITS_RESOLUTION    1023
#define NPM1300_BCHARGER_ADC_CALC_DISCHARGE_MUL 112
#define NPM1300_BCHARGER_ADC_CALC_DISCHARGE_DIV 100
#define NPM1300_BCHARGER_ADC_CALC_CHARGE_MUL    1250
#define NPM1300_BCHARGER_ADC_CALC_CHARGE_DIV    -1000
// Full scale voltage for battery voltage measurement
#define NPM1300_ADC_VFS_VBAT_MV      5000UL
#define NPM1300_VBUS_CURRENT_DIVISOR 100U

// 10-bit values split into an MSB register (bits 9:2) and LSB bits elsewhere
#define NPM1300_10BIT_MSB_MASK PBL_GENMASK(9, 2)
#define NPM1300_10BIT_LSB_MASK PBL_GENMASK(1, 0)

// Charge current is set in 2 mA steps, split into an MSB register and a LSB bit
#define NPM1300_BCHGISET_MSB_MASK PBL_GENMASK(8, 1)
#define NPM1300_BCHGISET_LSB_MASK PBL_BIT(0)

static bool dischg_limit_ma_set(uint32_t dischg_limit_ma);

static uint16_t prv_ntc_threshold_code(uint8_t celsius) {
  // Ref: PS v1.1 Section 6.2.5: K_NTCTEMP = round(1024 * R_T / (R_T + R_B))
  float t_k = (float)celsius + 273.15f;
  float exponent = (float)NPM1300_CONFIG.thermistor_beta * ((1.f / 298.15f) - (1.f / t_k));
  return (uint16_t)((1024.0f / (1.0f + exp(exponent))) + 0.5f);
}

void battery_init(void) {
}

static uint16_t prv_adc_raw(uint8_t msb, uint8_t lsbs, uint8_t lsb_mask) {
  return PBL_FIELD_PREP(NPM1300_10BIT_MSB_MASK, msb) |
         PBL_FIELD_PREP(NPM1300_10BIT_LSB_MASK, PBL_FIELD_GET(lsb_mask, lsbs));
}

static bool prv_read_register(uint16_t register_address, uint8_t *result) {
  i2c_use(I2C_NPM1300);
  uint8_t regad[2] = {register_address >> 8, register_address & 0xFF};
  bool rv = i2c_write_read_block(I2C_NPM1300, 2, regad, 1, result);
  i2c_release(I2C_NPM1300);
  return rv;
}

static bool prv_write_register(uint16_t register_address, uint8_t datum) {
  i2c_use(I2C_NPM1300);
  uint8_t d[3] = {register_address >> 8, register_address & 0xFF, datum};
  bool rv = i2c_write_block(I2C_NPM1300, 3, d);
  i2c_release(I2C_NPM1300);
  return rv;
}

// Anomaly 27 workaround: when switching BUCKn to SW control, if BUCKnNORMVOUT
// equals the VSET pin value (BUCKnVOUTSTATUS), quiescent current increases by
// 1mA. To avoid this, first set BUCKnNORMVOUT to a different value, switch to
// SW control, then set the desired voltage.
static bool prv_buck_set_sw_ctrl(uint16_t normvout_reg, uint16_t voutstatus_reg,
                                 uint8_t swctrlsel_bit, uint8_t desired_vout) {
  uint8_t voutstatus;
  if (!prv_read_register(voutstatus_reg, &voutstatus)) {
    return false;
  }

  // Ensure NORMVOUT differs from VOUTSTATUS before enabling SW control
  uint8_t initial_vout = (desired_vout != voutstatus) ? desired_vout : (desired_vout ^ 1);
  bool ok = prv_write_register(normvout_reg, initial_vout);

  // Read current SWCTRLSEL and set our bit
  uint8_t swctrlsel;
  if (!prv_read_register(NPM1300_BUCKSWCTRLSEL, &swctrlsel)) {
    return false;
  }
  ok &= prv_write_register(NPM1300_BUCKSWCTRLSEL, swctrlsel | swctrlsel_bit);

  // Now set the actual desired voltage
  if (initial_vout != desired_vout) {
    ok &= prv_write_register(normvout_reg, desired_vout);
  }

  return ok;
}

static void prv_handle_charge_state_change(void *null) {
  const bool is_charging = pmic_is_charging();
  const bool is_connected = pmic_is_usb_connected();
  PBL_LOG_DBG("nPM1300 Interrupt: Charging? %s Plugged? %s", is_charging ? "YES" : "NO",
              is_connected ? "YES" : "NO");

  if (is_connected && NPM1300_CONFIG.vbus_current_lim0 != 0) {
    bool ok = prv_write_register(NPM1300_VBUSINILIM0,
                                 NPM1300_CONFIG.vbus_current_lim0 / NPM1300_VBUS_CURRENT_DIVISOR);
    ok &= prv_write_register(NPM1300_TASKUPDATELIMSW, NPM1300_TASKUPDATELIMSW_EN);
    if (!ok) {
      PBL_LOG_ERR("config vbus limite0 failed");
    }
  }

  PebbleEvent event = {
    .type = PEBBLE_BATTERY_CONNECTION_EVENT,
    .battery_connection = {
      .is_connected = battery_is_usb_connected(),
    },
  };
  event_put(&event);
}

static void prv_clear_pending_interrupts() {
  prv_write_register(NPM1300_EVENTSBCHARGER1CLR, NPM1300_EVENTSBCHARGER1_CHGCOMPLETED);
  prv_write_register(NPM1300_EVENTSVBUSIN0CLR,
                     NPM1300_EVENTSVBUSIN0_VBUSDETECTED | NPM1300_EVENTSVBUSIN0_VBUSREMOVED);
}

static void prv_pmic_state_change_cb(void *null) {
  prv_clear_pending_interrupts();
  new_timer_start(s_debounce_charger_timer, CHARGER_DEBOUNCE_MS, prv_handle_charge_state_change,
                  NULL, 0 /*flags*/);
}

static void prv_npm1300_interrupt_handler(void) {
  system_task_add_callback_from_isr(prv_pmic_state_change_cb, NULL);
}

static void prv_configure_interrupts(void) {
  prv_clear_pending_interrupts();

  exti_configure_pin(BOARD_CONFIG_POWER.pmic_int, ExtiTrigger_Rising,
                     prv_npm1300_interrupt_handler);
  exti_enable(BOARD_CONFIG_POWER.pmic_int);
}

bool pmic_init(void) {
  bool ok = true;
  uint8_t val;

  s_debounce_charger_timer = new_timer_create();

  // TODO(NPM1300): This needs to be configurable at board level
#ifdef CONFIG_BOARD_ASTERIX
  // Anomaly 27: set BUCK1/BUCK2 to SW control with workaround
  ok &= prv_buck_set_sw_ctrl(NPM1300_BUCK1NORMVOUT, NPM1300_BUCK1VOUTSTATUS,
                             NPM1300_BUCKSWCTRLSEL_BUCK1, 8 /* 1.8V */);
  ok &= prv_buck_set_sw_ctrl(NPM1300_BUCK2NORMVOUT, NPM1300_BUCK2VOUTSTATUS,
                             NPM1300_BUCKSWCTRLSEL_BUCK2, 20 /* 3.0V */);

  if (!prv_read_register(NPM1300_LDSWSTATUS, &val)) {
    PBL_LOG_ERR("failed to read LDSWSTATUS");
    return false;
  }

  if ((val & NPM1300_LDSWSTATUS_LDSW2PWRUPLDO) == 0U) {
    ok &= prv_write_register(NPM1300_TASKLDSW2CLR, 0x01);
    ok &= prv_write_register(NPM1300_LDSW2VOUTSEL, 8 /* 1.8V */);
    ok &= prv_write_register(NPM1300_LDSW2LDOSEL, 1 /* LDO */);
    ok &= prv_write_register(NPM1300_TASKLDSW2SET, 0x01);
  } else {
    ok &= prv_write_register(NPM1300_LDSW2VOUTSEL, 8 /* 1.8V */);
  }
#endif

// FIXME(OBELIX,GETAFIX): Needs to be configurable at board level
#if defined(CONFIG_BOARD_OBELIX) || defined(CONFIG_BOARD_GETAFIX)
  // Anomaly 27: set BUCK1 to SW control with workaround, then disable it
  ok &= prv_buck_set_sw_ctrl(NPM1300_BUCK1NORMVOUT, NPM1300_BUCK1VOUTSTATUS,
                             NPM1300_BUCKSWCTRLSEL_BUCK1, 8 /* 1.8V */);
  ok &= prv_write_register(NPM1300_BUCK1ENACLR, 1);
  // enable 1.8V@LDO1
  ok &= prv_write_register(NPM1300_LDSW1LDOSEL, 1);  // LDO
  ok &= prv_write_register(NPM1300_LDSW1VOUTSEL, 8); // 1.8V
  ok &= prv_write_register(NPM1300_TASKLDSW1SET, 1); // enable
#endif

  ok &= prv_write_register(NPM1300_EVENTSBCHARGER1CLR, NPM1300_EVENTSBCHARGER1_CHGCOMPLETED);
  ok &= prv_write_register(NPM1300_INTENEVENTSBCHARGER1SET, NPM1300_EVENTSBCHARGER1_CHGCOMPLETED);
  ok &= prv_write_register(NPM1300_EVENTSVBUSIN0CLR,
                           NPM1300_EVENTSVBUSIN0_VBUSDETECTED | NPM1300_EVENTSVBUSIN0_VBUSREMOVED);
  ok &= prv_write_register(NPM1300_INTENEVENTSVBUSIN0SET,
                           NPM1300_EVENTSVBUSIN0_VBUSDETECTED | NPM1300_EVENTSVBUSIN0_VBUSREMOVED);
  ok &= prv_write_register(NPM1300_GPIOMODE1, NPM1300_GPIOMODE_GPOIRQ);
  ok &= prv_write_register(NPM1300_GPIOOPENDRAIN1, 0);

  ok &= prv_write_register(NPM1300_SHPHLDCONFIG, NPM1300_SHPHLDCONFIG_96MS);
  ok &= prv_write_register(NPM1300_TASKSHPHLDCFGSTROBE, 1);

  // automatic IBAT measurement after VBAT
  ok &= prv_write_register(NPM1300_ADCIBATMEASEN, 1);

  if ((NPM1300_CONFIG.chg_current_ma < 32U) || (NPM1300_CONFIG.chg_current_ma > 800U) ||
      (NPM1300_CONFIG.chg_current_ma % 2U != 0U)) {
    PBL_LOG_ERR("Invalid charge current: %d mA", NPM1300_CONFIG.chg_current_ma);
    return false;
  }

  ok &= prv_write_register(NPM1300_BCHGENABLECLR, 1);

  ok &= prv_write_register(NPM1300_TASKCLEARCHGERR, 1);
  ok &= prv_write_register(NPM1300_TASKRELEASEERROR, 1);

  // FIXME: this needs to be configurable at board level
#ifdef CONFIG_BOARD_OBELIX
  ok &= prv_write_register(NPM1300_ADCNTCRSEL, NPM1300_ADCNTCRSEL_10K);

  ok &= prv_write_register(NPM1300_BCHGVTERM, NPM1300_BCHGVTERM_4V35);
  ok &= prv_write_register(NPM1300_BCHGVTERMR, NPM1300_BCHGVTERMR_4V00);
#elif defined(CONFIG_BOARD_GETAFIX)
  ok &= prv_write_register(NPM1300_ADCNTCRSEL, NPM1300_ADCNTCRSEL_10K);

  ok &= prv_write_register(NPM1300_BCHGVTERM, NPM1300_BCHGVTERM_4V45);
  ok &= prv_write_register(NPM1300_BCHGVTERMR, NPM1300_BCHGVTERMR_4V00);
#elif defined(CONFIG_BOARD_ASTERIX)
  ok &= prv_write_register(NPM1300_ADCNTCRSEL, NPM1300_ADCNTCRSEL_10K);

  ok &= prv_write_register(NPM1300_BCHGVTERM, NPM1300_BCHGVTERM_4V20);
  ok &= prv_write_register(NPM1300_BCHGVTERMR, NPM1300_BCHGVTERMR_4V00);
#endif

  {
    uint16_t code = prv_ntc_threshold_code(NPM1300_CONFIG.ntc_hot_celsius);
    ok &= prv_write_register(NPM1300_NTCHOT, PBL_FIELD_GET(NPM1300_10BIT_MSB_MASK, code));
    ok &= prv_write_register(NPM1300_NTCHOTLSB, PBL_FIELD_GET(NPM1300_10BIT_LSB_MASK, code));
  }

  // FIXME: this needs to be configurable at board level
#ifdef CONFIG_BOARD_OBELIX
  // 3.3V @ LDO2
  ok &= prv_write_register(NPM1300_LDSW2LDOSEL, NPM1300_LDSWLDOSEL_LDO);
  ok &= prv_write_register(NPM1300_LDSW2VOUTSEL, NPM1300_LDSWVOUTSEL_3V3);
  ok &= prv_write_register(NPM1300_TASKLDSW2CLR, 1);
#elif defined(CONFIG_BOARD_GETAFIX)
  // LDSW2 (3.3V for PDM)
  ok &= prv_write_register(NPM1300_LDSW2LDOSEL, NPM1300_LDSWLDOSEL_LDSW);
  ok &= prv_write_register(NPM1300_TASKLDSW2CLR, 1);
#endif

  val = PBL_FIELD_GET(NPM1300_BCHGISET_MSB_MASK, NPM1300_CONFIG.chg_current_ma / 2U);
  ok &= prv_write_register(NPM1300_BCHGISETMSB, val);
  val = PBL_FIELD_GET(NPM1300_BCHGISET_LSB_MASK, NPM1300_CONFIG.chg_current_ma / 2U);
  ok &= prv_write_register(NPM1300_BCHGISETLSB, val);

  ok &= dischg_limit_ma_set(NPM1300_CONFIG.dischg_limit_ma);

  if (NPM1300_CONFIG.vbus_current_startup != 0) {
    ok &= prv_write_register(NPM1300_VBUSINILIMSTARTUP,
                             NPM1300_CONFIG.vbus_current_startup / NPM1300_VBUS_CURRENT_DIVISOR);
  }

  if (NPM1300_CONFIG.term_current_pct == 10U) {
    ok &= prv_write_register(NPM1300_BCHGITERMSEL, NPM1300_BCHGITERMSEL_SEL10);
  } else if (NPM1300_CONFIG.term_current_pct == 20U) {
    ok &= prv_write_register(NPM1300_BCHGITERMSEL, NPM1300_BCHGITERMSEL_SEL20);
  } else {
    PBL_LOG_ERR("Invalid termination current: %d", NPM1300_CONFIG.term_current_pct);
    return false;
  }

  ok &= prv_write_register(NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL0);
  ok &= prv_write_register(NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL1);
  ok &= prv_write_register(NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL2);
  ok &= prv_write_register(NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL3);

  ok &= prv_write_register(NPM1300_BCHGDEBUG, NPM1300_BCHGDEBUG_DISABLEBATTERYDETECT);

  ok &= prv_write_register(NPM1300_BCHGVBATLOWCHARGE, 1);

  prv_configure_interrupts();

  if (!ok) {
    PBL_LOG_ERR("one or more PMIC transactions failed");
  }

  return ok;
}

bool pmic_power_off(void) {
  // TODO: review implementation, see GH-238
  if (pmic_is_usb_connected()) {
    PBL_LOG_ERR("USB is connected, cannot power off");
    return false;
  }

  if (!prv_write_register(NPM1300_TASKENTERSHIPMODE, 1)) {
    PBL_LOG_ERR("Failed to enter ship mode");
    return false;
  }

  // Give enough time for the PMIC to fully power down (tPWRDN = 100ms).
  // We will die here, if we do not, return false and let upper layers handle
  // the shutdown failure.
  delay_us(100000);

  return false;
}

bool pmic_full_power_off(void) {
  return pmic_power_off();
}

uint16_t pmic_get_vsys(void) {
  if (!prv_write_register(NPM1300_EVENTSADCCLR, NPM1300_EVENTSADC_VSYSRDY)) {
    return 0;
  }
  if (!prv_write_register(NPM1300_TASKVSYSMEASURE, 1)) {
    return 0;
  }
  uint8_t reg = 0;
  uint32_t elapsed = 0;
  while ((reg & NPM1300_EVENTSADC_VSYSRDY) == 0) {
    if (elapsed >= ADC_POLL_TIMEOUT_MS) {
      return 0; // Timeout waiting for ADC
    }
    if (!prv_read_register(NPM1300_EVENTSADCCLR, &reg)) {
      return 0;
    }
    if ((reg & NPM1300_EVENTSADC_VSYSRDY) == 0) {
      psleep(ADC_POLL_DELAY_MS);
      elapsed += ADC_POLL_DELAY_MS;
    }
  }

  uint8_t vsys_msb;
  uint8_t lsbs;
  if (!prv_read_register(NPM1300_ADCVSYSRESULTMSB, &vsys_msb)) {
    return 0;
  }
  if (!prv_read_register(NPM1300_ADCGP0RESULTLSBS, &lsbs)) {
    return 0;
  }
  uint16_t vsys_raw = prv_adc_raw(vsys_msb, lsbs, NPM1300_ADCGP0RESULTLSBS_VSYS_MASK);
  uint32_t vsys = vsys_raw * 6375 / 1023;

  return vsys;
}

int battery_get_millivolts(void) {
  if (!prv_write_register(NPM1300_EVENTSADCCLR, NPM1300_EVENTSADC_VBATRDY)) {
    return 0;
  }
  if (!prv_write_register(NPM1300_TASKVBATMEASURE, 1)) {
    return 0;
  }
  uint8_t reg = 0;
  uint32_t elapsed = 0;
  while ((reg & NPM1300_EVENTSADC_VBATRDY) == 0) {
    if (elapsed >= ADC_POLL_TIMEOUT_MS) {
      return 0; // Timeout waiting for ADC
    }
    if (!prv_read_register(NPM1300_EVENTSADCCLR, &reg)) {
      return 0;
    }
    if ((reg & NPM1300_EVENTSADC_VBATRDY) == 0) {
      psleep(ADC_POLL_DELAY_MS);
      elapsed += ADC_POLL_DELAY_MS;
    }
  }

  uint8_t vbat_msb;
  uint8_t lsbs;
  if (!prv_read_register(NPM1300_ADCVBATRESULTMSB, &vbat_msb)) {
    return 0;
  }
  if (!prv_read_register(NPM1300_ADCGP0RESULTLSBS, &lsbs)) {
    return 0;
  }
  uint16_t vbat_raw = prv_adc_raw(vbat_msb, lsbs, NPM1300_ADCGP0RESULTLSBS_VBAT_MASK);
  uint32_t vbat = vbat_raw * 5000 / 1023;

  return vbat;
}

int battery_get_constants(BatteryConstants *constants) {
  uint8_t ibat_status;
  int32_t full_scale_ua;
  uint8_t msb;
  uint8_t lsb;
  uint16_t raw;
  uint8_t reg;

  // Obtain IBAT full scale
  if (!prv_read_register(NPM1300_ADCIBATMEASSTATUS, &ibat_status)) {
    return -1;
  }

  if (PBL_FIELD_GET(NPM1300_ADCIBATMEASSTATUS_MODE_MASK, ibat_status) ==
      NPM1300_ADCIBATMEASSTATUS_MODE_CHRG) {
    full_scale_ua =
        ((int32_t)NPM1300_CONFIG.chg_current_ma * 1000 * NPM1300_BCHARGER_ADC_CALC_CHARGE_MUL) /
        NPM1300_BCHARGER_ADC_CALC_CHARGE_DIV;
  } else {
    full_scale_ua = ((int32_t)s_dischg_limit_ma * 1000 * NPM1300_BCHARGER_ADC_CALC_DISCHARGE_MUL) /
                    NPM1300_BCHARGER_ADC_CALC_DISCHARGE_DIV;
  }

  // Clear the ADC ready events for VBAT, IBAT, and NTC
  if (!prv_write_register(NPM1300_EVENTSADCCLR, NPM1300_EVENTSADC_VBATRDY |
                                                    NPM1300_EVENTSADC_IBATRDY |
                                                    NPM1300_EVENTSADC_NTCRDY)) {
    return -1;
  }

  // Trigger VBAT+IBAT measurement (IBATMEASENABLE is enabled)
  if (!prv_write_register(NPM1300_TASKVBATMEASURE, 1)) {
    return -1;
  }

  // Trigger NTC measurement
  if (!prv_write_register(NPM1300_TASKNTCMEASURE, 1)) {
    return -1;
  }

  // Process the VBAT measurement
  reg = 0U;
  uint32_t elapsed = 0;
  while ((reg & NPM1300_EVENTSADC_VBATRDY) == 0U) {
    if (elapsed >= ADC_POLL_TIMEOUT_MS) {
      return -1; // Timeout waiting for VBAT ADC
    }
    if (!prv_read_register(NPM1300_EVENTSADCCLR, &reg)) {
      return -1;
    }
    if ((reg & NPM1300_EVENTSADC_VBATRDY) == 0U) {
      psleep(ADC_POLL_DELAY_MS);
      elapsed += ADC_POLL_DELAY_MS;
    }
  }

  if (!prv_read_register(NPM1300_ADCVBATRESULTMSB, &msb)) {
    return -1;
  }

  if (!prv_read_register(NPM1300_ADCGP0RESULTLSBS, &lsb)) {
    return -1;
  }

  raw = prv_adc_raw(msb, lsb, NPM1300_ADCGP0RESULTLSBS_VBAT_MASK);

  constants->v_mv = (int32_t)(raw * NPM1300_ADC_VFS_VBAT_MV) / NPM1300_BCHARGER_ADC_BITS_RESOLUTION;

  // Process the IBAT measurement
  elapsed = 0;
  while ((reg & NPM1300_EVENTSADC_IBATRDY) == 0U) {
    if (elapsed >= ADC_POLL_TIMEOUT_MS) {
      return -1; // Timeout waiting for IBAT ADC
    }
    if (!prv_read_register(NPM1300_EVENTSADCCLR, &reg)) {
      return -1;
    }
    if ((reg & NPM1300_EVENTSADC_IBATRDY) == 0U) {
      psleep(ADC_POLL_DELAY_MS);
      elapsed += ADC_POLL_DELAY_MS;
    }
  }

  if (!prv_read_register(NPM1300_ADCVBAT2RESULTMSB, &msb)) {
    return -1;
  }

  if (!prv_read_register(NPM1300_ADCGP1RESULTLSBS, &lsb)) {
    return -1;
  }

  raw = prv_adc_raw(msb, lsb, NPM1300_ADCGP1RESULTLSBS_VBAT2_MASK);

  constants->i_ua = ((int32_t)raw * full_scale_ua) / NPM1300_BCHARGER_ADC_BITS_RESOLUTION;

  // Process the NTC measurement
  elapsed = 0;
  while ((reg & NPM1300_EVENTSADC_NTCRDY) == 0U) {
    if (elapsed >= ADC_POLL_TIMEOUT_MS) {
      return -1; // Timeout waiting for NTC ADC
    }
    if (!prv_read_register(NPM1300_EVENTSADCCLR, &reg)) {
      return -1;
    }
    if ((reg & NPM1300_EVENTSADC_NTCRDY) == 0U) {
      psleep(ADC_POLL_DELAY_MS);
      elapsed += ADC_POLL_DELAY_MS;
    }
  }

  if (!prv_read_register(NPM1300_ADCNTCRESULTMSB, &msb)) {
    return -1;
  }

  if (!prv_read_register(NPM1300_ADCGP0RESULTLSBS, &lsb)) {
    return -1;
  }

  raw = prv_adc_raw(msb, lsb, NPM1300_ADCGP0RESULTLSBS_NTC_MASK);

  // Ref: PS v1.2 Section 7.1.4: Battery temperature (Kelvin)
  float log_result = logf((1024.f / (float)raw) - 1.0f);
  float inv_temp_k = (1.f / 298.15f) - (log_result / (float)NPM1300_CONFIG.thermistor_beta);

  constants->t_mc = (int32_t)(1000.0f * ((1.f / inv_temp_k) - 273.15f));

  return 0;
}

bool pmic_set_charger_state(bool enable) {
  return prv_write_register(enable ? NPM1300_BCHGENABLESET : NPM1300_BCHGENABLECLR, 1);
}

void battery_set_charge_enable(bool charging_enabled) {
  pmic_set_charger_state(charging_enabled);
}

void battery_set_fast_charge(bool fast_charge_enabled) {
  /* the PMIC handles this for us */
}

bool pmic_is_charging(void) {
  uint8_t status;
  if (!prv_read_register(NPM1300_BCHGCHARGESTATUS, &status)) {
    return false;
  }

  return (status &
          (NPM1300_BCHGCHARGESTATUS_TRICKLECHARGE | NPM1300_BCHGCHARGESTATUS_CONSTANTCURRENT |
           NPM1300_BCHGCHARGESTATUS_CONSTANTVOLTAGE)) != 0;
}

bool battery_charge_controller_thinks_we_are_charging_impl(void) {
  return pmic_is_charging();
}

bool pmic_is_usb_connected(void) {
  uint8_t status;
  if (!prv_read_register(NPM1300_VBUSINSTATUS, &status)) {
    return false;
  }

  return (status & NPM1300_VBUSINSTATUS_PRESENT) != 0;
}

bool battery_is_usb_connected_impl(void) {
  return pmic_is_usb_connected();
}

void pmic_read_chip_info(uint8_t *chip_id, uint8_t *chip_revision, uint8_t *buck1_vset) {
}

bool pmic_enable_battery_measure(void) {
  return true;
}

bool pmic_disable_battery_measure(void) {
  return true;
}

void set_ldo3_power_state(bool enabled) {
}

void set_4V5_power_state(bool enabled) {
}

void set_6V6_power_state(bool enabled) {
}

int battery_charge_status_get(BatteryChargeStatus *status) {
  uint8_t chg_status;

  if (!prv_read_register(NPM1300_BCHGCHARGESTATUS, &chg_status)) {
    return -1;
  }

  switch (chg_status &
          (NPM1300_BCHGCHARGESTATUS_COMPLETED | NPM1300_BCHGCHARGESTATUS_TRICKLECHARGE |
           NPM1300_BCHGCHARGESTATUS_CONSTANTCURRENT | NPM1300_BCHGCHARGESTATUS_CONSTANTVOLTAGE)) {
    case NPM1300_BCHGCHARGESTATUS_COMPLETED:
      *status = BatteryChargeStatusComplete;
      break;
    case NPM1300_BCHGCHARGESTATUS_TRICKLECHARGE:
      *status = BatteryChargeStatusTrickle;
      break;
    case NPM1300_BCHGCHARGESTATUS_CONSTANTCURRENT:
      *status = BatteryChargeStatusCC;
      break;
    case NPM1300_BCHGCHARGESTATUS_CONSTANTVOLTAGE:
      *status = BatteryChargeStatusCV;
      break;
    default:
      *status = BatteryChargeStatusUnknown;
      break;
  }

  return 0;
}

static bool gpio_set(Npm1300GpioId_t id, bool is_high) {
  bool rv = false;
  switch (id) {
    case Npm1300_Gpio2:
      rv = prv_write_register(NPM1300_GPIOMODE2,
                              is_high ? NPM1300_GPIOMODE_OUTPUT_HIGH : NPM1300_GPIOMODE_OUTPUT_LOW);
      rv &= prv_write_register(NPM1300_GPIOPUEN2,
                               is_high ? NPM1300_GPIOPUEN_EN : NPM1300_GPIOPUEN_DIS);
      break;
    case Npm1300_Gpio3: {
      rv = prv_write_register(NPM1300_GPIOMODE3,
                              is_high ? NPM1300_GPIOMODE_OUTPUT_HIGH : NPM1300_GPIOMODE_OUTPUT_LOW);
      rv &= prv_write_register(NPM1300_GPIOPUEN3,
                               is_high ? NPM1300_GPIOPUEN_EN : NPM1300_GPIOPUEN_DIS);
      break;
    }
    default:
      break;
  }

  return rv;
}

static bool ldo2_set_enabled(bool enabled) {
  if (enabled) {
    return prv_write_register(NPM1300_TASKLDSW2SET, 1);
  } else {
    return prv_write_register(NPM1300_TASKLDSW2CLR, 1);
  }
}

static bool dischg_limit_ma_set(uint32_t dischg_limit_ma) {
  bool ret;

  if (s_dischg_limit_ma == dischg_limit_ma) {
    return true;
  }

  if (dischg_limit_ma == 200) {
    ret = prv_write_register(NPM1300_BCHGISETDISCHARGEMSB, NPM1300_BCHGISETDISCHARGEMSB_200MA);
    if (!ret) {
      return ret;
    }

    ret = prv_write_register(NPM1300_BCHGISETDISCHARGELSB, NPM1300_BCHGISETDISCHARGELSB_200MA);
    if (!ret) {
      return ret;
    }
  } else if (dischg_limit_ma == 1000) {
    ret = prv_write_register(NPM1300_BCHGISETDISCHARGEMSB, NPM1300_BCHGISETDISCHARGEMSB_1000MA);
    if (!ret) {
      return ret;
    }

    ret = prv_write_register(NPM1300_BCHGISETDISCHARGELSB, NPM1300_BCHGISETDISCHARGELSB_1000MA);
    if (!ret) {
      return ret;
    }
  } else {
    PBL_LOG_ERR("Invalid discharge limit: %" PRIu32 " mA", dischg_limit_ma);
    return false;
  }

  s_dischg_limit_ma = dischg_limit_ma;

  return true;
}

Npm1300Ops_t NPM1300_OPS = {
  .gpio_set = gpio_set,
  .ldo2_set_enabled = ldo2_set_enabled,
  .dischg_limit_ma_set = dischg_limit_ma_set,
};

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>

static int prv_cmd_pmic_regs(const struct pbl_shell *sh, size_t argc, char **argv) {
#define SAY(x)                                                   \
  do {                                                           \
    uint8_t reg;                                                 \
    int rv = prv_read_register(NPM1300_##x, &reg);               \
    pbl_shell_print(sh, "PMIC: " #x " = %02x (rv %d)", reg, rv); \
  } while (0)
  SAY(SCRATCH0);
  SAY(SCRATCH1);
  SAY(BUCK1NORMVOUT);
  SAY(BUCK2NORMVOUT);
  SAY(BUCKSTATUS);
  SAY(VBUSINSTATUS);
  SAY(BCHGCHARGESTATUS);
  SAY(BCHGERRREASON);
#undef SAY
  pbl_shell_print(sh, "PMIC: Vsys = %d mV", pmic_get_vsys());
  pbl_shell_print(sh, "PMIC: Vbat = %d mV", battery_get_millivolts());
  return 0;
}

static const struct pbl_shell_cmd sub_pmic[] = {
  PBL_SHELL_CMD(regs, NULL, "Dump the main registers", prv_cmd_pmic_regs),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(pmic, sub_pmic, "PMIC", NULL);
#endif
