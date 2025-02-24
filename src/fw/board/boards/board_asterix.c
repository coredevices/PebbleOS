#include "board/board.h"

#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "drivers/nrf5/uart_definitions.h"
#include "drivers/nrf5/i2c_hal_definitions.h"
#include "drivers/nrf5/spi_definitions.h"
#include "drivers/flash/qspi_flash_definitions.h"
#include "drivers/i2c_definitions.h"
#include "drivers/qspi_definitions.h"
#include "drivers/temperature.h"
#include "drivers/voltage_monitor.h"
#include "flash_region/flash_region.h"
#include "util/units.h"
#include "system/passert.h"
#include "kernel/util/sleep.h"
#include "drivers/pwm.h"

#include <nrfx_i2s.h>

// QSPI
#include <nrfx_qspi.h>
#include <nrfx_power_clock.h>
#include <hal/nrf_gpio.h>

#undef STRINGIFY
#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>


static QSPIPortState s_qspi_port_state;
static QSPIPort QSPI_PORT = {
  .state = &s_qspi_port_state,
  .auto_polling_interval = 16,
  .cs_gpio = NRF_GPIO_PIN_MAP(0, 17),
  .clk_gpio = NRF_GPIO_PIN_MAP(0, 19),
  .data_gpio = {
    NRF_GPIO_PIN_MAP(0, 20),
    NRF_GPIO_PIN_MAP(0, 21),
    NRF_QSPI_PIN_NOT_CONNECTED,
    NRF_QSPI_PIN_NOT_CONNECTED,
  },
};
QSPIPort * const QSPI = &QSPI_PORT;

static QSPIFlashState s_qspi_flash_state;
static QSPIFlash QSPI_FLASH_DEVICE = {
  .state = &s_qspi_flash_state,
  .qspi = &QSPI_PORT,
  .default_fast_read_ddr_enabled = false,
  .reset_gpio = { GPIO_Port_NULL },
};
QSPIFlash * const QSPI_FLASH = &QSPI_FLASH_DEVICE;
IRQ_MAP_NRFX(QSPI, nrfx_qspi_irq_handler);
/* PERIPHERAL ID 43 */

static UARTDeviceState s_dbg_uart_state;
static UARTDevice DBG_UART_DEVICE = {
  .state = &s_dbg_uart_state,
  .tx_gpio = NRF_GPIO_PIN_MAP(0, 27),
  .rx_gpio = NRF_GPIO_PIN_MAP(0, 5),
  .rts_gpio = NRF_UARTE_PSEL_DISCONNECTED,
  .cts_gpio = NRF_UARTE_PSEL_DISCONNECTED,
  .periph = NRFX_UARTE_INSTANCE(0),
  .counter = NRFX_TIMER_INSTANCE(2),
};
UARTDevice * const DBG_UART = &DBG_UART_DEVICE;
IRQ_MAP_NRFX(UARTE0_UART0, nrfx_uarte_0_irq_handler);
/* PERIPHERAL ID 8 */

/* buttons */
IRQ_MAP_NRFX(TIMER1, nrfx_timer_1_irq_handler);
IRQ_MAP_NRFX(TIMER2, nrfx_timer_2_irq_handler);

/* display */

IRQ_MAP_NRFX(SPIM3, nrfx_spim_3_irq_handler);
/* PERIPHERAL ID 10 */

/* EXTI */
IRQ_MAP_NRFX(GPIOTE, nrfx_gpiote_irq_handler);

/* nPM1300 */
static I2CBusState I2C_NPMC_IIC1_BUS_STATE = {};

static const I2CBusHal I2C_NPMC_IIC1_BUS_HAL = {
  .twim = NRFX_TWIM_INSTANCE(1),
  .frequency = NRF_TWIM_FREQ_400K,
};

static const I2CBus I2C_NPMC_IIC1_BUS = {
  .state = &I2C_NPMC_IIC1_BUS_STATE,
  .hal = &I2C_NPMC_IIC1_BUS_HAL,
  .scl_gpio = {
    .gpio = NRF5_GPIO_RESOURCE_EXISTS,
    .gpio_pin = NRF_GPIO_PIN_MAP(0, 14),
  },
  .sda_gpio = {
    .gpio = NRF5_GPIO_RESOURCE_EXISTS,
    .gpio_pin = NRF_GPIO_PIN_MAP(0, 15),
  },
  .name = "I2C_NPMC_IIC1"
};
IRQ_MAP_NRFX(SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1, nrfx_twim_1_irq_handler);
/* PERIPHERAL ID 9 */

static const I2CSlavePort I2C_SLAVE_NPM1300 = {
  .bus = &I2C_NPMC_IIC1_BUS,
  .address = 0xD6,
};

I2CSlavePort * const I2C_NPM1300 = &I2C_SLAVE_NPM1300;

/* peripheral I2C bus */
static I2CBusState I2C_IIC2_BUS_STATE = {};

static const I2CBusHal I2C_IIC2_BUS_HAL = {
  .twim = NRFX_TWIM_INSTANCE(0),
  .frequency = NRF_TWIM_FREQ_400K,
};

/* FIXME */
static const I2CBus I2C_IIC2_BUS = {
  .state = &I2C_IIC2_BUS_STATE,
  .hal = &I2C_IIC2_BUS_HAL,
  .scl_gpio = {
    .gpio = NRF5_GPIO_RESOURCE_EXISTS,
    .gpio_pin = NRF_GPIO_PIN_MAP(0, 25),
  },
  .sda_gpio = {
    .gpio = NRF5_GPIO_RESOURCE_EXISTS,
    .gpio_pin = NRF_GPIO_PIN_MAP(0, 11),
  },
  .name = "I2C_IIC2"
};
IRQ_MAP_NRFX(SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0, nrfx_twim_0_irq_handler);

/* PERIPHERAL ID 11 */

/* sensor SPI bus */

/* asterix_evt1 shares SPI with flash, which we don't support */

PwmState BACKLIGHT_PWM_STATE;
IRQ_MAP_NRFX(PWM0, nrfx_pwm_0_irq_handler);

IRQ_MAP_NRFX(POWER_CLOCK, nrfx_power_clock_irq_handler);

void board_early_init(void) {
  PBL_LOG(LOG_LEVEL_ERROR, "asterix early init");
  
  /* shared SPI chip outputs */
  nrf_gpio_cfg_output(15);
  nrf_gpio_cfg_output(16);
  nrf_gpio_pin_set(15);
  nrf_gpio_pin_set(16);
  
  /* do this now to turn on lfclk */
  ret_code_t rv; 
  rv = nrf_sdh_enable_request();
  PBL_ASSERTN(rv == NRF_SUCCESS);

  PBL_LOG(LOG_LEVEL_ERROR, "SDH enabled");
}

void board_init(void) {
  i2c_init(&I2C_NPMC_IIC1_BUS);
  i2c_init(&I2C_IIC2_BUS);

#if 0
  i2c_init(&I2C_PMIC_HRM_BUS);

  voltage_monitor_device_init(VOLTAGE_MONITOR_ALS);
  voltage_monitor_device_init(VOLTAGE_MONITOR_BATTERY);

  qspi_init(QSPI, BOARD_NOR_FLASH_SIZE);
#endif
}
