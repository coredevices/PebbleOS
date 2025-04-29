#include <board.h>
#include <drivers/dbgserial.h>
#include <nrfx_twi.h>

#define VBUSIN_BASE 0x02U
#define CHARGER_BASE 0x03U
#define BUCK_BASE 0x04U
#define ADC_BASE 0x05U
#define GPIOS_BASE 0x06U
#define TIMER_BASE 0x07U
#define LDSW_BASE 0x08U
#define SHIP_BASE 0x0BU
#define ERRLOG_BASE 0x0EU

// VBUSIN
#define VBUSINILIMSTARTUP 0x02U
#define VBUSLIM_500MA 0x00U

// CHARGER
#define TASKRELEASEERROR 0x0U

#define TASKCLEARCHGERR 0x1U

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
#define BUCK1ENASET 0x0
#define BUCK2ENASET 0x2

#define BUCK1PWMCLR 0x5U
#define BUCK2PWMCLR 0x7U
#define BUCKPWMCLR_SET 0x01U

#define BUCK1NORMVOUT 0x8U
#define BUCK1RETVOUT 0x9U
#define BUCK2NORMVOUT 0xAU
#define BUCK2RETVOUT 0xBU
#define BUCKVOUT_1V8 8U
#define BUCKVOUT_3V0 20U

#define BUCKENCTRL 0xC
#define BUCKVRETCTRL 0xD
#define BUCKPWMCTRL 0xE

#define BUCKSWCTRLSET 0xFU
#define BUCKSWCTRLSET_BUCK1SWCTRLSET 0x01U
#define BUCKSWCTRLSET_BUCK2SWCTRLSET 0x02U

#define BUCKCTRL0 0x15U

// ADC
#define ADCNTCRSEL 0x0AU
#define ADCNTCRSEL_10K 0x1U

// GPIOS
#define GPIOMODE0 0x0U
#define GPIOMODE1 0x1U
#define GPIOMODE2 0x2U
#define GPIOMODE3 0x3U
#define GPIOMODE4 0x4U

#define GPIOMODE_GPIINPUT 0U
#define GPIOMODE_GPIEVENTFALL 4U
#define GPIOMODE_GPOIRQ 5U

#define GPIOPUEN0 0xAU
#define GPIOOPENDRAIN0 0x14U

// TIMER
#define TIMERCLR 0x01U
#define TIMERCLR_TASKTIMERDIS 0x01U

// LDO
#define TASKLDSW1SET 0x00U
#define TASKLDSW2SET 0x02U

#define LDSW1GPISEL 0x05U
#define LDSW2GPISEL 0x06U

#define LDSW1LDOSEL 0x08U
#define LDSW2LDOSEL 0x09U
#define LDSWLDOSEL_LDO 0x01U

#define LDSW1VOUTSEL 0x0CU
#define LDSW2VOUTSEL 0x0DU
#define LDSWVOUTSEL_1V8 0x08U

// SHIP
#define TASKSHPHLDCONFIGSTROBE 0x1U
#define LPRESETCFG 0x6U

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

struct pmic_reg {
  uint8_t base;
  uint8_t reg;
  uint8_t val;
};

int pmic_init(void) {
  int ret;
  nrfx_err_t err;

  err = nrfx_twi_init(&twi, &config, NULL, NULL);
  if (err != NRFX_SUCCESS) {
    return -1;
  }

  nrfx_twi_enable(&twi);
  
  const struct pmic_reg regs[] = {
    // Turn off any watchdog / boot timer right away.
    { TIMER_BASE, TIMERCLR, TIMERCLR_TASKTIMERDIS },
    { ERRLOG_BASE, SCRATCH0, 0x00 /* contains boot timer bit */ },
    
    // Make sure right away that we can reset the device if needed.
    { SHIP_BASE, LPRESETCFG, 0 },
    { SHIP_BASE, TASKSHPHLDCONFIGSTROBE, 1 },
    
    // Set up the BUCK1 regulator for manual control to 1.8V, automatic
    // PWM/hysteresis control.
    { BUCK_BASE, BUCK1ENASET, 1 },
    { BUCK_BASE, BUCK2ENASET, 1 },
    { BUCK_BASE, BUCK1PWMCLR, 1 },
    { BUCK_BASE, BUCK2PWMCLR, 1 },
    { BUCK_BASE, BUCK1NORMVOUT, BUCKVOUT_1V8 },
    { BUCK_BASE, BUCK1RETVOUT, BUCKVOUT_1V8 },
    { BUCK_BASE, BUCK2NORMVOUT, BUCKVOUT_3V0 },
    { BUCK_BASE, BUCK2RETVOUT, BUCKVOUT_3V0 },
    { BUCK_BASE, BUCKENCTRL, 0 },
    { BUCK_BASE, BUCKVRETCTRL, 0 },
    { BUCK_BASE, BUCKPWMCTRL, 0 },
    { BUCK_BASE, BUCKSWCTRLSET, BUCKSWCTRLSET_BUCK1SWCTRLSET | BUCKSWCTRLSET_BUCK2SWCTRLSET /* use registers rather than resistor settings */ },
    { BUCK_BASE, BUCKCTRL0, 0 },

    // Configure charger (TODO: values are board/battery dependent)
    // - Thermistor: 10K NTC
    // - Termination voltage: 4.2V
    // - Reduced termination voltage (for warm region): 4.00V
    // - Charge current limit of 152 mA (approximately 1C for most reasonable wearable batteries)
    // - Discharge current limit of 200 mA (increase current measurement accuracy)
    // - Release charger from error state if applicable (but do not clear
    //   safety timers) -- this doesn't happen in a loop because after we
    //   fail to boot three times, we will sit at sadwatch until a button is
    //   pressed
    // - Enable charging
    { VBUSIN_BASE, VBUSINILIMSTARTUP, VBUSLIM_500MA }, // should be default, but 'reset value from OTP, value listed in this table may not be correct'
    { CHARGER_BASE, BCHGENABLECLR, ENABLECHARGING_DISABLECHG },
    { ADC_BASE, ADCNTCRSEL, ADCNTCRSEL_10K },
    { CHARGER_BASE, BCHGVTERM, BCHGVTERMNORM_4V20 },
    { CHARGER_BASE, BCHGVTERMR, BCHGVTERMREDUCED_4V00 },
    { CHARGER_BASE, BCHGISETMSB, 38 },
    { CHARGER_BASE, BCHGISETDISCHARGEMSB, 42 },
    { CHARGER_BASE, TASKCLEARCHGERR, 1 },
    { CHARGER_BASE, TASKRELEASEERROR, 1 },
    { CHARGER_BASE, BCHGENABLESET, ENABLECHARGING_ENABLECHG },

    // LDO1 as LDO @ 1.8V (powers the DA7212 ... do not back power it through I/O pins, and it must always be on because sensors share I2C bus with it!)
    { LDSW_BASE, LDSW1GPISEL, 0 },
    { LDSW_BASE, LDSW1VOUTSEL, LDSWVOUTSEL_1V8 },
    { LDSW_BASE, LDSW1LDOSEL, LDSWLDOSEL_LDO },
    { LDSW_BASE, TASKLDSW1SET, 0x01U },

    // LDO2 as LDO @ 1.8V (powers the QSPI flash)
    { LDSW_BASE, LDSW2GPISEL, 0 },
    { LDSW_BASE, LDSW2VOUTSEL, LDSWVOUTSEL_1V8 },
    { LDSW_BASE, LDSW2LDOSEL, LDSWLDOSEL_LDO },
    { LDSW_BASE, TASKLDSW2SET, 0x01U },
    
    // Firmware will set up GPIOs as desired; set up everything as an input
    // now to avoid drive fights in case it was previously set strangely.
    { GPIOS_BASE, GPIOMODE0, GPIOMODE_GPIINPUT },
    { GPIOS_BASE, GPIOMODE1, GPIOMODE_GPIINPUT },
    { GPIOS_BASE, GPIOMODE2, GPIOMODE_GPIINPUT },
    { GPIOS_BASE, GPIOMODE3, GPIOMODE_GPIINPUT },
    { GPIOS_BASE, GPIOMODE4, GPIOMODE_GPIINPUT },
  };
  
  for (unsigned int i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
    ret = prv_pmic_write(regs[i].base, regs[i].reg, regs[i].val);
    if (ret != 0) {
      return ret;
    }
  }

  nrfx_twi_disable(&twi);

  return 0;
}
