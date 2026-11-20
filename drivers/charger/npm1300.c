/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <math.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/battery.h>
#include <pbl/drivers/charger.h>
#include <pbl/drivers/intc.h>
#include <pbl/drivers/mfd/npm1300.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/sched.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

PBL_LOG_MODULE_DECLARE(driver_charger, CONFIG_DRIVER_CHARGER_LOG_LEVEL);

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

#define DISCHG_LIMIT_MA_MAX 1000U

struct pbl_charger_npm1300_config {
  // Discharge current limit in mA, 200 or 1000
  uint16_t dischg_limit_ma;
  // Charge termination voltage in the warm and cool regions in mV
  uint16_t vterm_reduced_mv;
  uint16_t thermistor_beta;
  // VBUS current limits in mA with USB connected and at startup, 0 for the default
  uint16_t vbus_current_lim0;
  uint16_t vbus_current_startup;
  // NTC thermistor nominal resistance in kOhm: 10, 47 or 100
  uint8_t ntc_kohm;
  // Charging stops above it
  uint8_t ntc_hot_celsius;
  // Ship mode cuts the power of the system
  bool system_power_controller;
};

struct pbl_charger_npm1300_data {
  const struct pbl_charger_npm1300 *chg;
  uint32_t dischg_limit_ma;
  struct pbl_delayed_work debounce_timer;
};

struct pbl_charger_npm1300 {
  struct pbl_charger charger;
  struct pbl_charger_npm1300_config cfg;
  // Charge current 32 to 800 mA in 2 mA steps, termination current 10 or 20% of it
  const struct pbl_battery *battery;
  // From the PMIC's GPIO1
  struct pbl_irq_spec irq;
  struct pbl_charger_npm1300_data *data;
};

static uint16_t prv_ntc_threshold_code(const struct pbl_charger_npm1300_config *cfg,
                                       uint8_t celsius) {
  // Ref: PS v1.1 Section 6.2.5: K_NTCTEMP = round(1024 * R_T / (R_T + R_B))
  float t_k = (float)celsius + 273.15f;
  float exponent = (float)cfg->thermistor_beta * ((1.f / 298.15f) - (1.f / t_k));
  return (uint16_t)((1024.0f / (1.0f + expf(exponent))) + 0.5f);
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

static const struct pbl_charger_npm1300 *prv_chg(const struct pbl_charger *charger) {
  return PBL_CONTAINER_OF(charger, const struct pbl_charger_npm1300, charger);
}

static int prv_read_register(const struct pbl_charger_npm1300 *chg, uint16_t reg, uint8_t *val) {
  return pbl_npm1300_read(pbl_npm1300_from_cell(&chg->charger.dev), reg, val);
}

static int prv_write_register(const struct pbl_charger_npm1300 *chg, uint16_t reg, uint8_t val) {
  return pbl_npm1300_write(pbl_npm1300_from_cell(&chg->charger.dev), reg, val);
}

static int prv_vbus_present(const struct pbl_charger_npm1300 *chg, bool *present) {
  uint8_t status;
  int ret;

  ret = prv_read_register(chg, NPM1300_VBUSINSTATUS, &status);
  if (ret != 0) {
    return ret;
  }

  *present = (status & NPM1300_VBUSINSTATUS_PRESENT) != 0U;

  return 0;
}

static int prv_set_vbus_current_lim0(const struct pbl_charger_npm1300 *chg) {
  int ret;

  ret = prv_write_register(chg, NPM1300_VBUSINILIM0,
                           chg->cfg.vbus_current_lim0 / NPM1300_VBUS_CURRENT_DIVISOR);
  if (ret != 0) {
    return ret;
  }

  return prv_write_register(chg, NPM1300_TASKUPDATELIMSW, NPM1300_TASKUPDATELIMSW_EN);
}

static void prv_handle_charge_state_change(struct pbl_work *work) {
  struct pbl_charger_npm1300_data *data =
      PBL_CONTAINER_OF(work, struct pbl_charger_npm1300_data, debounce_timer.work);
  const struct pbl_charger_npm1300 *chg = data->chg;
  bool present = false;
  int ret;

  ret = prv_vbus_present(chg, &present);
  if (ret != 0) {
    PBL_LOG_ERR("VBUS status read failed (%d)", ret);
  }

  PBL_LOG_DBG("VBUS %s", present ? "present" : "absent");

  if (present && chg->cfg.vbus_current_lim0 != 0U) {
    ret = prv_set_vbus_current_lim0(chg);
    if (ret != 0) {
      PBL_LOG_ERR("VBUS current limit setup failed (%d)", ret);
    }
  }

  pbl_charger_report_online_changed(&chg->charger);
}

static int prv_clear_pending_interrupts(const struct pbl_charger_npm1300 *chg) {
  int ret;

  ret = prv_write_register(chg, NPM1300_EVENTSBCHARGER1CLR, NPM1300_EVENTSBCHARGER1_CHGCOMPLETED);
  if (ret != 0) {
    return ret;
  }

  return prv_write_register(chg, NPM1300_EVENTSVBUSIN0CLR,
                            NPM1300_EVENTSVBUSIN0_VBUSDETECTED | NPM1300_EVENTSVBUSIN0_VBUSREMOVED);
}

static void prv_pmic_state_change_cb(void *arg) {
  const struct pbl_charger_npm1300 *chg = arg;
  int ret;

  ret = prv_clear_pending_interrupts(chg);
  if (ret != 0) {
    PBL_LOG_ERR("Interrupt clear failed (%d)", ret);
  }

  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &chg->data->debounce_timer,
                              PBL_MSEC(CHARGER_DEBOUNCE_MS));
}

static void prv_npm1300_interrupt_handler(void *arg) {
  pbl_sys_work_call_from_isr(prv_pmic_state_change_cb, arg);
}

static int prv_set_dischg_limit_ma(const struct pbl_charger_npm1300 *chg, uint32_t ma);

static int prv_init(const struct pbl_charger *charger) {
  const struct pbl_charger_npm1300 *chg = prv_chg(charger);
  const struct pbl_npm1300 *pmic = pbl_npm1300_from_cell(&charger->dev);
  const struct pbl_charger_npm1300_config *cfg = &chg->cfg;
  const struct pbl_battery *battery = chg->battery;
  uint32_t chg_current_ma = battery->constant_charge_current_ua / 1000U;
  uint32_t vterm_mv = battery->constant_charge_voltage_uv / 1000U;
  uint32_t term_current_pct = 0U;
  uint16_t ntc_hot;
  uint8_t vterm;
  uint8_t vtermr;
  uint8_t ntcsel;
  int ret;

  chg->data->chg = chg;
  chg->data->dischg_limit_ma = 0U;
  pbl_delayed_work_init(&chg->data->debounce_timer, prv_handle_charge_state_change);

  if ((chg_current_ma < 32U) || (chg_current_ma > 800U) || (chg_current_ma % 2U != 0U)) {
    PBL_LOG_ERR("Invalid charge current: %d mA", (int)chg_current_ma);
    return -EINVAL;
  }

  term_current_pct = battery->charge_term_current_ua * 100U / battery->constant_charge_current_ua;
  if ((term_current_pct != 10U) && (term_current_pct != 20U)) {
    PBL_LOG_ERR("Invalid termination current: %d", (int)term_current_pct);
    return -EINVAL;
  }

  ret = prv_vterm_code(vterm_mv, &vterm);
  if (ret != 0) {
    PBL_LOG_ERR("Invalid termination voltage: %d mV", (int)vterm_mv);
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
    {NPM1300_BCHGISETMSB, PBL_FIELD_GET(NPM1300_BCHGISET_MSB_MASK, chg_current_ma / 2U)},
    {NPM1300_BCHGISETLSB, PBL_FIELD_GET(NPM1300_BCHGISET_LSB_MASK, chg_current_ma / 2U)},
  };

  ret = prv_write_seq(pmic, setup, PBL_ARRAY_SIZE(setup));
  if (ret != 0) {
    PBL_LOG_ERR("Setup failed (%d)", ret);
    return ret;
  }

  ret = prv_set_dischg_limit_ma(chg, cfg->dischg_limit_ma);
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
     (term_current_pct == 10U) ? NPM1300_BCHGITERMSEL_SEL10 : NPM1300_BCHGITERMSEL_SEL20},
    {NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL0},
    {NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL1},
    {NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL2},
    {NPM1300_TESTACCESS, NPM1300_TESTACCESS_VAL3},
    {NPM1300_BCHGDEBUG, NPM1300_BCHGDEBUG_DISABLEBATTERYDETECT},
    {NPM1300_BCHGVBATLOWCHARGE, 1},
  };

  ret = prv_write_seq(pmic, charge_setup, PBL_ARRAY_SIZE(charge_setup));
  if (ret != 0) {
    PBL_LOG_ERR("Charger setup failed (%d)", ret);
    return ret;
  }

  ret = prv_clear_pending_interrupts(chg);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_device_init(&chg->irq.intc->dev);
  if (ret != 0) {
    PBL_LOG_ERR("Interrupt controller unavailable");
    return ret;
  }

  ret = pbl_intc_request(&chg->irq, prv_npm1300_interrupt_handler, (void *)chg);
  if (ret != 0) {
    PBL_LOG_ERR("Could not request the interrupt (%d)", ret);
    return ret;
  }

  ret = pbl_intc_enable(&chg->irq);
  if (ret != 0) {
    PBL_LOG_ERR("Could not enable the interrupt (%d)", ret);
    return ret;
  }

  return 0;
}

static int prv_power_cut_check(void *user_data) {
  const struct pbl_charger_npm1300 *chg = user_data;
  bool present;
  int ret;

  if (!chg->cfg.system_power_controller) {
    return -ENOTSUP;
  }

  // Ship mode exits as soon as VBUS is present
  ret = prv_vbus_present(chg, &present);
  if (ret != 0) {
    return ret;
  }

  return present ? -EBUSY : 0;
}

// Ship mode: everything but the RTC loses power until a button press
static void prv_power_cut(enum pbl_sys_off_mode mode, void *user_data) {
  int ret;

  // I2C transfers need the scheduler
  if (pbl_in_isr() || pbl_irq_is_locked() || !pbl_kernel_is_running()) {
    return;
  }

  ret = prv_power_cut_check(user_data);
  if (ret != 0) {
    PBL_LOG_ERR("Cannot enter ship mode (%d)", ret);
    return;
  }

  ret = prv_write_register(user_data, NPM1300_TASKENTERSHIPMODE, 1);
  if (ret != 0) {
    PBL_LOG_ERR("Failed to enter ship mode (%d)", ret);
    return;
  }

  // tPWRDN is 100 ms
  pbl_busy_wait_us(100000);

  PBL_LOG_ERR("Still powered after entering ship mode");
}

static int prv_get_measurements(const struct pbl_charger *charger,
                                struct pbl_charger_measurements *meas) {
  const struct pbl_charger_npm1300 *chg = prv_chg(charger);
  uint8_t ibat_status;
  int ret;
  int32_t full_scale_ua;
  uint8_t msb;
  uint8_t lsb;
  uint16_t raw;
  uint8_t reg;

  // Clear the ADC ready events for VBAT, IBAT, and NTC
  ret = prv_write_register(
      chg, NPM1300_EVENTSADCCLR,
      NPM1300_EVENTSADC_VBATRDY | NPM1300_EVENTSADC_IBATRDY | NPM1300_EVENTSADC_NTCRDY);
  if (ret != 0) {
    return ret;
  }

  // Trigger VBAT+IBAT measurement (IBATMEASENABLE is enabled)
  ret = prv_write_register(chg, NPM1300_TASKVBATMEASURE, 1);
  if (ret != 0) {
    return ret;
  }

  // Trigger NTC measurement
  ret = prv_write_register(chg, NPM1300_TASKNTCMEASURE, 1);
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
    ret = prv_read_register(chg, NPM1300_EVENTSADCCLR, &reg);
    if (ret != 0) {
      return ret;
    }
    if ((reg & NPM1300_EVENTSADC_VBATRDY) == 0U) {
      pbl_sleep_ms(ADC_POLL_DELAY_MS);
      elapsed += ADC_POLL_DELAY_MS;
    }
  }

  ret = prv_read_register(chg, NPM1300_ADCVBATRESULTMSB, &msb);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read_register(chg, NPM1300_ADCGP0RESULTLSBS, &lsb);
  if (ret != 0) {
    return ret;
  }

  raw = prv_adc_raw(msb, lsb, NPM1300_ADCGP0RESULTLSBS_VBAT_MASK);

  meas->voltage_mv =
      (int32_t)(raw * NPM1300_ADC_VFS_VBAT_MV) / NPM1300_BCHARGER_ADC_BITS_RESOLUTION;

  // Process the IBAT measurement
  elapsed = 0;
  while ((reg & NPM1300_EVENTSADC_IBATRDY) == 0U) {
    if (elapsed >= ADC_POLL_TIMEOUT_MS) {
      return -ETIMEDOUT;
    }
    ret = prv_read_register(chg, NPM1300_EVENTSADCCLR, &reg);
    if (ret != 0) {
      return ret;
    }
    if ((reg & NPM1300_EVENTSADC_IBATRDY) == 0U) {
      pbl_sleep_ms(ADC_POLL_DELAY_MS);
      elapsed += ADC_POLL_DELAY_MS;
    }
  }

  // The status holds the mode of the last measurement, so read it only once
  // this one is done.
  ret = prv_read_register(chg, NPM1300_ADCIBATMEASSTATUS, &ibat_status);
  if (ret != 0) {
    return ret;
  }

  if (PBL_FIELD_GET(NPM1300_ADCIBATMEASSTATUS_MODE_MASK, ibat_status) ==
      NPM1300_ADCIBATMEASSTATUS_MODE_CHRG) {
    full_scale_ua =
        ((int32_t)chg->battery->constant_charge_current_ua * NPM1300_BCHARGER_ADC_CALC_CHARGE_MUL) /
        NPM1300_BCHARGER_ADC_CALC_CHARGE_DIV;
  } else {
    full_scale_ua =
        ((int32_t)chg->data->dischg_limit_ma * 1000 * NPM1300_BCHARGER_ADC_CALC_DISCHARGE_MUL) /
        NPM1300_BCHARGER_ADC_CALC_DISCHARGE_DIV;
  }

  ret = prv_read_register(chg, NPM1300_ADCVBAT2RESULTMSB, &msb);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read_register(chg, NPM1300_ADCGP1RESULTLSBS, &lsb);
  if (ret != 0) {
    return ret;
  }

  raw = prv_adc_raw(msb, lsb, NPM1300_ADCGP1RESULTLSBS_VBAT2_MASK);

  meas->current_ua = ((int32_t)raw * full_scale_ua) / NPM1300_BCHARGER_ADC_BITS_RESOLUTION;

  // Process the NTC measurement
  elapsed = 0;
  while ((reg & NPM1300_EVENTSADC_NTCRDY) == 0U) {
    if (elapsed >= ADC_POLL_TIMEOUT_MS) {
      return -ETIMEDOUT;
    }
    ret = prv_read_register(chg, NPM1300_EVENTSADCCLR, &reg);
    if (ret != 0) {
      return ret;
    }
    if ((reg & NPM1300_EVENTSADC_NTCRDY) == 0U) {
      pbl_sleep_ms(ADC_POLL_DELAY_MS);
      elapsed += ADC_POLL_DELAY_MS;
    }
  }

  ret = prv_read_register(chg, NPM1300_ADCNTCRESULTMSB, &msb);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read_register(chg, NPM1300_ADCGP0RESULTLSBS, &lsb);
  if (ret != 0) {
    return ret;
  }

  raw = prv_adc_raw(msb, lsb, NPM1300_ADCGP0RESULTLSBS_NTC_MASK);

  // Ref: PS v1.2 Section 7.1.4: Battery temperature (Kelvin)
  float log_result = logf((1024.f / (float)raw) - 1.0f);
  float inv_temp_k = (1.f / 298.15f) - (log_result / (float)chg->cfg.thermistor_beta);

  meas->temp_mc = (int32_t)(1000.0f * ((1.f / inv_temp_k) - 273.15f));

  return 0;
}

static int prv_is_online(const struct pbl_charger *charger, bool *online) {
  return prv_vbus_present(prv_chg(charger), online);
}

static int prv_get_status(const struct pbl_charger *charger, enum pbl_charger_status *status) {
  uint8_t chg_status;
  int ret;

  ret = prv_read_register(prv_chg(charger), NPM1300_BCHGCHARGESTATUS, &chg_status);
  if (ret != 0) {
    return ret;
  }

  switch (chg_status &
          (NPM1300_BCHGCHARGESTATUS_COMPLETED | NPM1300_BCHGCHARGESTATUS_TRICKLECHARGE |
           NPM1300_BCHGCHARGESTATUS_CONSTANTCURRENT | NPM1300_BCHGCHARGESTATUS_CONSTANTVOLTAGE)) {
    case NPM1300_BCHGCHARGESTATUS_COMPLETED:
      *status = PBL_CHARGER_STATUS_FULL;
      break;
    case NPM1300_BCHGCHARGESTATUS_TRICKLECHARGE:
      *status = PBL_CHARGER_STATUS_TRICKLE;
      break;
    case NPM1300_BCHGCHARGESTATUS_CONSTANTCURRENT:
      *status = PBL_CHARGER_STATUS_CONSTANT_CURRENT;
      break;
    case NPM1300_BCHGCHARGESTATUS_CONSTANTVOLTAGE:
      *status = PBL_CHARGER_STATUS_CONSTANT_VOLTAGE;
      break;
    default:
      *status = PBL_CHARGER_STATUS_UNKNOWN;
      break;
  }

  return 0;
}

static int prv_enable(const struct pbl_charger *charger, bool enable) {
  return prv_write_register(prv_chg(charger),
                            enable ? NPM1300_BCHGENABLESET : NPM1300_BCHGENABLECLR, 1);
}

static int prv_set_dischg_limit_ma(const struct pbl_charger_npm1300 *chg, uint32_t ma) {
  const struct pbl_npm1300 *pmic = pbl_npm1300_from_cell(&chg->charger.dev);
  uint8_t msb;
  uint8_t lsb;
  int ret;

  if (chg->data->dischg_limit_ma == ma) {
    return 0;
  }

  if (ma == 200U) {
    msb = NPM1300_BCHGISETDISCHARGEMSB_200MA;
    lsb = NPM1300_BCHGISETDISCHARGELSB_200MA;
  } else if (ma == DISCHG_LIMIT_MA_MAX) {
    msb = NPM1300_BCHGISETDISCHARGEMSB_1000MA;
    lsb = NPM1300_BCHGISETDISCHARGELSB_1000MA;
  } else {
    PBL_LOG_ERR("Invalid discharge limit: %" PRIu32 " mA", ma);
    return -EINVAL;
  }

  pbl_npm1300_lock(pmic);
  ret = pbl_npm1300_write(pmic, NPM1300_BCHGISETDISCHARGEMSB, msb);
  if (ret == 0) {
    ret = pbl_npm1300_write(pmic, NPM1300_BCHGISETDISCHARGELSB, lsb);
  }
  pbl_npm1300_unlock(pmic);
  if (ret != 0) {
    return ret;
  }

  chg->data->dischg_limit_ma = ma;

  return 0;
}

static int prv_set_high_load(const struct pbl_charger *charger, bool high_load) {
  const struct pbl_charger_npm1300 *chg = prv_chg(charger);

  return prv_set_dischg_limit_ma(chg, high_load ? DISCHG_LIMIT_MA_MAX : chg->cfg.dischg_limit_ma);
}

static const struct pbl_charger_ops s_ops = {
  .init = prv_init,
  .is_online = prv_is_online,
  .get_status = prv_get_status,
  .get_measurements = prv_get_measurements,
  .enable = prv_enable,
  .set_high_load = prv_set_high_load,
};

#define PBL_CHARGER_NPM1300_DEFINE(sym, _pmic, _irq, _battery, _vterm_reduced_uv, _ntc_ohms,    \
                                   _beta, _hot_celsius, _dischg_ua, _vbus_ua, _vbus_startup_ua, \
                                   _system_power_controller)                                    \
  PBL_CHARGER_STATE_DEFINE(sym);                                                                \
  static struct pbl_charger_npm1300_data sym##_data;                                            \
  PBL_DEVICE_INSTANCE(sym, struct pbl_charger_npm1300, struct pbl_charger, charger){            \
    .charger = PBL_CHARGER_INIT(sym, "npm1300_charger", &(_pmic)->dev, &s_ops),                 \
    .cfg =                                                                                      \
        {                                                                                       \
          .dischg_limit_ma = (_dischg_ua) / 1000,                                               \
          .vterm_reduced_mv = (_vterm_reduced_uv) / 1000,                                       \
          .thermistor_beta = (_beta),                                                           \
          .vbus_current_lim0 = (_vbus_ua) / 1000,                                               \
          .vbus_current_startup = (_vbus_startup_ua) / 1000,                                    \
          .ntc_kohm = (_ntc_ohms) / 1000,                                                       \
          .ntc_hot_celsius = (_hot_celsius),                                                    \
          .system_power_controller = (_system_power_controller),                                \
        },                                                                                      \
    .battery = (_battery),                                                                      \
    .irq = _irq,                                                                                \
    .data = &sym##_data,                                                                        \
  };                                                                                            \
  PBL_SYS_OFF_POWER_CUT_HANDLER_DEFINE(sym##_power_cut, PBL_SYS_OFF_PRIO_DEFAULT,               \
                                       prv_power_cut_check, prv_power_cut, (void *)&sym##_inst)

#include PBL_DT_INSTANCES(nordic_npm1300_charger)
