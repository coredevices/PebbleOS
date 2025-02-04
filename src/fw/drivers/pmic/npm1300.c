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

typedef enum {
  PmicRegisters_MAIN_EVENTSADCCLR = 0x0003,
  PmicRegisters_ADC_TASKVBATMEASURE  = 0x0500,
  PmicRegisters_ADC_TASKNTCMEASURE   = 0x0501,
  PmicRegisters_ADC_TASKVSYSMEASURE  = 0x0503,
  PmicRegisters_ADC_TASKIBATMEASURE  = 0x0506,
  PmicRegisters_ADC_TASKVBUS7MEASURE = 0x0507,
  PmicRegisters_ADC_ADCVSYSRESULTMSB = 0x0514,
  PmicRegisters_ADC_ADCGP0RESULTLSBS = 0x0515,
  PmicRegisters_ERRLOG_SCRATCH0 = 0x0E01,
  PmicRegisters_ERRLOG_SCRATCH1 = 0x0E02,
  PmicRegisters_BUCK_BUCK1NORMVOUT = 0x0408,
  PmicRegisters_BUCK_BUCK2NORMVOUT = 0x040A,
  PmicRegisters_BUCK_BUCKSTATUS = 0x0434,
  PmicRegisters_LDSW_TASKLDSW1SET = 0x0800,
  PmicRegisters_LDSW_TASKLDSW1CLR = 0x0801,
  PmicRegisters_LDSW_TASKLDSW2SET = 0x0802,
  PmicRegisters_LDSW_TASKLDSW2CLR = 0x0803,
  PmicRegisters_LDSW_LDSWSTATUS = 0x0804,
  PmicRegisters_LDSW_LDSWCONFIG = 0x0807,
  PmicRegisters_LDSW_LDSW1LDOSEL = 0x0808,
  PmicRegisters_LDSW_LDSW2LDOSEL = 0x0809,
  PmicRegisters_LDSW_LDSW1VOUTSEL = 0x080C,
  PmicRegisters_LDSW_LDSW2VOUTSEL = 0x080D,
} PmicRegisters;

uint32_t pmic_get_last_reset_reason(void) {
  return 0;
}

static bool prv_read_register(uint16_t register_address, uint8_t *result) {
  i2c_use(I2C_NPM1300);
  uint8_t regad[2] = { register_address >> 8, register_address & 0xFF };
  bool rv = i2c_write_block(I2C_NPM1300, 2, regad);
  if (rv)
    rv = i2c_read_block(I2C_NPM1300, 1, result);
  i2c_release(I2C_NPM1300);
  return rv;
}

static bool prv_write_register(uint16_t register_address, uint8_t datum) {
  i2c_use(I2C_NPM1300);
  uint8_t d[3] = { register_address >> 8, register_address & 0xFF, datum };
  bool rv = i2c_write_block(I2C_NPM1300, 3, d);
  i2c_release(I2C_NPM1300);
  return rv;
}

bool pmic_init(void) {
  /* consider configuring pmic_int pin */
  uint8_t buck_out;
  prv_write_register(PmicRegisters_ERRLOG_SCRATCH0, 0x55);
  prv_write_register(PmicRegisters_ERRLOG_SCRATCH1, 0xAA);
  if (!prv_read_register(PmicRegisters_BUCK_BUCK1NORMVOUT, &buck_out)) {
    PBL_LOG(LOG_LEVEL_ERROR, "failed to read BUCK1NORMVOUT");
    return false;
  }
  PBL_LOG(LOG_LEVEL_DEBUG, "found the nPM1300, BUCK1NORMVOUT = 0x%x", buck_out);
  
  prv_read_register(PmicRegisters_LDSW_LDSWSTATUS, &buck_out);
  PBL_LOG(LOG_LEVEL_DEBUG, "nPM1300 LDSW status before enabling LDSW2 0x%x", buck_out);
  
  prv_write_register(PmicRegisters_LDSW_TASKLDSW2CLR, 0x01);
  prv_write_register(PmicRegisters_LDSW_LDSW2VOUTSEL, 8 /* 1.8V */);
  prv_write_register(PmicRegisters_LDSW_LDSW2LDOSEL, 1 /* LDO */);
  prv_write_register(PmicRegisters_LDSW_TASKLDSW2SET, 0x01);

  prv_read_register(PmicRegisters_LDSW_LDSWSTATUS, &buck_out);
  PBL_LOG(LOG_LEVEL_DEBUG, "nPM1300 LDSW status after enabling LDSW2 0x%x", buck_out);

  return true;
}

bool pmic_power_off(void) {
  return false;
}

// This is a hard power off, resulting in all rails being disabled.
// Generally, this is not desirable since we'll lose the backup domain.
// You're *probably* looking for pmic_power_off.
bool pmic_full_power_off(void) {
  return false;
}

// We have no way of directly reading Vsup with as3701b on Silk. Just assume
// that we are getting what we've configured as regulated Vsup.
uint16_t pmic_get_vsys(void) {
  if (!prv_write_register(PmicRegisters_MAIN_EVENTSADCCLR, 0x08 /* EVENTADCVSYSRDY */)) {
    return 0;
  }
  if (!prv_write_register(PmicRegisters_ADC_TASKVSYSMEASURE, 1)) {
    return 0;
  }
  uint8_t reg = 0;
  while ((reg & 0x08) == 0) {
    if (!prv_read_register(PmicRegisters_MAIN_EVENTSADCCLR, &reg)) {
      return 0;
    }
  }
  
  uint8_t vsys_msb;
  uint8_t lsbs;
  if (!prv_read_register(PmicRegisters_ADC_ADCVSYSRESULTMSB, &vsys_msb)) {
    return 0;
  }
  if (!prv_read_register(PmicRegisters_ADC_ADCGP0RESULTLSBS, &lsbs)) {
    return 0;
  }
  uint16_t vsys_raw = (vsys_msb << 2) | (lsbs >> 6);
  uint32_t vsys = vsys_raw * 6375 / 1023;
  
  return vsys;
}

bool pmic_set_charger_state(bool enable) {
  return false;
}

bool pmic_is_charging(void) {
  return false;
}

bool pmic_is_usb_connected(void) {
  return true;
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


void command_pmic_read_registers(void) {
#define SAY(x) do { uint8_t reg; int rv = prv_read_register(PmicRegisters_##x, &reg); PBL_LOG(LOG_LEVEL_DEBUG, "PMIC: " #x " = %02x (rv %d)", reg, rv); } while(0)
  SAY(ERRLOG_SCRATCH0);
  SAY(ERRLOG_SCRATCH1);
  SAY(BUCK_BUCK1NORMVOUT);
  SAY(BUCK_BUCK2NORMVOUT);
  SAY(BUCK_BUCKSTATUS);
  PBL_LOG(LOG_LEVEL_DEBUG, "PMIC: Vsys = %d mV", pmic_get_vsys());
}

void command_pmic_status(void) {
}

void command_pmic_rails(void) {
  // TODO: Implement.
}
