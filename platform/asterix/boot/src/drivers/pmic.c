#include <board.h>
#include <drivers/dbgserial.h>
#include <nrfx_twi.h>

#define CHARGER_BASE 0x03U
#define BUCK_BASE 0x04U
#define ADC_BASE 0x05U
#define TIMER_BASE 0x07U
#define LDSW_BASE 0x08U
#define ERRLOG_BASE 0x0EU

// CHARGER
#define BCHGENABLESET 0x04U
#define ENABLECHARGING_ENABLECHG 0x01U

#define BCHGENABLECLR 0x05U
#define ENABLECHARGING_DISABLECHG 0x1U

#define BCHGISETMSB 0x08U

#define BCHGISETDISCHARGEMSB 0x0AU

#define BCHGVTERM 0x0CU
#define BCHGVTERMNORM_4V20 0x8U

#define BCHGVTERMR 0x0DU
#define BCHGVTERMREDUCED_4V00 0x4U

// BUCK
#define BUCK1PWMSET 0x4U
#define BUCK1PWMSET_SET 0x01U

#define BUCK1NORMVOUT 0x8U
#define BUCK1RETVOUT 0x8U
#define BUCKVOUT_1V8 0x8U

#define BUCKSWCTRLSET 0xFU
#define BUCKSWCTRLSET_BUCK1SWCTRLSET 0x01U

// ADC
#define ADCNTCRSEL 0x0AU
#define ADCNTCRSEL_10K 0x1U

// TIMER
#define TIMERCLR 0x01U
#define TIMERCLR_TASKTIMERDIS 0x01U

// LDO
#define TASKLDSW2SET 0x02U

#define LDSW2LDOSEL 0x09U
#define LDSW2LDOSEL_LDO 0x01U

#define LDSW2VOUTSEL 0x0DU
#define LDSW2VOUTSEL_1V8 0x08U

// ERRLOG
#define SCRATCH0 0x1U
#define SCRATCH0_BOOTTIMEREN 0x01U


static const nrfx_twi_t twi = NRFX_TWI_INSTANCE(BOARD_PMIC_I2C);
static const nrfx_twi_config_t config =
    NRFX_TWI_DEFAULT_CONFIG(BOARD_PMIC_I2C_SCL_PIN, BOARD_PMIC_I2C_SDA_PIN);

static int prv_pmic_write(uint8_t base, uint8_t reg, uint8_t val) {
  nrfx_err_t err;
  uint8_t data[3];
  nrfx_twi_xfer_desc_t xfer = NRFX_TWI_XFER_DESC_TX(0x6b, data, sizeof(data));

  data[0] = base;
  data[1] = reg;
  data[2] = val;

  err = nrfx_twi_xfer(&twi, &xfer, 0);
  if (err != NRFX_SUCCESS) {
    return -1;
  }

  return 0;
}

int pmic_init(void) {
  int ret;
  nrfx_err_t err;

  err = nrfx_twi_init(&twi, &config, NULL, NULL);
  if (err != NRFX_SUCCESS) {
    return -1;
  }

  nrfx_twi_enable(&twi);
  
  // Turn off any watchdog / boot timer right away.
  ret = prv_pmic_write(TIMER_BASE, TIMERCLR, TIMERCLR_TASKTIMERDIS);
  if (ret != 0) {
    return ret;
  }

  ret = prv_pmic_write(ERRLOG_BASE, SCRATCH0, 0x00);
  if (ret != 0) {
    return ret;
  }

  // Set up the BUCK1 regulator for maximal stability and manual control to
  // 1.8V -- system FW can reenable hysteretic mode later to save power.
  ret = prv_pmic_write(BUCK_BASE, BUCK1PWMSET, BUCK1PWMSET_SET);
  if (ret != 0) {
    return ret;
  }

  ret = prv_pmic_write(BUCK_BASE, BUCK1NORMVOUT, BUCKVOUT_1V8);
  if (ret != 0) {
    return ret;
  }

  ret = prv_pmic_write(BUCK_BASE, BUCK1RETVOUT, BUCKVOUT_1V8);
  if (ret != 0) {
    return ret;
  }

  ret = prv_pmic_write(BUCK_BASE, BUCKSWCTRLSET, BUCKSWCTRLSET_BUCK1SWCTRLSET);
  if (ret != 0) {
    return ret;
  }

  // Configure charger (TODO: values are board/battery dependent)
  // - Thermistor: 10K NTC
  // - Termination voltage: 4.2V
  // - Reduced termination voltage (for warm region): 4.00V
  // - 64mA charge/discharge current (standard charging)
  // - Enable charging
  ret = prv_pmic_write(CHARGER_BASE, BCHGENABLECLR, ENABLECHARGING_DISABLECHG);
  if (ret != 0) {
    return ret;
  }

  ret = prv_pmic_write(ADC_BASE, ADCNTCRSEL, ADCNTCRSEL_10K);
  if (ret != 0) {
    return ret;
  }

  ret = prv_pmic_write(CHARGER_BASE, BCHGVTERM, BCHGVTERMNORM_4V20);
  if (ret != 0) {
    return ret;
  }

  ret = prv_pmic_write(CHARGER_BASE, BCHGVTERMR, BCHGVTERMREDUCED_4V00);
  if (ret != 0) {
    return ret;
  }

  ret = prv_pmic_write(CHARGER_BASE, BCHGISETMSB, 16);
  if (ret != 0) {
    return ret;
  }

  ret = prv_pmic_write(CHARGER_BASE, BCHGISETDISCHARGEMSB, 16);
  if (ret != 0) {
    return ret;
  }

  ret = prv_pmic_write(CHARGER_BASE, BCHGENABLESET, ENABLECHARGING_ENABLECHG);
  if (ret != 0) {
    return ret;
  }

  // LDO2 as LDO @ 1.8V (powers the QSPI flash)
  ret = prv_pmic_write(LDSW_BASE, LDSW2LDOSEL, LDSW2LDOSEL_LDO);
  if (ret != 0) {
    return ret;
  }

  ret = prv_pmic_write(LDSW_BASE, LDSW2VOUTSEL, LDSW2VOUTSEL_1V8);
  if (ret != 0) {
    return ret;
  }

  ret = prv_pmic_write(LDSW_BASE, TASKLDSW2SET, 0x01U);
  if (ret != 0) {
    return ret;
  }

  nrfx_twi_disable(&twi);

  return 0;
}
