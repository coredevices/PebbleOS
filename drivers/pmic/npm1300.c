/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/* Because nPM1300 also has the battery monitor, we implement both the
 * pmic_* and the battery_* API here.  */

#include <errno.h>
#include <inttypes.h>
#include <math.h>

#include <pbl/drivers/battery.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/pmic.h>
#include <pbl/drivers/pmic/npm1300.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/services/system_task.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <board/board.h>
#include <kernel/events.h>

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

static int prv_vterm_code(uint16_t mv, uint8_t *code) {
  if ((mv % NPM1300_BCHGVTERM_STEP_MV) != 0U) {
    return -EINVAL;
  }

  if ((mv >= NPM1300_BCHGVTERM_LOW_MIN_MV) && (mv <= NPM1300_BCHGVTERM_LOW_MAX_MV)) {
    *code = (mv - NPM1300_BCHGVTERM_LOW_MIN_MV) / NPM1300_BCHGVTERM_STEP_MV;
    return 0;
  }

  if ((mv >= NPM1300_BCHGVTERM_HIGH_MIN_MV) && (mv <= NPM1300_BCHGVTERM_HIGH_MAX_MV)) {
    *code = NPM1300_BCHGVTERM_HIGH_CODE +
            (mv - NPM1300_BCHGVTERM_HIGH_MIN_MV) / NPM1300_BCHGVTERM_STEP_MV;
    return 0;
  }

  return -EINVAL;
}

static int prv_ntc_sel(uint8_t kohm, uint8_t *sel) {
  switch (kohm) {
    case 10:
      *sel = NPM1300_ADCNTCRSEL_10K;
      return 0;
    case 47:
      *sel = NPM1300_ADCNTCRSEL_47K;
      return 0;
    case 100:
      *sel = NPM1300_ADCNTCRSEL_100K;
      return 0;
    default:
      return -EINVAL;
  }
}

void battery_init(void) {
}

static uint16_t prv_adc_raw(uint8_t msb, uint8_t lsbs, uint8_t lsb_mask) {
  return PBL_FIELD_PREP(NPM1300_10BIT_MSB_MASK, msb) |
         PBL_FIELD_PREP(NPM1300_10BIT_LSB_MASK, PBL_FIELD_GET(lsb_mask, lsbs));
}

struct prv_reg_val {
  uint16_t reg;
  uint8_t val;
};

static int prv_write_seq(const struct pbl_npm1300 *pmic, const struct prv_reg_val *seq,
                         size_t len) {
  int ret;

  for (size_t i = 0U; i < len; i++) {
    ret = pbl_npm1300_write(pmic, seq[i].reg, seq[i].val);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

static const struct pbl_npm1300 *prv_pmic(void) {
  return pbl_npm1300_from_cell(&NPM1300_CHARGER->dev);
}

static int prv_read_register(uint16_t reg, uint8_t *val) {
  return pbl_npm1300_read(prv_pmic(), reg, val);
}

static int prv_write_register(uint16_t reg, uint8_t val) {
  return pbl_npm1300_write(prv_pmic(), reg, val);
}

static int prv_set_vbus_current_lim0(void) {
  int ret;

  ret = prv_write_register(NPM1300_VBUSINILIM0,
                           NPM1300_CHARGER->cfg->vbus_current_lim0 / NPM1300_VBUS_CURRENT_DIVISOR);
  if (ret != 0) {
    return ret;
  }

  return prv_write_register(NPM1300_TASKUPDATELIMSW, NPM1300_TASKUPDATELIMSW_EN);
}

static void prv_handle_charge_state_change(void *null) {
  const bool is_charging = pmic_is_charging();
  const bool is_connected = pmic_is_usb_connected();
  PBL_LOG_DBG("nPM1300 Interrupt: Charging? %s Plugged? %s", is_charging ? "YES" : "NO",
              is_connected ? "YES" : "NO");

  if (is_connected && NPM1300_CHARGER->cfg->vbus_current_lim0 != 0) {
    int ret = prv_set_vbus_current_lim0();
    if (ret != 0) {
      PBL_LOG_ERR("VBUS current limit setup failed (%d)", ret);
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

static int prv_clear_pending_interrupts(void) {
  int ret;

  ret = prv_write_register(NPM1300_EVENTSBCHARGER1CLR, NPM1300_EVENTSBCHARGER1_CHGCOMPLETED);
  if (ret != 0) {
    return ret;
  }

  return prv_write_register(NPM1300_EVENTSVBUSIN0CLR,
                            NPM1300_EVENTSVBUSIN0_VBUSDETECTED | NPM1300_EVENTSVBUSIN0_VBUSREMOVED);
}

static void prv_pmic_state_change_cb(void *null) {
  int ret = prv_clear_pending_interrupts();
  if (ret != 0) {
    PBL_LOG_ERR("Interrupt clear failed (%d)", ret);
  }

  new_timer_start(NPM1300_CHARGER->state->debounce_timer, CHARGER_DEBOUNCE_MS,
                  prv_handle_charge_state_change, nullptr, 0 /*flags*/);
}

static void prv_npm1300_interrupt_handler(void *arg) {
  system_task_add_callback_from_isr(prv_pmic_state_change_cb, nullptr);
}

int pbl_npm1300_charger_init(const struct pbl_device *dev) {
  const struct pbl_npm1300_charger *charger =
      container_of(dev, const struct pbl_npm1300_charger, dev);
  const struct pbl_npm1300 *pmic = pbl_npm1300_from_cell(dev);
  const Npm1300Config *cfg = charger->cfg;
  uint16_t ntc_hot;
  uint8_t vterm;
  uint8_t vtermr;
  uint8_t ntcsel;
  int ret;

  charger->state->dischg_limit_ma = 0;
  charger->state->debounce_timer = new_timer_create();

  if ((cfg->chg_current_ma < 32U) || (cfg->chg_current_ma > 800U) ||
      (cfg->chg_current_ma % 2U != 0U)) {
    PBL_LOG_ERR("Invalid charge current: %d mA", cfg->chg_current_ma);
    return -EINVAL;
  }

  if ((cfg->term_current_pct != 10U) && (cfg->term_current_pct != 20U)) {
    PBL_LOG_ERR("Invalid termination current: %d", cfg->term_current_pct);
    return -EINVAL;
  }

  ret = prv_vterm_code(cfg->vterm_mv, &vterm);
  if (ret != 0) {
    PBL_LOG_ERR("Invalid termination voltage: %d mV", cfg->vterm_mv);
    return ret;
  }

  ret = prv_vterm_code(cfg->vterm_reduced_mv, &vtermr);
  if (ret != 0) {
    PBL_LOG_ERR("Invalid reduced termination voltage: %d mV", cfg->vterm_reduced_mv);
    return ret;
  }

  ret = prv_ntc_sel(cfg->ntc_kohm, &ntcsel);
  if (ret != 0) {
    PBL_LOG_ERR("Invalid NTC resistance: %d kOhm", cfg->ntc_kohm);
    return ret;
  }

  ntc_hot = prv_ntc_threshold_code(cfg, cfg->ntc_hot_celsius);

  const struct prv_reg_val setup[] = {
    {NPM1300_EVENTSBCHARGER1CLR, NPM1300_EVENTSBCHARGER1_CHGCOMPLETED},
    {NPM1300_INTENEVENTSBCHARGER1SET, NPM1300_EVENTSBCHARGER1_CHGCOMPLETED},
    {NPM1300_EVENTSVBUSIN0CLR,
     NPM1300_EVENTSVBUSIN0_VBUSDETECTED | NPM1300_EVENTSVBUSIN0_VBUSREMOVED},
    {NPM1300_INTENEVENTSVBUSIN0SET,
     NPM1300_EVENTSVBUSIN0_VBUSDETECTED | NPM1300_EVENTSVBUSIN0_VBUSREMOVED},
    {NPM1300_GPIOMODE1, NPM1300_GPIOMODE_GPOIRQ},
    {NPM1300_GPIOOPENDRAIN1, 0},
    {NPM1300_SHPHLDCONFIG, NPM1300_SHPHLDCONFIG_96MS},
    {NPM1300_TASKSHPHLDCFGSTROBE, 1},
    // Automatic IBAT measurement after VBAT
    {NPM1300_ADCIBATMEASEN, 1},
    {NPM1300_BCHGENABLECLR, 1},
    {NPM1300_TASKCLEARCHGERR, 1},
    {NPM1300_TASKRELEASEERROR, 1},
    {NPM1300_ADCNTCRSEL, ntcsel},
    {NPM1300_BCHGVTERM, vterm},
    {NPM1300_BCHGVTERMR, vtermr},
    {NPM1300_NTCHOT, PBL_FIELD_GET(NPM1300_10BIT_MSB_MASK, ntc_hot)},
    {NPM1300_NTCHOTLSB, PBL_FIELD_GET(NPM1300_10BIT_LSB_MASK, ntc_hot)},
    {NPM1300_BCHGISETMSB, PBL_FIELD_GET(NPM1300_BCHGISET_MSB_MASK, cfg->chg_current_ma / 2U)},
    {NPM1300_BCHGISETLSB, PBL_FIELD_GET(NPM1300_BCHGISET_LSB_MASK, cfg->chg_current_ma / 2U)},
  };

  ret = prv_write_seq(pmic, setup, ARRAY_LENGTH(setup));
  if (ret != 0) {
    PBL_LOG_ERR("Setup failed (%d)", ret);
    return ret;
  }

  ret = pbl_npm1300_charger_set_dischg_limit_ma(charger, cfg->dischg_limit_ma);
  if (ret != 0) {
    return ret;
  }

  if (cfg->vbus_current_startup != 0) {
    ret = pbl_npm1300_write(pmic, NPM1300_VBUSINILIMSTARTUP,
                            cfg->vbus_current_startup / NPM1300_VBUS_CURRENT_DIVISOR);
    if (ret != 0) {
      PBL_LOG_ERR("VBUS startup current limit setup failed (%d)", ret);
      return ret;
    }
  }

  const struct prv_reg_val charge_setup[] = {
    {NPM1300_BCHGITERMSEL,
     (cfg->term_current_pct == 10U) ? NPM1300_BCHGITERMSEL_SEL10 : NPM1300_BCHGITERMSEL_SEL20},
    {NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL0},
    {NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL1},
    {NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL2},
    {NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL3},
    {NPM1300_BCHGDEBUG, NPM1300_BCHGDEBUG_DISABLEBATTERYDETECT},
    {NPM1300_BCHGVBATLOWCHARGE, 1},
  };

  ret = prv_write_seq(pmic, charge_setup, ARRAY_LENGTH(charge_setup));
  if (ret != 0) {
    PBL_LOG_ERR("Charger setup failed (%d)", ret);
    return ret;
  }

  ret = prv_clear_pending_interrupts();
  if (ret != 0) {
    return ret;
  }

  ret = pbl_device_init(&charger->irq.intc->dev);
  if (ret != 0) {
    PBL_LOG_ERR("Interrupt controller unavailable");
    return ret;
  }

  ret = pbl_intc_request(&charger->irq, prv_npm1300_interrupt_handler, nullptr);
  if (ret != 0) {
    PBL_LOG_ERR("Could not request the interrupt (%d)", ret);
    return ret;
  }

  ret = pbl_intc_enable(&charger->irq);
  if (ret != 0) {
    PBL_LOG_ERR("Could not enable the interrupt (%d)", ret);
    return ret;
  }

  return 0;
}

int pmic_power_off(void) {
  int ret;

  // Ship mode exits as soon as VBUS is present
  if (pmic_is_usb_connected()) {
    return -EBUSY;
  }

  ret = prv_write_register(NPM1300_TASKENTERSHIPMODE, 1);
  if (ret != 0) {
    PBL_LOG_ERR("Failed to enter ship mode (%d)", ret);
    return ret;
  }

  // tPWRDN is 100 ms
  pbl_busy_wait_us(100000);

  return -ETIMEDOUT;
}

uint16_t pmic_get_vsys(void) {
  if (prv_write_register(NPM1300_EVENTSADCCLR, NPM1300_EVENTSADC_VSYSRDY) != 0) {
    return 0;
  }
  if (prv_write_register(NPM1300_TASKVSYSMEASURE, 1) != 0) {
    return 0;
  }
  uint8_t reg = 0;
  uint32_t elapsed = 0;
  while ((reg & NPM1300_EVENTSADC_VSYSRDY) == 0) {
    if (elapsed >= ADC_POLL_TIMEOUT_MS) {
      return 0; // Timeout waiting for ADC
    }
    if (prv_read_register(NPM1300_EVENTSADCCLR, &reg) != 0) {
      return 0;
    }
    if ((reg & NPM1300_EVENTSADC_VSYSRDY) == 0) {
      pbl_sleep_ms(ADC_POLL_DELAY_MS);
      elapsed += ADC_POLL_DELAY_MS;
    }
  }

  uint8_t vsys_msb;
  uint8_t lsbs;
  if (prv_read_register(NPM1300_ADCVSYSRESULTMSB, &vsys_msb) != 0) {
    return 0;
  }
  if (prv_read_register(NPM1300_ADCGP0RESULTLSBS, &lsbs) != 0) {
    return 0;
  }
  uint16_t vsys_raw = prv_adc_raw(vsys_msb, lsbs, NPM1300_ADCGP0RESULTLSBS_VSYS_MASK);
  uint32_t vsys = vsys_raw * 6375 / 1023;

  return vsys;
}

int battery_get_millivolts(void) {
  if (prv_write_register(NPM1300_EVENTSADCCLR, NPM1300_EVENTSADC_VBATRDY) != 0) {
    return 0;
  }
  if (prv_write_register(NPM1300_TASKVBATMEASURE, 1) != 0) {
    return 0;
  }
  uint8_t reg = 0;
  uint32_t elapsed = 0;
  while ((reg & NPM1300_EVENTSADC_VBATRDY) == 0) {
    if (elapsed >= ADC_POLL_TIMEOUT_MS) {
      return 0; // Timeout waiting for ADC
    }
    if (prv_read_register(NPM1300_EVENTSADCCLR, &reg) != 0) {
      return 0;
    }
    if ((reg & NPM1300_EVENTSADC_VBATRDY) == 0) {
      pbl_sleep_ms(ADC_POLL_DELAY_MS);
      elapsed += ADC_POLL_DELAY_MS;
    }
  }

  uint8_t vbat_msb;
  uint8_t lsbs;
  if (prv_read_register(NPM1300_ADCVBATRESULTMSB, &vbat_msb) != 0) {
    return 0;
  }
  if (prv_read_register(NPM1300_ADCGP0RESULTLSBS, &lsbs) != 0) {
    return 0;
  }
  uint16_t vbat_raw = prv_adc_raw(vbat_msb, lsbs, NPM1300_ADCGP0RESULTLSBS_VBAT_MASK);
  uint32_t vbat = vbat_raw * 5000 / 1023;

  return vbat;
}

int battery_get_constants(BatteryConstants *constants) {
  uint8_t ibat_status;
  int ret;
  int32_t full_scale_ua;
  uint8_t msb;
  uint8_t lsb;
  uint16_t raw;
  uint8_t reg;

  // Obtain IBAT full scale
  ret = prv_read_register(NPM1300_ADCIBATMEASSTATUS, &ibat_status);
  if (ret != 0) {
    return ret;
  }

  if (PBL_FIELD_GET(NPM1300_ADCIBATMEASSTATUS_MODE_MASK, ibat_status) ==
      NPM1300_ADCIBATMEASSTATUS_MODE_CHRG) {
    full_scale_ua = ((int32_t)NPM1300_CHARGER->cfg->chg_current_ma * 1000 *
                     NPM1300_BCHARGER_ADC_CALC_CHARGE_MUL) /
                    NPM1300_BCHARGER_ADC_CALC_CHARGE_DIV;
  } else {
    full_scale_ua = ((int32_t)NPM1300_CHARGER->state->dischg_limit_ma * 1000 *
                     NPM1300_BCHARGER_ADC_CALC_DISCHARGE_MUL) /
                    NPM1300_BCHARGER_ADC_CALC_DISCHARGE_DIV;
  }

  // Clear the ADC ready events for VBAT, IBAT, and NTC
  ret = prv_write_register(NPM1300_EVENTSADCCLR, NPM1300_EVENTSADC_VBATRDY |
                                                     NPM1300_EVENTSADC_IBATRDY |
                                                     NPM1300_EVENTSADC_NTCRDY);
  if (ret != 0) {
    return ret;
  }

  // Trigger VBAT+IBAT measurement (IBATMEASENABLE is enabled)
  ret = prv_write_register(NPM1300_TASKVBATMEASURE, 1);
  if (ret != 0) {
    return ret;
  }

  // Trigger NTC measurement
  ret = prv_write_register(NPM1300_TASKNTCMEASURE, 1);
  if (ret != 0) {
    return ret;
  }

  // Process the VBAT measurement
  reg = 0U;
  uint32_t elapsed = 0;
  while ((reg & NPM1300_EVENTSADC_VBATRDY) == 0U) {
    if (elapsed >= ADC_POLL_TIMEOUT_MS) {
      return -ETIMEDOUT;
    }
    ret = prv_read_register(NPM1300_EVENTSADCCLR, &reg);
    if (ret != 0) {
      return ret;
    }
    if ((reg & NPM1300_EVENTSADC_VBATRDY) == 0U) {
      pbl_sleep_ms(ADC_POLL_DELAY_MS);
      elapsed += ADC_POLL_DELAY_MS;
    }
  }

  ret = prv_read_register(NPM1300_ADCVBATRESULTMSB, &msb);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read_register(NPM1300_ADCGP0RESULTLSBS, &lsb);
  if (ret != 0) {
    return ret;
  }

  raw = prv_adc_raw(msb, lsb, NPM1300_ADCGP0RESULTLSBS_VBAT_MASK);

  constants->v_mv = (int32_t)(raw * NPM1300_ADC_VFS_VBAT_MV) / NPM1300_BCHARGER_ADC_BITS_RESOLUTION;

  // Process the IBAT measurement
  elapsed = 0;
  while ((reg & NPM1300_EVENTSADC_IBATRDY) == 0U) {
    if (elapsed >= ADC_POLL_TIMEOUT_MS) {
      return -ETIMEDOUT;
    }
    ret = prv_read_register(NPM1300_EVENTSADCCLR, &reg);
    if (ret != 0) {
      return ret;
    }
    if ((reg & NPM1300_EVENTSADC_IBATRDY) == 0U) {
      pbl_sleep_ms(ADC_POLL_DELAY_MS);
      elapsed += ADC_POLL_DELAY_MS;
    }
  }

  ret = prv_read_register(NPM1300_ADCVBAT2RESULTMSB, &msb);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read_register(NPM1300_ADCGP1RESULTLSBS, &lsb);
  if (ret != 0) {
    return ret;
  }

  raw = prv_adc_raw(msb, lsb, NPM1300_ADCGP1RESULTLSBS_VBAT2_MASK);

  constants->i_ua = ((int32_t)raw * full_scale_ua) / NPM1300_BCHARGER_ADC_BITS_RESOLUTION;

  // Process the NTC measurement
  elapsed = 0;
  while ((reg & NPM1300_EVENTSADC_NTCRDY) == 0U) {
    if (elapsed >= ADC_POLL_TIMEOUT_MS) {
      return -ETIMEDOUT;
    }
    ret = prv_read_register(NPM1300_EVENTSADCCLR, &reg);
    if (ret != 0) {
      return ret;
    }
    if ((reg & NPM1300_EVENTSADC_NTCRDY) == 0U) {
      pbl_sleep_ms(ADC_POLL_DELAY_MS);
      elapsed += ADC_POLL_DELAY_MS;
    }
  }

  ret = prv_read_register(NPM1300_ADCNTCRESULTMSB, &msb);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read_register(NPM1300_ADCGP0RESULTLSBS, &lsb);
  if (ret != 0) {
    return ret;
  }

  raw = prv_adc_raw(msb, lsb, NPM1300_ADCGP0RESULTLSBS_NTC_MASK);

  // Ref: PS v1.2 Section 7.1.4: Battery temperature (Kelvin)
  float log_result = logf((1024.f / (float)raw) - 1.0f);
  float inv_temp_k = (1.f / 298.15f) - (log_result / (float)NPM1300_CHARGER->cfg->thermistor_beta);

  constants->t_mc = (int32_t)(1000.0f * ((1.f / inv_temp_k) - 273.15f));

  return 0;
}

bool pmic_set_charger_state(bool enable) {
  return prv_write_register(enable ? NPM1300_BCHGENABLESET : NPM1300_BCHGENABLECLR, 1) == 0;
}

void battery_set_charge_enable(bool charging_enabled) {
  pmic_set_charger_state(charging_enabled);
}

void battery_set_fast_charge(bool fast_charge_enabled) {
  /* the PMIC handles this for us */
}

bool pmic_is_charging(void) {
  uint8_t status;
  if (prv_read_register(NPM1300_BCHGCHARGESTATUS, &status) != 0) {
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
  if (prv_read_register(NPM1300_VBUSINSTATUS, &status) != 0) {
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
  int ret;

  ret = prv_read_register(NPM1300_BCHGCHARGESTATUS, &chg_status);
  if (ret != 0) {
    return ret;
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

static int prv_set_dischg_limit_locked(const struct pbl_npm1300_charger *charger, uint32_t ma,
                                       uint8_t msb, uint8_t lsb) {
  const struct pbl_npm1300 *pmic = pbl_npm1300_from_cell(&charger->dev);
  int ret;

  if (charger->state->dischg_limit_ma == ma) {
    return 0;
  }

  ret = pbl_npm1300_write(pmic, NPM1300_BCHGISETDISCHARGEMSB, msb);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_npm1300_write(pmic, NPM1300_BCHGISETDISCHARGELSB, lsb);
  if (ret != 0) {
    return ret;
  }

  charger->state->dischg_limit_ma = ma;

  return 0;
}

static int prv_dischg_limit_regs(uint32_t ma, uint8_t *msb, uint8_t *lsb) {
  if (ma == 200U) {
    *msb = NPM1300_BCHGISETDISCHARGEMSB_200MA;
    *lsb = NPM1300_BCHGISETDISCHARGELSB_200MA;
  } else if (ma == NPM1300_DISCHG_LIMIT_MA_MAX) {
    *msb = NPM1300_BCHGISETDISCHARGEMSB_1000MA;
    *lsb = NPM1300_BCHGISETDISCHARGELSB_1000MA;
  } else {
    PBL_LOG_ERR("Invalid discharge limit: %" PRIu32 " mA", ma);
    return -EINVAL;
  }

  return 0;
}

static int prv_apply_dischg_limit_locked(const struct pbl_npm1300_charger *charger) {
  uint32_t ma = charger->state->high_load_count > 0U ? NPM1300_DISCHG_LIMIT_MA_MAX
                                                     : charger->cfg->dischg_limit_ma;
  uint8_t msb;
  uint8_t lsb;
  int ret;

  ret = prv_dischg_limit_regs(ma, &msb, &lsb);
  if (ret != 0) {
    return ret;
  }

  return prv_set_dischg_limit_locked(charger, ma, msb, lsb);
}

int pbl_npm1300_charger_set_dischg_limit_ma(const struct pbl_npm1300_charger *charger,
                                            uint32_t ma) {
  const struct pbl_npm1300 *pmic = pbl_npm1300_from_cell(&charger->dev);
  uint8_t msb;
  uint8_t lsb;
  int ret;

  ret = prv_dischg_limit_regs(ma, &msb, &lsb);
  if (ret != 0) {
    return ret;
  }

  pbl_npm1300_lock(pmic);
  ret = prv_set_dischg_limit_locked(charger, ma, msb, lsb);
  pbl_npm1300_unlock(pmic);

  return ret;
}

static int prv_high_load_update(int delta) {
  const struct pbl_npm1300_charger *charger = NPM1300_CHARGER;
  const struct pbl_npm1300 *pmic = pbl_npm1300_from_cell(&charger->dev);
  struct pbl_npm1300_charger_state *state = charger->state;
  int ret;

  if (!pbl_device_is_ready(&charger->dev)) {
    return -ENODEV;
  }

  pbl_npm1300_lock(pmic);
  if (delta < 0 && state->high_load_count == 0U) {
    pbl_npm1300_unlock(pmic);
    return -EALREADY;
  }

  state->high_load_count += delta;
  ret = prv_apply_dischg_limit_locked(charger);
  if (ret != 0) {
    state->high_load_count -= delta;
  }
  pbl_npm1300_unlock(pmic);

  return ret;
}

int battery_high_load_request(void) {
  return prv_high_load_update(1);
}

int battery_high_load_release(void) {
  return prv_high_load_update(-1);
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
  PBL_SHELL_CMD(regs, nullptr, "Dump the main registers", prv_cmd_pmic_regs),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(pmic, sub_pmic, "PMIC", nullptr);
#endif
