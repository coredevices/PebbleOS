/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/* Because nPM1300 also has the battery monitor, we implement both the
 * pmic_* and the battery_* API here.  */

#include <errno.h>
#include <inttypes.h>
#include <math.h>

#include <pbl/drivers/battery.h>
#include <pbl/drivers/exti.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/pmic.h>
#include <pbl/drivers/pmic/npm1300.h>
#include <pbl/logging/logging.h>
#include <pbl/services/system_task.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <board/board.h>
#include <kernel/events.h>
#include <kernel/util/delay.h>
#include <kernel/util/sleep.h>

PBL_LOG_MODULE_DEFINE(driver_pmic_npm1300, CONFIG_DRIVER_PMIC_LOG_LEVEL);

#define CHARGER_DEBOUNCE_MS 400
#define ADC_POLL_DELAY_MS   5   // Delay between ADC poll iterations to reduce I2C traffic
#define ADC_POLL_TIMEOUT_MS 100 // Max time to wait for ADC measurement

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
#define NPM1300_BCHGVTERMR                       0x030DU
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
#define NPM1300_BUCK1NORMVOUT 0x0408U
#define NPM1300_BUCK2NORMVOUT 0x040AU
#define NPM1300_BUCKSTATUS    0x0434U

// ADC
#define NPM1300_TASKVBATMEASURE                0x0500U
#define NPM1300_TASKNTCMEASURE                 0x0501U
#define NPM1300_TASKVSYSMEASURE                0x0503U
#define NPM1300_TASKIBATMEASURE                0x0506U
#define NPM1300_TASKVBUS7MEASURE               0x0507U
#define NPM1300_ADCNTCRSEL                     0x050AU
#define NPM1300_ADCNTCRSEL_10K                 0x1U
#define NPM1300_ADCNTCRSEL_47K                 0x2U
#define NPM1300_ADCNTCRSEL_100K                0x3U
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
#define NPM1300_GPIOMODE1       0x0601U
#define NPM1300_GPIOMODE_GPOIRQ 5U
#define NPM1300_GPIOOPENDRAIN1  0x0615U

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

// Charge termination voltage codes: 3.50-3.65 V and 4.00-4.45 V, in 50 mV steps
#define NPM1300_BCHGVTERM_STEP_MV     50U
#define NPM1300_BCHGVTERM_LOW_MIN_MV  3500U
#define NPM1300_BCHGVTERM_LOW_MAX_MV  3650U
#define NPM1300_BCHGVTERM_HIGH_MIN_MV 4000U
#define NPM1300_BCHGVTERM_HIGH_MAX_MV 4450U
#define NPM1300_BCHGVTERM_HIGH_CODE   4U

// 10-bit values split into an MSB register (bits 9:2) and LSB bits elsewhere
#define NPM1300_10BIT_MSB_MASK PBL_GENMASK(9, 2)
#define NPM1300_10BIT_LSB_MASK PBL_GENMASK(1, 0)

// Charge current is set in 2 mA steps, split into an MSB register and a LSB bit
#define NPM1300_BCHGISET_MSB_MASK PBL_GENMASK(8, 1)
#define NPM1300_BCHGISET_LSB_MASK PBL_BIT(0)

static uint16_t prv_ntc_threshold_code(const Npm1300Config *cfg, uint8_t celsius) {
  // Ref: PS v1.1 Section 6.2.5: K_NTCTEMP = round(1024 * R_T / (R_T + R_B))
  float t_k = (float)celsius + 273.15f;
  float exponent = (float)cfg->thermistor_beta * ((1.f / 298.15f) - (1.f / t_k));
  return (uint16_t)((1024.0f / (1.0f + exp(exponent))) + 0.5f);
}

static bool prv_vterm_code(uint16_t mv, uint8_t *code) {
  if ((mv % NPM1300_BCHGVTERM_STEP_MV) != 0U) {
    return false;
  }

  if ((mv >= NPM1300_BCHGVTERM_LOW_MIN_MV) && (mv <= NPM1300_BCHGVTERM_LOW_MAX_MV)) {
    *code = (mv - NPM1300_BCHGVTERM_LOW_MIN_MV) / NPM1300_BCHGVTERM_STEP_MV;
    return true;
  }

  if ((mv >= NPM1300_BCHGVTERM_HIGH_MIN_MV) && (mv <= NPM1300_BCHGVTERM_HIGH_MAX_MV)) {
    *code = NPM1300_BCHGVTERM_HIGH_CODE +
            (mv - NPM1300_BCHGVTERM_HIGH_MIN_MV) / NPM1300_BCHGVTERM_STEP_MV;
    return true;
  }

  return false;
}

static bool prv_ntc_sel(uint8_t kohm, uint8_t *sel) {
  switch (kohm) {
    case 10:
      *sel = NPM1300_ADCNTCRSEL_10K;
      return true;
    case 47:
      *sel = NPM1300_ADCNTCRSEL_47K;
      return true;
    case 100:
      *sel = NPM1300_ADCNTCRSEL_100K;
      return true;
    default:
      return false;
  }
}

void battery_init(void) {
}

static uint16_t prv_adc_raw(uint8_t msb, uint8_t lsbs, uint8_t lsb_mask) {
  return PBL_FIELD_PREP(NPM1300_10BIT_MSB_MASK, msb) |
         PBL_FIELD_PREP(NPM1300_10BIT_LSB_MASK, PBL_FIELD_GET(lsb_mask, lsbs));
}

void pbl_npm1300_lock(const struct pbl_npm1300 *pmic) {
  pbl_mutex_lock(&pmic->state->lock, PBL_FOREVER);
}

void pbl_npm1300_unlock(const struct pbl_npm1300 *pmic) {
  pbl_mutex_unlock(&pmic->state->lock);
}

bool pbl_npm1300_read(const struct pbl_npm1300 *pmic, uint16_t reg, uint8_t *val) {
  uint8_t regad[2] = {reg >> 8, reg & 0xFF};

  pbl_npm1300_lock(pmic);
  pbl_i2c_use(&pmic->i2c);
  bool rv = pbl_i2c_write_read_block(&pmic->i2c, sizeof(regad), regad, 1, val);
  pbl_i2c_release(&pmic->i2c);
  pbl_npm1300_unlock(pmic);

  return rv;
}

bool pbl_npm1300_write(const struct pbl_npm1300 *pmic, uint16_t reg, uint8_t val) {
  uint8_t d[3] = {reg >> 8, reg & 0xFF, val};

  pbl_npm1300_lock(pmic);
  pbl_i2c_use(&pmic->i2c);
  bool rv = pbl_i2c_write_block(&pmic->i2c, sizeof(d), d);
  pbl_i2c_release(&pmic->i2c);
  pbl_npm1300_unlock(pmic);

  return rv;
}

static bool prv_read_register(uint16_t reg, uint8_t *val) {
  return pbl_npm1300_read(NPM1300, reg, val);
}

static bool prv_write_register(uint16_t reg, uint8_t val) {
  return pbl_npm1300_write(NPM1300, reg, val);
}

static void prv_handle_charge_state_change(void *null) {
  const bool is_charging = pmic_is_charging();
  const bool is_connected = pmic_is_usb_connected();
  PBL_LOG_DBG("nPM1300 Interrupt: Charging? %s Plugged? %s", is_charging ? "YES" : "NO",
              is_connected ? "YES" : "NO");

  if (is_connected && NPM1300->cfg->vbus_current_lim0 != 0) {
    bool ok = prv_write_register(NPM1300_VBUSINILIM0,
                                 NPM1300->cfg->vbus_current_lim0 / NPM1300_VBUS_CURRENT_DIVISOR);
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
  new_timer_start(NPM1300->state->debounce_charger_timer, CHARGER_DEBOUNCE_MS,
                  prv_handle_charge_state_change, NULL, 0 /*flags*/);
}

static void prv_npm1300_interrupt_handler(void) {
  system_task_add_callback_from_isr(prv_pmic_state_change_cb, NULL);
}

int pbl_npm1300_init(const struct pbl_device *dev) {
  const struct pbl_npm1300 *pmic = container_of(dev, const struct pbl_npm1300, dev);
  const Npm1300Config *cfg = pmic->cfg;
  uint8_t vterm;
  uint8_t vtermr;
  uint8_t ntcsel;
  uint8_t val;
  bool ok = true;

  pbl_mutex_init(&pmic->state->lock);
  pmic->state->dischg_limit_ma = 0;
  pmic->state->gpio_pullup_mask = 0;
  pmic->state->debounce_charger_timer = new_timer_create();

  if ((cfg->chg_current_ma < 32U) || (cfg->chg_current_ma > 800U) ||
      (cfg->chg_current_ma % 2U != 0U)) {
    PBL_LOG_ERR("Invalid charge current: %d mA", cfg->chg_current_ma);
    return -EINVAL;
  }

  if ((cfg->term_current_pct != 10U) && (cfg->term_current_pct != 20U)) {
    PBL_LOG_ERR("Invalid termination current: %d", cfg->term_current_pct);
    return -EINVAL;
  }

  if (!prv_vterm_code(cfg->vterm_mv, &vterm) || !prv_vterm_code(cfg->vterm_reduced_mv, &vtermr)) {
    PBL_LOG_ERR("Invalid termination voltage");
    return -EINVAL;
  }

  if (!prv_ntc_sel(cfg->ntc_kohm, &ntcsel)) {
    PBL_LOG_ERR("Invalid NTC resistance: %d kOhm", cfg->ntc_kohm);
    return -EINVAL;
  }

  ok &= pbl_npm1300_write(pmic, NPM1300_EVENTSBCHARGER1CLR, NPM1300_EVENTSBCHARGER1_CHGCOMPLETED);
  ok &= pbl_npm1300_write(pmic, NPM1300_INTENEVENTSBCHARGER1SET,
                          NPM1300_EVENTSBCHARGER1_CHGCOMPLETED);
  ok &= pbl_npm1300_write(pmic, NPM1300_EVENTSVBUSIN0CLR,
                          NPM1300_EVENTSVBUSIN0_VBUSDETECTED | NPM1300_EVENTSVBUSIN0_VBUSREMOVED);
  ok &= pbl_npm1300_write(pmic, NPM1300_INTENEVENTSVBUSIN0SET,
                          NPM1300_EVENTSVBUSIN0_VBUSDETECTED | NPM1300_EVENTSVBUSIN0_VBUSREMOVED);
  ok &= pbl_npm1300_write(pmic, NPM1300_GPIOMODE1, NPM1300_GPIOMODE_GPOIRQ);
  ok &= pbl_npm1300_write(pmic, NPM1300_GPIOOPENDRAIN1, 0);

  ok &= pbl_npm1300_write(pmic, NPM1300_SHPHLDCONFIG, NPM1300_SHPHLDCONFIG_96MS);
  ok &= pbl_npm1300_write(pmic, NPM1300_TASKSHPHLDCFGSTROBE, 1);

  // automatic IBAT measurement after VBAT
  ok &= pbl_npm1300_write(pmic, NPM1300_ADCIBATMEASEN, 1);

  ok &= pbl_npm1300_write(pmic, NPM1300_BCHGENABLECLR, 1);

  ok &= pbl_npm1300_write(pmic, NPM1300_TASKCLEARCHGERR, 1);
  ok &= pbl_npm1300_write(pmic, NPM1300_TASKRELEASEERROR, 1);

  ok &= pbl_npm1300_write(pmic, NPM1300_ADCNTCRSEL, ntcsel);
  ok &= pbl_npm1300_write(pmic, NPM1300_BCHGVTERM, vterm);
  ok &= pbl_npm1300_write(pmic, NPM1300_BCHGVTERMR, vtermr);

  uint16_t code = prv_ntc_threshold_code(cfg, cfg->ntc_hot_celsius);
  ok &= pbl_npm1300_write(pmic, NPM1300_NTCHOT, PBL_FIELD_GET(NPM1300_10BIT_MSB_MASK, code));
  ok &= pbl_npm1300_write(pmic, NPM1300_NTCHOTLSB, PBL_FIELD_GET(NPM1300_10BIT_LSB_MASK, code));

  val = PBL_FIELD_GET(NPM1300_BCHGISET_MSB_MASK, cfg->chg_current_ma / 2U);
  ok &= pbl_npm1300_write(pmic, NPM1300_BCHGISETMSB, val);
  val = PBL_FIELD_GET(NPM1300_BCHGISET_LSB_MASK, cfg->chg_current_ma / 2U);
  ok &= pbl_npm1300_write(pmic, NPM1300_BCHGISETLSB, val);

  ok &= pbl_npm1300_set_dischg_limit_ma(pmic, cfg->dischg_limit_ma);

  if (cfg->vbus_current_startup != 0) {
    ok &= pbl_npm1300_write(pmic, NPM1300_VBUSINILIMSTARTUP,
                            cfg->vbus_current_startup / NPM1300_VBUS_CURRENT_DIVISOR);
  }

  ok &= pbl_npm1300_write(
      pmic, NPM1300_BCHGITERMSEL,
      (cfg->term_current_pct == 10U) ? NPM1300_BCHGITERMSEL_SEL10 : NPM1300_BCHGITERMSEL_SEL20);

  ok &= pbl_npm1300_write(pmic, NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL0);
  ok &= pbl_npm1300_write(pmic, NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL1);
  ok &= pbl_npm1300_write(pmic, NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL2);
  ok &= pbl_npm1300_write(pmic, NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL3);

  ok &= pbl_npm1300_write(pmic, NPM1300_BCHGDEBUG, NPM1300_BCHGDEBUG_DISABLEBATTERYDETECT);

  ok &= pbl_npm1300_write(pmic, NPM1300_BCHGVBATLOWCHARGE, 1);

  if (!ok) {
    PBL_LOG_ERR("one or more PMIC transactions failed");
    return -EIO;
  }

  prv_clear_pending_interrupts();
  exti_configure_pin(pmic->irq, ExtiTrigger_Rising, prv_npm1300_interrupt_handler);
  exti_enable(pmic->irq);

  // The GPIO port and the board's rails
  return (pbl_device_init_children(dev) == 0) ? 0 : -EIO;
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
        ((int32_t)NPM1300->cfg->chg_current_ma * 1000 * NPM1300_BCHARGER_ADC_CALC_CHARGE_MUL) /
        NPM1300_BCHARGER_ADC_CALC_CHARGE_DIV;
  } else {
    full_scale_ua = ((int32_t)NPM1300->state->dischg_limit_ma * 1000 *
                     NPM1300_BCHARGER_ADC_CALC_DISCHARGE_MUL) /
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
  float inv_temp_k = (1.f / 298.15f) - (log_result / (float)NPM1300->cfg->thermistor_beta);

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

bool pbl_npm1300_set_dischg_limit_ma(const struct pbl_npm1300 *pmic, uint32_t ma) {
  uint8_t msb;
  uint8_t lsb;
  bool ok = true;

  if (ma == 200U) {
    msb = NPM1300_BCHGISETDISCHARGEMSB_200MA;
    lsb = NPM1300_BCHGISETDISCHARGELSB_200MA;
  } else if (ma == 1000U) {
    msb = NPM1300_BCHGISETDISCHARGEMSB_1000MA;
    lsb = NPM1300_BCHGISETDISCHARGELSB_1000MA;
  } else {
    PBL_LOG_ERR("Invalid discharge limit: %" PRIu32 " mA", ma);
    return false;
  }

  pbl_npm1300_lock(pmic);

  if (pmic->state->dischg_limit_ma != ma) {
    ok = pbl_npm1300_write(pmic, NPM1300_BCHGISETDISCHARGEMSB, msb) &&
         pbl_npm1300_write(pmic, NPM1300_BCHGISETDISCHARGELSB, lsb);
    if (ok) {
      pmic->state->dischg_limit_ma = ma;
    }
  }

  pbl_npm1300_unlock(pmic);

  return ok;
}

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
