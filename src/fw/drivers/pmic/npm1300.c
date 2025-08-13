/* Because nPM1300 also has the battery monitor, we implement both the
 * pmic_* and the battery_* API here.  */

#include <math.h>

#include "drivers/pmic.h"
#include "drivers/battery.h"

#include "board/board.h"
#include "console/prompt.h"
#include "drivers/battery.h"
#include "drivers/exti.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "drivers/periph_config.h"
#include "kernel/events.h"
#include "kernel/util/delay.h"
#include "os/mutex.h"
#include "services/common/system_task.h"
#include "system/logging.h"
#include "system/passert.h"

#define CHARGER_DEBOUNCE_MS 400
static TimerID s_debounce_charger_timer = TIMER_INVALID_ID;
static PebbleMutex *s_i2c_lock;

typedef enum {
  PmicRegisters_MAIN_EVENTSADCCLR = 0x0003,
  PmicRegisters_MAIN_EVENTSADCCLR__EVENTADCVBATRDY = 0x01,
  PmicRegisters_MAIN_EVENTSADCCLR__EVENTADCNTCRDY = 0x02,
  PmicRegisters_MAIN_EVENTSADCCLR__EVENTADCIBATRDY = 0x40,
  PmicRegisters_MAIN_EVENTSBCHARGER1CLR = 0x000B,
  PmicRegisters_MAIN_INTENEVENTSBCHARGER1SET = 0x000C,
  PmicRegisters_MAIN_EVENTSBCHARGER1__EVENTCHGCOMPLETED = 16,
  PmicRegisters_MAIN_EVENTSVBUSIN0CLR = 0x0017,
  PmicRegisters_MAIN_INTENEVENTSVBUSIN0SET = 0x0018,
  PmicRegisters_MAIN_EVENTSVBUSIN0__EVENTVBUSDETECTED = 1,
  PmicRegisters_MAIN_EVENTSVBUSIN0__EVENTVBUSREMOVED = 2,
  PmicRegisters_SYSTEM_TESTACCESS = 0x0123,
  PmicRegisters_SYSTEM_TESTACCESS__VAL0 = 0x44,
  PmicRegisters_SYSTEM_TESTACCESS__VAL1 = 0x90,
  PmicRegisters_SYSTEM_TESTACCESS__VAL2 = 0xFA,
  PmicRegisters_SYSTEM_TESTACCESS__VAL3 = 0xCE,
  PmicRegisters_VBUSIN_VBUSINSTATUS = 0x0207,
  PmicRegisters_VBUSIN_VBUSINSTATUS__VBUSINPRESENT = 1,
  PmicRegisters_BCHARGER_TASKRELEASEERROR = 0x0300U,
  PmicRegisters_BCHARGER_TASKCLEARCHGERR = 0x0301U,
  PmicRegisters_BCHARGER_BCHGENABLESET = 0x0304,
  PmicRegisters_BCHARGER_BCHGENABLECLR = 0x0305,
  PmicRegisters_BCHARGER_BCHGISETMSB = 0x0308,
  PmicRegisters_BCHARGER_BCHGISETLSB = 0x0309,
  PmicRegisters_BCHARGER_BCHGISETDISCHARGEMSB = 0x030A,
  PmicRegisters_BCHARGER_BCHGISETDISCHARGELSB = 0x30B,
  PmicRegisters_BCHARGER_BCHGVTERM = 0x030CU,
  PmicRegisters_BCHARGER_BCHGVTERM__BCHGVTERMNORM_4V20 = 0x8U,
  PmicRegisters_BCHARGER_BCHGVTERMR = 0x030DU,
  PmicRegisters_BCHARGER_BCHGVTERMR__BCHGVTERMREDUCED_4V00 = 0x4U,
  PmicRegisters_BCHARGER_BCHGITERMSEL = 0x030F,
  PmicRegisters_BCHARGER_BCHGITERMSEL__SEL10 = 0U,
  PmicRegisters_BCHARGER_BCHGITERMSEL__SEL20 = 1U,
  PmicRegisters_BCHARGER_BCHGCHARGESTATUS = 0x0334,
  PmicRegisters_BCHARGER_BCHGCHARGESTATUS__COMPLETED = 2,
  PmicRegisters_BCHARGER_BCHGCHARGESTATUS__TRICKLECHARGE = 4,
  PmicRegisters_BCHARGER_BCHGCHARGESTATUS__CONSTANTCURRENT = 8,
  PmicRegisters_BCHARGER_BCHGCHARGESTATUS__CONSTANTVOLTAGE = 16,
  PmicRegisters_BCHARGER_BCHGERRREASON = 0x0336,
  PmicRegisters_BCHARGER_BCHGDEBUG = 0x0346,
  PmicRegisters_BCHARGER_BCHGDEBUG__DISABLEBATTERYDETECT = 0x04,
  PmicRegisters_BCHARGER_BCHGVBATLOWCHARGE = 0x0350,
  PmicRegisters_ADC_TASKVBATMEASURE  = 0x0500,
  PmicRegisters_ADC_TASKNTCMEASURE   = 0x0501,
  PmicRegisters_ADC_TASKVSYSMEASURE  = 0x0503,
  PmicRegisters_ADC_TASKIBATMEASURE  = 0x0506,
  PmicRegisters_ADC_TASKVBUS7MEASURE = 0x0507,
  PmicRegisters_ADC_ADCIBATMEASSTATUS = 0x0510,
  PmicRegisters_ADC_ADCIBATMEASSTATUS__BCHARGERMODE_MASK = 0x0C,
  PmicRegisters_ADC_ADCIBATMEASSTATUS__BCHARGERMODE_DISCHRG = 0x04,
  PmicRegisters_ADC_ADCIBATMEASSTATUS__BCHARGERMODE_CHRG = 0x0C,
  PmicRegisters_ADC_ADCNTCRSEL = 0x050AU,
  PmicRegisters_ADC_ADCNTCRSEL__ADCNTCRSEL_HIZ = 0x0U,
  PmicRegisters_ADC_ADCNTCRSEL__ADCNTCRSEL_10K = 0x1U,
  PmicRegisters_ADC_ADCVBATRESULTMSB = 0x0511,
  PmicRegisters_ADC_ADCNTCRESULTMSB = 0x512,
  PmicRegisters_ADC_ADCVSYSRESULTMSB = 0x0514,
  PmicRegisters_ADC_ADCGP0RESULTLSBS = 0x0515,
  PmicRegisters_ADC_ADCGP0RESULTLSBS_VBATRESULTLSB_MSK = 0x03,
  PmicRegisters_ADC_ADCGP0RESULTLSBS_VBATRESULTLSB_POS = 0U,
  PmicRegisters_ADC_ADCGP0RESULTLSBS_NTCRESULTLSB_MSK = 0x03,
  PmicRegisters_ADC_ADCGP0RESULTLSBS_NTCRESULTLSB_POS = 2U,
  PmicRegisters_ADC_ADCVBAT2RESULTMSB = 0x0518,
  PmicRegisters_ADC_ADCGP1RESULTLSBS = 0x051a,
  PmicRegisters_ADC_ADCGP1RESULTLSBS_VBAT2RESULTLSB_MSK = 0x03,
  PmicRegisters_ADC_ADCGP1RESULTLSBS_VBAT2RESULTLSB_POS = 0x04,
  PmicRegisters_ADC_ADCIBATMEASEN = 0x0524,
  PmicRegisters_GPIOS_GPIOMODE1 = 0x0601,
  PmicRegisters_GPIOS_GPIOMODE__GPOIRQ = 5,
  PmicRegisters_GPIOS_GPIOOPENDRAIN1 = 0x0615,
  PmicRegisters_ERRLOG_SCRATCH0 = 0x0E01,
  PmicRegisters_ERRLOG_SCRATCH1 = 0x0E02,
  PmicRegisters_BUCK_BUCK1NORMVOUT = 0x0408,
  PmicRegisters_BUCK_BUCK2NORMVOUT = 0x040A,
  PmicRegisters_BUCK_BUCKSWCTRLSEL = 0x040F,
  PmicRegisters_BUCK_BUCKSTATUS = 0x0434,
  PmicRegisters_LDSW_TASKLDSW1SET = 0x0800,
  PmicRegisters_LDSW_TASKLDSW1CLR = 0x0801,
  PmicRegisters_LDSW_TASKLDSW2SET = 0x0802,
  PmicRegisters_LDSW_TASKLDSW2CLR = 0x0803,
  PmicRegisters_LDSW_LDSWSTATUS = 0x0804,
  PmicRegisters_LDSW_LDSWSTATUS__LDSW2PWRUPLDO = 0x08,
  PmicRegisters_LDSW_LDSWCONFIG = 0x0807,
  PmicRegisters_LDSW_LDSW1LDOSEL = 0x0808,
  PmicRegisters_LDSW_LDSW2LDOSEL = 0x0809,
  PmicRegisters_LDSW_LDSW1VOUTSEL = 0x080C,
  PmicRegisters_LDSW_LDSW2VOUTSEL = 0x080D,
  PmicRegisters_SHIP_TASKSHPHLDCFGSTROBE = 0x0B01,
  PmicRegisters_SHIP_TASKENTERSHIPMODE = 0x0B02,
  PmicRegisters_SHIP_SHPHLDCONFIG = 0x0B04,
  PmicRegisters_SHIP_SHPHLDCONFIG__SHPHLDTIM_96MS = 3,
} PmicRegisters;

#define NPM1300_BCHGISETDISCHARGEMSB_200MA 42U
#define NPM1300_BCHGISETDISCHARGELSB_200MA 0U
#define NPM1300_BCHGISETDISCHARGEMSB_1000MA 207U
#define NPM1300_BCHGISETDISCHARGELSB_1000MA 1U

#define NPM1300_BCHARGER_ADC_BITS_RESOLUTION 1023
#define NPM1300_BCHARGER_ADC_CALC_DISCHARGE_MUL 112
#define NPM1300_BCHARGER_ADC_CALC_DISCHARGE_DIV 100
#define NPM1300_BCHARGER_ADC_CALC_CHARGE_MUL 1250
#define NPM1300_BCHARGER_ADC_CALC_CHARGE_DIV -1000
// Full scale voltage for battery voltage measurement
#define NPM1300_ADC_VFS_VBAT_MV 5000UL
// ADC MSB shift
#define NPM1300_ADC_MSB_SHIFT 2U


void battery_init(void) {
}

uint32_t pmic_get_last_reset_reason(void) {
  return 0;
}

static bool prv_read_register(uint16_t register_address, uint8_t *result) {
  mutex_lock(s_i2c_lock);
  i2c_use(I2C_NPM1300);
  uint8_t regad[2] = { register_address >> 8, register_address & 0xFF };
  bool rv = i2c_write_block(I2C_NPM1300, 2, regad);
  if (rv)
    rv = i2c_read_block(I2C_NPM1300, 1, result);
  i2c_release(I2C_NPM1300);
  mutex_unlock(s_i2c_lock);
  return rv;
}

static bool prv_write_register(uint16_t register_address, uint8_t datum) {
  mutex_lock(s_i2c_lock);
  i2c_use(I2C_NPM1300);
  uint8_t d[3] = { register_address >> 8, register_address & 0xFF, datum };
  bool rv = i2c_write_block(I2C_NPM1300, 3, d);
  i2c_release(I2C_NPM1300);
  mutex_unlock(s_i2c_lock);
  return rv;
}

static void prv_handle_charge_state_change(void *null) {
  const bool is_charging = pmic_is_charging();
  const bool is_connected = pmic_is_usb_connected();
  PBL_LOG(LOG_LEVEL_DEBUG, "nPM1300 Interrupt: Charging? %s Plugged? %s",
      is_charging ? "YES" : "NO", is_connected ? "YES" : "NO");

  PebbleEvent event = {
    .type = PEBBLE_BATTERY_CONNECTION_EVENT,
    .battery_connection = {
      .is_connected = battery_is_usb_connected(),
    },
  };
  event_put(&event);
}

static void prv_clear_pending_interrupts() {
  prv_write_register(PmicRegisters_MAIN_EVENTSBCHARGER1CLR, PmicRegisters_MAIN_EVENTSBCHARGER1__EVENTCHGCOMPLETED);
  prv_write_register(PmicRegisters_MAIN_EVENTSVBUSIN0CLR, PmicRegisters_MAIN_EVENTSVBUSIN0__EVENTVBUSDETECTED | PmicRegisters_MAIN_EVENTSVBUSIN0__EVENTVBUSREMOVED);
}

static void prv_pmic_state_change_cb(void *null) {
  prv_clear_pending_interrupts();
  new_timer_start(s_debounce_charger_timer, CHARGER_DEBOUNCE_MS,
                  prv_handle_charge_state_change, NULL, 0 /*flags*/);
}

static void prv_npm1300_interrupt_handler(bool *should_context_switch) {
  system_task_add_callback_from_isr(prv_pmic_state_change_cb, NULL, should_context_switch);
}

static void prv_configure_interrupts(void) {
  prv_clear_pending_interrupts();

  exti_configure_pin(BOARD_CONFIG_POWER.pmic_int, ExtiTrigger_Rising, prv_npm1300_interrupt_handler);
  exti_enable(BOARD_CONFIG_POWER.pmic_int);
}

bool pmic_init(void) {
  bool ok = true;
  uint8_t val;

  s_i2c_lock = mutex_create();
  s_debounce_charger_timer = new_timer_create();

  // TODO(NPM1300): This needs to be configurable at board level
#if PLATFORM_ASTERIX
  uint8_t buck_out;
  if (!prv_read_register(PmicRegisters_BUCK_BUCK1NORMVOUT, &buck_out)) {
    PBL_LOG(LOG_LEVEL_ERROR, "failed to read BUCK1NORMVOUT");
    return false;
  }
  PBL_LOG(LOG_LEVEL_DEBUG, "found the nPM1300, BUCK1NORMVOUT = 0x%x", buck_out);
  
  // work around erratum 27 for nPM1300 rev1, which we tripped in the bootloader (oops)
  ok &= prv_write_register(PmicRegisters_BUCK_BUCK1NORMVOUT, 9 /* 1.9V */);
  ok &= prv_write_register(PmicRegisters_BUCK_BUCK2NORMVOUT, 21 /* 3.1V */);
  ok &= prv_write_register(PmicRegisters_BUCK_BUCKSWCTRLSEL, 3 /* both of them, load */);
  ok &= prv_write_register(PmicRegisters_BUCK_BUCK1NORMVOUT, 8 /* 1.8V */);
  ok &= prv_write_register(PmicRegisters_BUCK_BUCK2NORMVOUT, 20 /* 3.0V */);
  ok &= prv_write_register(PmicRegisters_BUCK_BUCKSWCTRLSEL, 3 /* both of them, load */);
  
  if (!prv_read_register(PmicRegisters_LDSW_LDSWSTATUS, &val)) {
    PBL_LOG(LOG_LEVEL_ERROR, "failed to read LDSWSTATUS");
    return false;
  }

  if ((val & PmicRegisters_LDSW_LDSWSTATUS__LDSW2PWRUPLDO) == 0U) {
    ok &= prv_write_register(PmicRegisters_LDSW_TASKLDSW2CLR, 0x01);
    ok &= prv_write_register(PmicRegisters_LDSW_LDSW2VOUTSEL, 8 /* 1.8V */);
    ok &= prv_write_register(PmicRegisters_LDSW_LDSW2LDOSEL, 1 /* LDO */);
    ok &= prv_write_register(PmicRegisters_LDSW_TASKLDSW2SET, 0x01);
  } else {
    ok &= prv_write_register(PmicRegisters_LDSW_LDSW2VOUTSEL, 8 /* 1.8V */);
  }
#endif

  ok &= prv_write_register(PmicRegisters_MAIN_EVENTSBCHARGER1CLR, PmicRegisters_MAIN_EVENTSBCHARGER1__EVENTCHGCOMPLETED);
  ok &= prv_write_register(PmicRegisters_MAIN_INTENEVENTSBCHARGER1SET, PmicRegisters_MAIN_EVENTSBCHARGER1__EVENTCHGCOMPLETED);
  ok &= prv_write_register(PmicRegisters_MAIN_EVENTSVBUSIN0CLR, PmicRegisters_MAIN_EVENTSVBUSIN0__EVENTVBUSDETECTED | PmicRegisters_MAIN_EVENTSVBUSIN0__EVENTVBUSREMOVED);
  ok &= prv_write_register(PmicRegisters_MAIN_INTENEVENTSVBUSIN0SET, PmicRegisters_MAIN_EVENTSVBUSIN0__EVENTVBUSDETECTED | PmicRegisters_MAIN_EVENTSVBUSIN0__EVENTVBUSREMOVED);
  ok &= prv_write_register(PmicRegisters_GPIOS_GPIOMODE1, PmicRegisters_GPIOS_GPIOMODE__GPOIRQ);
  ok &= prv_write_register(PmicRegisters_GPIOS_GPIOOPENDRAIN1, 0);

  ok &= prv_write_register(PmicRegisters_SHIP_SHPHLDCONFIG, PmicRegisters_SHIP_SHPHLDCONFIG__SHPHLDTIM_96MS);
  ok &= prv_write_register(PmicRegisters_SHIP_TASKSHPHLDCFGSTROBE, 1);

  // automatic IBAT measurement after VBAT
  ok &= prv_write_register(PmicRegisters_ADC_ADCIBATMEASEN, 1);

  if ((NPM1300_CONFIG.chg_current_ma < 32U) || (NPM1300_CONFIG.chg_current_ma > 800U) ||
      (NPM1300_CONFIG.chg_current_ma % 2U != 0U)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid charge current: %d mA", NPM1300_CONFIG.chg_current_ma);
    return false;
  }

  ok &= prv_write_register(PmicRegisters_BCHARGER_BCHGENABLECLR, 1);

  ok &= prv_write_register(PmicRegisters_BCHARGER_TASKCLEARCHGERR, 1);
  ok &= prv_write_register(PmicRegisters_BCHARGER_TASKRELEASEERROR, 1);

  // FIXME: this needs to be configurable at board level
#if PLATFORM_ASTERIX
  ok &= prv_write_register(PmicRegisters_ADC_ADCNTCRSEL, PmicRegisters_ADC_ADCNTCRSEL__ADCNTCRSEL_10K);
#elif PLATFORM_OBELIX
  // FIXME(OBELIX): NTC not working
  ok &= prv_write_register(PmicRegisters_ADC_ADCNTCRSEL, PmicRegisters_ADC_ADCNTCRSEL__ADCNTCRSEL_HIZ);
#endif

  // FIXME: this needs to be configurable at board level
#if PLATFORM_ASTERIX || PLATFORM_OBELIX
  ok &= prv_write_register(PmicRegisters_BCHARGER_BCHGVTERM, PmicRegisters_BCHARGER_BCHGVTERM__BCHGVTERMNORM_4V20);
  ok &= prv_write_register(PmicRegisters_BCHARGER_BCHGVTERMR, PmicRegisters_BCHARGER_BCHGVTERMR__BCHGVTERMREDUCED_4V00);
#endif

  val = (uint8_t)(NPM1300_CONFIG.chg_current_ma / 4U);
  ok &= prv_write_register(PmicRegisters_BCHARGER_BCHGISETMSB, val);
  val = (NPM1300_CONFIG.chg_current_ma / 2U) % 2U;
  ok &= prv_write_register(PmicRegisters_BCHARGER_BCHGISETLSB, val);

  if (NPM1300_CONFIG.dischg_limit_ma == 200) {
    ok &= prv_write_register(PmicRegisters_BCHARGER_BCHGISETDISCHARGEMSB,
                             NPM1300_BCHGISETDISCHARGEMSB_200MA);
    ok &= prv_write_register(PmicRegisters_BCHARGER_BCHGISETDISCHARGELSB,
                             NPM1300_BCHGISETDISCHARGELSB_200MA);
  } else if (NPM1300_CONFIG.dischg_limit_ma == 1000) {
    ok &= prv_write_register(PmicRegisters_BCHARGER_BCHGISETDISCHARGEMSB,
			     NPM1300_BCHGISETDISCHARGEMSB_1000MA);
    ok &= prv_write_register(PmicRegisters_BCHARGER_BCHGISETDISCHARGELSB,
			     NPM1300_BCHGISETDISCHARGELSB_1000MA);
  } else {
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid discharge limit: %d mA", NPM1300_CONFIG.dischg_limit_ma);
    return false;
  }

  if (NPM1300_CONFIG.term_current_pct == 10U) {
    ok &= prv_write_register(PmicRegisters_BCHARGER_BCHGITERMSEL,
                             PmicRegisters_BCHARGER_BCHGITERMSEL__SEL10);
  } else if(NPM1300_CONFIG.term_current_pct == 20U) {
    ok &= prv_write_register(PmicRegisters_BCHARGER_BCHGITERMSEL,
                             PmicRegisters_BCHARGER_BCHGITERMSEL__SEL20);
  } else {
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid termination current: %d", NPM1300_CONFIG.term_current_pct);
    return false;
  }

  ok &= prv_write_register(PmicRegisters_SYSTEM_TESTACCESS, 
                           PmicRegisters_SYSTEM_TESTACCESS__VAL0);
  ok &= prv_write_register(PmicRegisters_SYSTEM_TESTACCESS, 
                           PmicRegisters_SYSTEM_TESTACCESS__VAL1);
  ok &= prv_write_register(PmicRegisters_SYSTEM_TESTACCESS, 
                           PmicRegisters_SYSTEM_TESTACCESS__VAL2);
  ok &= prv_write_register(PmicRegisters_SYSTEM_TESTACCESS, 
                           PmicRegisters_SYSTEM_TESTACCESS__VAL3);

  ok &= prv_write_register(PmicRegisters_BCHARGER_BCHGDEBUG,
                           PmicRegisters_BCHARGER_BCHGDEBUG__DISABLEBATTERYDETECT);

  ok &= prv_write_register(PmicRegisters_BCHARGER_BCHGVBATLOWCHARGE, 1);

  ok &= prv_write_register(PmicRegisters_BCHARGER_BCHGENABLESET, 1);

  prv_configure_interrupts();

  if (!ok) {
    PBL_LOG(LOG_LEVEL_ERROR, "one or more PMIC transactions failed");
  }

  return ok;
}

bool pmic_power_off(void) {
  // TODO: review implementation, see GH-238
  if (pmic_is_usb_connected()) {
    PBL_LOG(LOG_LEVEL_ERROR, "USB is connected, cannot power off");
    return false;
  }

  if (!prv_write_register(PmicRegisters_SHIP_TASKENTERSHIPMODE, 1)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to enter ship mode");
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

int battery_get_millivolts(void) {
  if (!prv_write_register(PmicRegisters_MAIN_EVENTSADCCLR, 0x01 /* EVENTADCVBATRDY */)) {
    return 0;
  }
  if (!prv_write_register(PmicRegisters_ADC_TASKVBATMEASURE, 1)) {
    return 0;
  }
  uint8_t reg = 0;
  while ((reg & 0x01) == 0) {
    if (!prv_read_register(PmicRegisters_MAIN_EVENTSADCCLR, &reg)) {
      return 0;
    }
  }
  
  uint8_t vbat_msb;
  uint8_t lsbs;
  if (!prv_read_register(PmicRegisters_ADC_ADCVBATRESULTMSB, &vbat_msb)) {
    return 0;
  }
  if (!prv_read_register(PmicRegisters_ADC_ADCGP0RESULTLSBS, &lsbs)) {
    return 0;
  }
  uint16_t vbat_raw = (vbat_msb << 2) | (lsbs & 3);
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
  if (!prv_read_register(PmicRegisters_ADC_ADCIBATMEASSTATUS, &ibat_status)) {
    return -1;
  }

  if ((ibat_status & PmicRegisters_ADC_ADCIBATMEASSTATUS__BCHARGERMODE_MASK) ==
      PmicRegisters_ADC_ADCIBATMEASSTATUS__BCHARGERMODE_CHRG) {
    full_scale_ua =
        ((int32_t)NPM1300_CONFIG.chg_current_ma * 1000 * NPM1300_BCHARGER_ADC_CALC_CHARGE_MUL) /
        NPM1300_BCHARGER_ADC_CALC_CHARGE_DIV;
  } else {
    full_scale_ua =
        ((int32_t)NPM1300_CONFIG.dischg_limit_ma * 1000 * NPM1300_BCHARGER_ADC_CALC_DISCHARGE_MUL) /
        NPM1300_BCHARGER_ADC_CALC_DISCHARGE_DIV;
  }

  // Clear the ADC ready events for VBAT, IBAT, and NTC
  if (!prv_write_register(PmicRegisters_MAIN_EVENTSADCCLR,
                          PmicRegisters_MAIN_EVENTSADCCLR__EVENTADCVBATRDY |
                          PmicRegisters_MAIN_EVENTSADCCLR__EVENTADCIBATRDY |
                          PmicRegisters_MAIN_EVENTSADCCLR__EVENTADCNTCRDY)) {
    return -1;
  }

  // Trigger VBAT+IBAT measurement (IBATMEASENABLE is enabled)
  if (!prv_write_register(PmicRegisters_ADC_TASKVBATMEASURE, 1)) {
    return -1;
  }

  // Trigger NTC measurement
  if (!prv_write_register(PmicRegisters_ADC_TASKNTCMEASURE, 1)) {
    return -1;
  }

  // Process the VBAT measurement
  reg = 0U;
  while ((reg & PmicRegisters_MAIN_EVENTSADCCLR__EVENTADCVBATRDY) == 0U) {
    if (!prv_read_register(PmicRegisters_MAIN_EVENTSADCCLR, &reg)) {
      return -1;
    }
  }

  if (!prv_read_register(PmicRegisters_ADC_ADCVBATRESULTMSB, &msb)) {
    return -1;
  }

  if (!prv_read_register(PmicRegisters_ADC_ADCGP0RESULTLSBS, &lsb)) {
    return -1;
  }

  raw = (msb << NPM1300_ADC_MSB_SHIFT) |
        ((lsb & PmicRegisters_ADC_ADCGP0RESULTLSBS_VBATRESULTLSB_MSK) >>
         PmicRegisters_ADC_ADCGP0RESULTLSBS_VBATRESULTLSB_POS);

  constants->v_mv = (int32_t)(raw * NPM1300_ADC_VFS_VBAT_MV) / NPM1300_BCHARGER_ADC_BITS_RESOLUTION;

  // Process the IBAT measurement
  while ((reg & PmicRegisters_MAIN_EVENTSADCCLR__EVENTADCIBATRDY) == 0U) {
    if (!prv_read_register(PmicRegisters_MAIN_EVENTSADCCLR, &reg)) {
      return -1;
    }
  }

  if (!prv_read_register(PmicRegisters_ADC_ADCVBAT2RESULTMSB, &msb)) {
    return -1;
  }

  if (!prv_read_register(PmicRegisters_ADC_ADCGP1RESULTLSBS, &lsb)) {
    return -1;
  }

  raw = (msb << NPM1300_ADC_MSB_SHIFT) |
        ((lsb & PmicRegisters_ADC_ADCGP1RESULTLSBS_VBAT2RESULTLSB_MSK) >>
         PmicRegisters_ADC_ADCGP1RESULTLSBS_VBAT2RESULTLSB_POS);

  constants->i_ua = ((int32_t)raw * full_scale_ua) / NPM1300_BCHARGER_ADC_BITS_RESOLUTION;

  // Process the NTC measurement
  while ((reg & PmicRegisters_MAIN_EVENTSADCCLR__EVENTADCNTCRDY) == 0U) {
    if (!prv_read_register(PmicRegisters_MAIN_EVENTSADCCLR, &reg)) {
      return -1;
    }
  }

  if (!prv_read_register(PmicRegisters_ADC_ADCNTCRESULTMSB, &lsb)) {
    return -1;
  }

  if (!prv_read_register(PmicRegisters_ADC_ADCGP0RESULTLSBS, &msb)) {
    return -1;
  }

  raw = (lsb << NPM1300_ADC_MSB_SHIFT) |
        ((msb & PmicRegisters_ADC_ADCGP0RESULTLSBS_NTCRESULTLSB_MSK) >>
         PmicRegisters_ADC_ADCGP0RESULTLSBS_NTCRESULTLSB_POS);

  // Ref: PS v1.2 Section 7.1.4: Battery temperature (Kelvin)
  float log_result = logf((1024.f / (float)raw) - 1.0f);
  float inv_temp_k = (1.f / 298.15f) - (log_result / (float)NPM1300_CONFIG.thermistor_beta);

  constants->t_mc = (int32_t)(1000.0f * ((1.f / inv_temp_k) - 273.15f));

  return 0;
}

bool pmic_set_charger_state(bool enable) {
  return prv_write_register(enable ? PmicRegisters_BCHARGER_BCHGENABLESET : PmicRegisters_BCHARGER_BCHGENABLECLR, 1);
}

void battery_set_charge_enable(bool charging_enabled) {
  pmic_set_charger_state(charging_enabled);
}

void battery_set_fast_charge(bool fast_charge_enabled) {
  /* the PMIC handles this for us */
}

bool pmic_is_charging(void) {
  uint8_t status;
  if (!prv_read_register(PmicRegisters_BCHARGER_BCHGCHARGESTATUS, &status)) {
    return false;
  }

  return (status & (PmicRegisters_BCHARGER_BCHGCHARGESTATUS__TRICKLECHARGE | PmicRegisters_BCHARGER_BCHGCHARGESTATUS__CONSTANTCURRENT | PmicRegisters_BCHARGER_BCHGCHARGESTATUS__CONSTANTVOLTAGE)) != 0;
}

bool battery_charge_controller_thinks_we_are_charging_impl(void) {
  return pmic_is_charging();
}

bool pmic_is_usb_connected(void) {
  uint8_t status;
  if (!prv_read_register(PmicRegisters_VBUSIN_VBUSINSTATUS, &status)) {
    return false;
  }

  return (status & PmicRegisters_VBUSIN_VBUSINSTATUS__VBUSINPRESENT) != 0;
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

  if (!prv_read_register(PmicRegisters_BCHARGER_BCHGCHARGESTATUS, &chg_status)) {
    return -1;
  }

  switch (chg_status & (PmicRegisters_BCHARGER_BCHGCHARGESTATUS__COMPLETED |
                        PmicRegisters_BCHARGER_BCHGCHARGESTATUS__TRICKLECHARGE |
                        PmicRegisters_BCHARGER_BCHGCHARGESTATUS__CONSTANTCURRENT |
                        PmicRegisters_BCHARGER_BCHGCHARGESTATUS__CONSTANTVOLTAGE)) {
    case PmicRegisters_BCHARGER_BCHGCHARGESTATUS__COMPLETED:
      *status = BatteryChargeStatusComplete;
      break;
    case PmicRegisters_BCHARGER_BCHGCHARGESTATUS__TRICKLECHARGE:
      *status = BatteryChargeStatusTrickle;
      break;
    case PmicRegisters_BCHARGER_BCHGCHARGESTATUS__CONSTANTCURRENT:
      *status = BatteryChargeStatusCC;
      break;
    case PmicRegisters_BCHARGER_BCHGCHARGESTATUS__CONSTANTVOLTAGE:
      *status = BatteryChargeStatusCV;
      break;
    default:
      *status = BatteryChargeStatusUnknown;
      break;
  }

  return 0;
}

void command_pmic_read_registers(void) {
  char buffer[64];
#define SAY(x) do { uint8_t reg; int rv = prv_read_register(PmicRegisters_##x, &reg); prompt_send_response_fmt(buffer, sizeof(buffer), "PMIC: " #x " = %02x (rv %d)", reg, rv); } while(0)
  SAY(ERRLOG_SCRATCH0);
  SAY(ERRLOG_SCRATCH1);
  SAY(BUCK_BUCK1NORMVOUT);
  SAY(BUCK_BUCK2NORMVOUT);
  SAY(BUCK_BUCKSTATUS);
  SAY(VBUSIN_VBUSINSTATUS);
  SAY(BCHARGER_BCHGCHARGESTATUS);
  SAY(BCHARGER_BCHGERRREASON);
  prompt_send_response_fmt(buffer, sizeof(buffer), "PMIC: Vsys = %d mV", pmic_get_vsys());
  prompt_send_response_fmt(buffer, sizeof(buffer), "PMIC: Vbat = %d mV", battery_get_millivolts());
}

void command_pmic_status(void) {
}

void command_pmic_rails(void) {
  // TODO: Implement.
}
