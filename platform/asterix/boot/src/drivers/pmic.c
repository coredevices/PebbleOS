#include <board.h>
#include <drivers/dbgserial.h>
#include <nrfx_twi.h>
#include <system/passert.h>

#define CHARGER_BASE 0x03U
#define ADC_BASE 0x05U
#define LDSW_BASE 0x08U

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

// ADC
#define ADCNTCRSEL 0x0AU
#define ADCNTCRSEL_10K 0x1U

// LDO
#define TASKLDSW2SET 0x02U

#define LDSW2LDOSEL 0x09U
#define LDSW2LDOSEL_LDO 0x01U

#define LDSW2VOUTSEL 0x0DU
#define LDSW2VOUTSEL_1V8 0x08U

static const nrfx_twi_t twi = NRFX_TWI_INSTANCE(BOARD_PMIC_I2C);
static const nrfx_twi_config_t config =
    NRFX_TWI_DEFAULT_CONFIG(BOARD_PMIC_I2C_SCL_PIN, BOARD_PMIC_I2C_SDA_PIN);

static void prv_pmic_write(uint8_t base, uint8_t reg, uint8_t val) {
  nrfx_err_t err;
  uint8_t data[3];
  nrfx_twi_xfer_desc_t xfer = NRFX_TWI_XFER_DESC_TX(0x6b, data, sizeof(data));

  data[0] = base;
  data[1] = reg;
  data[2] = val;

  err = nrfx_twi_xfer(&twi, &xfer, 0);
  PBL_ASSERT(err == NRFX_SUCCESS, "TWI transfer failed: %d", err);
}

void pmic_init(void) {
  nrfx_err_t err;

  err = nrfx_twi_init(&twi, &config, NULL, NULL);
  PBL_ASSERT(err == NRFX_SUCCESS, "TWI init failed: %d", err);

  nrfx_twi_enable(&twi);

  // Configure charger (TODO: values are board/battery dependent)
  // - Thermistor: 10K NTC
  // - Termination voltage: 4.2V
  // - Reduced termination voltage (for warm region): 4.00V
  // - 64mA charge/discharge current (standard charging)
  // - Enable charging
  prv_pmic_write(CHARGER_BASE, BCHGENABLECLR, ENABLECHARGING_DISABLECHG);

  prv_pmic_write(ADC_BASE, ADCNTCRSEL, ADCNTCRSEL_10K);

  prv_pmic_write(CHARGER_BASE, BCHGVTERM, BCHGVTERMNORM_4V20);
  prv_pmic_write(CHARGER_BASE, BCHGVTERMR, BCHGVTERMREDUCED_4V00);

  prv_pmic_write(CHARGER_BASE, BCHGISETMSB, 16);
  prv_pmic_write(CHARGER_BASE, BCHGISETDISCHARGEMSB, 16);

  prv_pmic_write(CHARGER_BASE, BCHGENABLESET, ENABLECHARGING_ENABLECHG);

  // LDO2 as LDO @ 1.8V (powers the QSPI flash)
  prv_pmic_write(LDSW_BASE, LDSW2LDOSEL, LDSW2LDOSEL_LDO);
  prv_pmic_write(LDSW_BASE, LDSW2VOUTSEL, LDSW2VOUTSEL_1V8);
  prv_pmic_write(LDSW_BASE, TASKLDSW2SET, 0x01U);

  nrfx_twi_disable(&twi);
}
