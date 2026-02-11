/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"

#include "drivers/exti.h"
#include "drivers/flash/qspi_flash_definitions.h"
#include "drivers/hrm/as7000/as7000.h"
#include "drivers/i2c_definitions.h"
#include "drivers/mic/stm32/dfsdm_definitions.h"
#include "drivers/qspi_definitions.h"
#include "drivers/stm32f2/dma_definitions.h"
#include "drivers/stm32f2/i2c_hal_definitions.h"
#include "drivers/stm32f2/spi_definitions.h"
#include "drivers/stm32f2/uart_definitions.h"
#include "drivers/temperature/analog.h"
#include "drivers/voltage_monitor.h"
#include "flash_region/flash_region.h"
#include "util/units.h"


// DMA Controllers

static DMAControllerState s_dma1_state;
static DMAController DMA1_DEVICE = {
  .state = &s_dma1_state,
  .periph = DMA1,
  .rcc_bit = RCC_AHB1Periph_DMA1,
};

static DMAControllerState s_dma2_state;
static DMAController DMA2_DEVICE = {
  .state = &s_dma2_state,
  .periph = DMA2,
  .rcc_bit = RCC_AHB1Periph_DMA2,
};

// DMA Streams

CREATE_DMA_STREAM(1, 4); // DMA2_STREAM2_DEVICE - Sharp SPI TX
CREATE_DMA_STREAM(2, 1); // DMA1_STREAM2_DEVICE - Accessory UART RX
CREATE_DMA_STREAM(2, 2); // DMA1_STREAM1_DEVICE - Debug UART RX
CREATE_DMA_STREAM(2, 6); // DMA2_STREAM4_DEVICE - DFSDM
CREATE_DMA_STREAM(2, 7); // DMA2_STREAM7_DEVICE - QSPI

// DMA Requests
// - On DMA1 we just have have "Sharp SPI TX" so just set its priority to "High" since it doesn't
//   matter.
// - On DMA2 we have "Accessory UART RX", "Debug UART RX", "DFSDM", and "QSPI". We want "DFSDM",
//   "Accessory UART RX", and "Debug UART RX" to have a very high priority because their peripheral
//   buffers may overflow if the DMA stream doesn't read from them in a while. After that, give the
//   remaining "QSPI" a high priority.

static DMARequestState s_sharp_spi_tx_dma_request_state;
static DMARequest SHARP_SPI_TX_DMA_REQUEST = {
  .state = &s_sharp_spi_tx_dma_request_state,
  .stream = &DMA1_STREAM4_DEVICE,
  .channel = 0,
  .irq_priority = 0x0f,
  .priority = DMARequestPriority_High,
  .type = DMARequestType_MemoryToPeripheral,
  .data_size = DMARequestDataSize_Byte,
};
DMARequest * const SHARP_SPI_TX_DMA = &SHARP_SPI_TX_DMA_REQUEST;

static DMARequestState s_dbg_uart_dma_request_state;
static DMARequest DBG_UART_RX_DMA_REQUEST = {
  .state = &s_dbg_uart_dma_request_state,
  .stream = &DMA2_STREAM2_DEVICE,
  .channel = 4,
  .irq_priority = IRQ_PRIORITY_INVALID, // no interrupts
  .priority = DMARequestPriority_VeryHigh,
  .type = DMARequestType_PeripheralToMemory,
  .data_size = DMARequestDataSize_Byte,
};

static DMARequestState s_dfsdm_dma_request_state;
static DMARequest DFSDM_DMA_REQUEST = {
  .state = &s_dfsdm_dma_request_state,
  .stream = &DMA2_STREAM6_DEVICE,
  .channel = 3,
  .irq_priority = 0x0f,
  .priority = DMARequestPriority_VeryHigh,
  .type = DMARequestType_PeripheralToMemory,
  .data_size = DMARequestDataSize_Word,
};

static DMARequestState s_qspi_dma_request_state;
static DMARequest QSPI_DMA_REQUEST = {
  .state = &s_qspi_dma_request_state,
  .stream = &DMA2_STREAM7_DEVICE,
  .channel = 3,
  .irq_priority = 0x0f,
  .priority = DMARequestPriority_High,
  .type = DMARequestType_PeripheralToMemory,
  .data_size = DMARequestDataSize_Word,
};


// UART DEVICES

static UARTDeviceState s_bt_bootrom_rx_uart_state;
static UARTDevice BT_RX_BOOTROM_UART_DEVICE = {
  .state = &s_bt_bootrom_rx_uart_state,
  .periph = USART6,
  .rx_gpio = { GPIOA, GPIO_Pin_12, GPIO_PinSource12, GPIO_AF_USART6 },
  .rcc_apb_periph = RCC_APB2Periph_USART6,
  .tx_gpio = { 0 }
};

static UARTDeviceState s_bt_bootrom_tx_uart_state;
static UARTDevice BT_TX_BOOTROM_UART_DEVICE = {
  .state = &s_bt_bootrom_tx_uart_state,
  .periph = USART2,
  .tx_gpio = { GPIOA, GPIO_Pin_2, GPIO_PinSource2, GPIO_AF_USART2 },
  .rcc_apb_periph = RCC_APB1Periph_USART2,
  .rx_gpio = { 0 }
};

UARTDevice * const BT_TX_BOOTROM_UART = &BT_TX_BOOTROM_UART_DEVICE;
UARTDevice * const BT_RX_BOOTROM_UART = &BT_RX_BOOTROM_UART_DEVICE;

#if TARGET_QEMU
static UARTDeviceState s_qemu_uart_state;
static UARTDevice QEMU_UART_DEVICE = {
  .state = &s_qemu_uart_state,
  // GPIO? Where we're going, we don't need GPIO. (connected to QEMU)
  .periph = USART2,
  .irq_channel = USART2_IRQn,
  .irq_priority = 13,
  .rcc_apb_periph = RCC_APB1Periph_USART2
};
UARTDevice * const QEMU_UART = &QEMU_UART_DEVICE;
IRQ_MAP(USART2, uart_irq_handler, QEMU_UART);
#endif

static UARTDeviceState s_dbg_uart_state;
static UARTDevice DBG_UART_DEVICE = {
  .state = &s_dbg_uart_state,
  .tx_gpio = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_9,
    .gpio_pin_source = GPIO_PinSource9,
    .gpio_af = GPIO_AF_USART1
  },
  .rx_gpio = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_7,
    .gpio_pin_source = GPIO_PinSource7,
    .gpio_af = GPIO_AF_USART1
  },
  .periph = USART1,
  .irq_channel = USART1_IRQn,
  .irq_priority = 13,
  .rcc_apb_periph = RCC_APB2Periph_USART1,
  .rx_dma = &DBG_UART_RX_DMA_REQUEST
};
UARTDevice * const DBG_UART = &DBG_UART_DEVICE;
IRQ_MAP(USART1, uart_irq_handler, DBG_UART);


// I2C DEVICES

static I2CBusState I2C_PMIC_HRM_BUS_STATE = {};

static const I2CBusHal I2C_PMIC_HRM_BUS_HAL = {
  .i2c = I2C3,
  .clock_ctrl = RCC_APB1Periph_I2C3,
  .clock_speed = 400000,
  .duty_cycle = I2CDutyCycle_2,
  .ev_irq_channel = I2C3_EV_IRQn,
  .er_irq_channel = I2C3_ER_IRQn,
};

static const I2CBus I2C_PMIC_HRM_BUS = {
  .state = &I2C_PMIC_HRM_BUS_STATE,
  .hal = &I2C_PMIC_HRM_BUS_HAL,
  .scl_gpio = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_8,
    .gpio_pin_source = GPIO_PinSource8,
    .gpio_af = GPIO_AF_I2C3
  },
  .sda_gpio = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_8,
    .gpio_pin_source = GPIO_PinSource8,
    .gpio_af = GPIO_AF9_I2C3
  },
  .stop_mode_inhibitor = InhibitorI2C3,
  .name = "I2C_PMIC"
};

static const I2CSlavePort I2C_SLAVE_AS3701B = {
  .bus = &I2C_PMIC_HRM_BUS,
  .address = 0x80
};

static const I2CSlavePort I2C_SLAVE_AS7000 = {
  .bus = &I2C_PMIC_HRM_BUS,
  .address = 0x60
};

I2CSlavePort * const I2C_AS3701B = &I2C_SLAVE_AS3701B;
I2CSlavePort * const I2C_AS7000 = &I2C_SLAVE_AS7000;

IRQ_MAP(I2C3_EV, i2c_hal_event_irq_handler, &I2C_PMIC_HRM_BUS);
IRQ_MAP(I2C3_ER, i2c_hal_error_irq_handler, &I2C_PMIC_HRM_BUS);

#if BOARD_SILK_FLINT
//We need this to get mag working on fake flint
static const I2CSlavePort I2C_SLAVE_MAG3110 = {
  .bus = &I2C_PMIC_HRM_BUS,
  .address = 0x1C
};
I2CSlavePort * const I2C_MAG3110 = &I2C_SLAVE_MAG3110;
#endif


// VOLTAGE MONITOR DEVICES
static const VoltageMonitorDevice VOLTAGE_MONITOR_ALS_DEVICE = {
  .adc = ADC1,
  .adc_channel = ADC_Channel_13,
  .clock_ctrl = RCC_APB2Periph_ADC1,
  .input = {
    .gpio = GPIOC,
    .gpio_pin = GPIO_Pin_3,
  },
};

static const VoltageMonitorDevice VOLTAGE_MONITOR_BATTERY_DEVICE = {
  .adc = ADC1,
  .adc_channel = ADC_Channel_5,
  .clock_ctrl = RCC_APB2Periph_ADC1,
  .input = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_5,
  },
};

static const VoltageMonitorDevice VOLTAGE_MONITOR_TEMPERATURE_DEVICE = {
  .adc = ADC1,
  .adc_channel = ADC_Channel_TempSensor,
  .clock_ctrl = RCC_APB2Periph_ADC1,
  // .input not applicable
};

const VoltageMonitorDevice * VOLTAGE_MONITOR_ALS = &VOLTAGE_MONITOR_ALS_DEVICE;
const VoltageMonitorDevice * VOLTAGE_MONITOR_BATTERY = &VOLTAGE_MONITOR_BATTERY_DEVICE;
const VoltageMonitorDevice * VOLTAGE_MONITOR_TEMPERATURE = &VOLTAGE_MONITOR_TEMPERATURE_DEVICE;

// Temperature sensor
// STM32F412 datasheet rev 2
// Section 6.3.21
AnalogTemperatureSensor const TEMPERATURE_SENSOR_DEVICE = {
  .voltage_monitor = &VOLTAGE_MONITOR_TEMPERATURE_DEVICE,
  .millivolts_ref = 760,
  .millidegrees_ref = 25000,
  .slope_numerator = 5,
  .slope_denominator = 2000,
};

AnalogTemperatureSensor * const TEMPERATURE_SENSOR = &TEMPERATURE_SENSOR_DEVICE;


// HRM DEVICE
static HRMDeviceState s_hrm_state;
static HRMDevice HRM_DEVICE = {
  .state = &s_hrm_state,
  .handshake_int = { EXTI_PortSourceGPIOA, 15 },
  .int_gpio = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_15
  },
  .en_gpio = {
    .gpio = GPIOC,
    .gpio_pin = GPIO_Pin_1,
    .active_high = false,
  },
  .i2c_slave = &I2C_SLAVE_AS7000,
};
HRMDevice * const HRM = &HRM_DEVICE;


// QSPI
static QSPIPortState s_qspi_port_state;
static QSPIPort QSPI_PORT = {
  .state = &s_qspi_port_state,
  .clock_speed_hz = MHZ_TO_HZ(50),
  .auto_polling_interval = 16,
  .clock_ctrl = RCC_AHB3Periph_QSPI,
  .cs_gpio = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_6,
    .gpio_pin_source = GPIO_PinSource6,
    .gpio_af = GPIO_AF10_QUADSPI,
  },
  .clk_gpio = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_2,
    .gpio_pin_source = GPIO_PinSource2,
    .gpio_af = GPIO_AF9_QUADSPI,
  },
  .data_gpio = {
    {
      .gpio = GPIOC,
      .gpio_pin = GPIO_Pin_9,
      .gpio_pin_source = GPIO_PinSource9,
      .gpio_af = GPIO_AF9_QUADSPI,
    },
    {
      .gpio = GPIOC,
      .gpio_pin = GPIO_Pin_10,
      .gpio_pin_source = GPIO_PinSource10,
      .gpio_af = GPIO_AF9_QUADSPI,
    },
    {
      .gpio = GPIOC,
      .gpio_pin = GPIO_Pin_8,
      .gpio_pin_source = GPIO_PinSource8,
      .gpio_af = GPIO_AF9_QUADSPI,
    },
    {
      .gpio = GPIOA,
      .gpio_pin = GPIO_Pin_1,
      .gpio_pin_source = GPIO_PinSource1,
      .gpio_af = GPIO_AF9_QUADSPI,
    },
  },
  .dma = &QSPI_DMA_REQUEST,
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


static MicDeviceState s_mic_state;
static MicDevice MIC_DEVICE = {
  .state = &s_mic_state,

  .filter = (DFSDM_TypeDef *) DFSDM1_Filter0_BASE,
  .channel = DFSDM1_Channel2,
  .extremes_detector_channel = DFSDM_ExtremChannel2,
  .regular_channel = DFSDM_RegularChannel2,
  .pdm_frequency = MHZ_TO_HZ(2),
  .rcc_apb_periph = RCC_APB2Periph_DFSDM,
  .dma = &DFSDM_DMA_REQUEST,
  .ck_gpio = { GPIOC, GPIO_Pin_2, GPIO_PinSource2, GPIO_AF8_DFSDM1 },
  .sd_gpio = { GPIOB, GPIO_Pin_14, GPIO_PinSource14, GPIO_AF8_DFSDM1 },
  .power_on_delay_ms = 50,
  .settling_delay_ms = 0,
  .default_volume = 64,
  .final_right_shift = 11,
};
MicDevice * const MIC = &MIC_DEVICE;


void board_early_init(void) {
}

void board_init(void) {
  i2c_init(&I2C_PMIC_HRM_BUS);

  voltage_monitor_device_init(VOLTAGE_MONITOR_ALS);
  voltage_monitor_device_init(VOLTAGE_MONITOR_BATTERY);

  qspi_init(QSPI, BOARD_NOR_FLASH_SIZE);
}
