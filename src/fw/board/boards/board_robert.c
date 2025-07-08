/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "board/board.h"

#include "drivers/display/ice40lp/ice40lp_definitions.h"
#include "drivers/exti.h"
#include "drivers/flash/qspi_flash_definitions.h"
#include "drivers/hrm/as7000.h"
#include "drivers/i2c_definitions.h"
#include "drivers/pmic.h"
#include "drivers/qspi_definitions.h"
#include "drivers/stm32f2/dma_definitions.h"
#include "drivers/stm32f2/spi_definitions.h"
#include "drivers/stm32f7/i2c_hal_definitions.h"
#include "drivers/stm32f7/uart_definitions.h"
#include "drivers/temperature/analog.h"
#include "drivers/touch/ewd1000/touch_sensor_definitions.h"
#include "drivers/voltage_monitor.h"
#include "flash_region/flash_region.h"
#include "util/units.h"

#define DIALOG_SPI_DMA_PRIORITY (0x0b)
// Make sure that the DMA IRQ is handled before EXTI:
// See comments in host/host_transport.c prv_int_exti_cb()
_Static_assert(DIALOG_SPI_DMA_PRIORITY < EXTI_PRIORITY, "Dialog SPI DMA priority too low!");

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

CREATE_DMA_STREAM(1, 1); // DMA1_STREAM1_DEVICE - Debug UART RX
CREATE_DMA_STREAM(1, 2); // DMA1_STREAM2_DEVICE - Accessory UART RX
CREATE_DMA_STREAM(2, 0); // DMA2_STREAM0_DEVICE - Dialog SPI RX
CREATE_DMA_STREAM(2, 1); // DMA2_STREAM1_DEVICE - Dialog SPI TX
CREATE_DMA_STREAM(2, 2); // DMA2_STREAM2_DEVICE - Compositor DMA
CREATE_DMA_STREAM(2, 4); // DMA2_STREAM4_DEVICE - DFSDM
CREATE_DMA_STREAM(2, 5); // DMA2_STREAM5_DEVICE - ICE40LP TX
CREATE_DMA_STREAM(2, 7); // DMA2_STREAM7_DEVICE - QSPI

// DMA Requests
// - On DMA1 we have "Debug UART RX" and "Accessory UART RX". The former is never used in a sealed
//   watch, and the latter is only sometimes used in a sealed watch. So, we don't really care about
//   their priorities and set them both to "High".
// - On DMA2 we have "Dialog SPI RX", "Dialog SPI TX", "Compositor DMA", "DFSDM", "ICE40LP TX", and
//   "QSPI". We want "DFSDM" and "Dialog SPI RX" to have a very high priority because their
//   peripheral buffers may overflow if the DMA stream doesn't read from them in a while. After
//   that, we want communication with the BLE chip and QSPI reads to be low-latency so give them a
//   high priority. Lastly, writing to the display prevents us from rendering the next frame, so
//   give the "ICE40LP TX" and "Compositor" DMAs a medium priority.

static DMARequestState s_dbg_uart_dma_request_state;
static DMARequest DBG_UART_RX_DMA_REQUEST = {
  .state = &s_dbg_uart_dma_request_state,
  .stream = &DMA1_STREAM1_DEVICE,
  .channel = 4,
  .irq_priority = IRQ_PRIORITY_INVALID, // no interrupts
  .priority = DMARequestPriority_High,
  .type = DMARequestType_PeripheralToMemory,
  .data_size = DMARequestDataSize_Byte,
};

static DMARequestState s_accessory_uart_dma_request_state;
static DMARequest ACCESSORY_UART_RX_DMA_REQUEST = {
  .state = &s_accessory_uart_dma_request_state,
  .stream = &DMA1_STREAM2_DEVICE,
  .channel = 4,
  .irq_priority = IRQ_PRIORITY_INVALID, // no interrupts
  .priority = DMARequestPriority_High,
  .type = DMARequestType_PeripheralToMemory,
  .data_size = DMARequestDataSize_Byte,
};

static DMARequestState s_dialog_spi_rx_dma_request_state;
static DMARequest DIALOG_SPI_RX_DMA_REQUEST = {
  .state = &s_dialog_spi_rx_dma_request_state,
  .stream = &DMA2_STREAM0_DEVICE,
  .channel = 4,
  .irq_priority = DIALOG_SPI_DMA_PRIORITY,
  .priority = DMARequestPriority_VeryHigh,
  .type = DMARequestType_PeripheralToMemory,
  .data_size = DMARequestDataSize_Byte,
};

static DMARequestState s_dialog_spi_tx_dma_request_state;
static DMARequest DIALOG_SPI_TX_DMA_REQUEST = {
  .state = &s_dialog_spi_tx_dma_request_state,
  .stream = &DMA2_STREAM1_DEVICE,
  .channel = 4,
  .irq_priority = DIALOG_SPI_DMA_PRIORITY,
  .priority = DMARequestPriority_High,
  .type = DMARequestType_MemoryToPeripheral,
  .data_size = DMARequestDataSize_Byte,
};

static DMARequestState s_compositor_dma_request_state;
static DMARequest COMPOSITOR_DMA_REQUEST = {
  .state = &s_compositor_dma_request_state,
  .stream = &DMA2_STREAM2_DEVICE,
  .channel = 0,
  .irq_priority = 11,
  .priority = DMARequestPriority_Medium,
  .type = DMARequestType_MemoryToMemory,
  .data_size = DMARequestDataSize_Byte,
};
DMARequest * const COMPOSITOR_DMA = &COMPOSITOR_DMA_REQUEST;

static DMARequestState s_dfsdm_dma_request_state;
static DMARequest DFSDM_DMA_REQUEST = {
  .state = &s_dfsdm_dma_request_state,
  .stream = &DMA2_STREAM4_DEVICE,
  .channel = 8,
  .irq_priority = 0x0f,
  .priority = DMARequestPriority_VeryHigh,
  .type = DMARequestType_PeripheralToMemory,
  .data_size = DMARequestDataSize_Word,
};

static DMARequestState s_ice40lp_spi_tx_dma_request_state;
static DMARequest ICE40LP_SPI_TX_DMA_REQUEST = {
  .state = &s_ice40lp_spi_tx_dma_request_state,
  .stream = &DMA2_STREAM5_DEVICE,
  .channel = 1,
  // Use the same priority as the EXTI handlers so that the DMA-complete
  // handler doesn't preempt the display BUSY (INTn) handler.
  .irq_priority = 0x0e,
  .priority = DMARequestPriority_Medium,
  .type = DMARequestType_MemoryToPeripheral,
  .data_size = DMARequestDataSize_Byte,
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
    .gpio = GPIOD,
    .gpio_pin = GPIO_Pin_8,
    .gpio_pin_source = GPIO_PinSource8,
    .gpio_af = GPIO_AF7_USART3
  },
  .rx_gpio = {
    .gpio = GPIOD,
    .gpio_pin = GPIO_Pin_9,
    .gpio_pin_source = GPIO_PinSource9,
    .gpio_af = GPIO_AF7_USART3
  },
  .periph = USART3,
  .irq_channel = USART3_IRQn,
  .irq_priority = 13,
  .rcc_apb_periph = RCC_APB1Periph_USART3,
  .rx_dma = &DBG_UART_RX_DMA_REQUEST
};
UARTDevice * const DBG_UART = &DBG_UART_DEVICE;
IRQ_MAP(USART3, uart_irq_handler, DBG_UART);

static UARTDeviceState s_accessory_uart_state;
static UARTDevice ACCESSORY_UART_DEVICE = {
  .state = &s_accessory_uart_state,
  .half_duplex = true,
  .tx_gpio = {
#if BOARD_ROBERT_BB || BOARD_CUTTS_BB
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_12,
    .gpio_pin_source = GPIO_PinSource12,
    .gpio_af = GPIO_AF6_UART4
#elif BOARD_ROBERT_BB2 || BOARD_ROBERT_EVT
    .gpio = GPIOD,
    .gpio_pin = GPIO_Pin_1,
    .gpio_pin_source = GPIO_PinSource1,
    .gpio_af = GPIO_AF8_UART4
#else
#error "Unknown board"
#endif
  },
  .periph = UART4,
  .irq_channel = UART4_IRQn,
  .irq_priority = 0xb,
  .rcc_apb_periph = RCC_APB1Periph_UART4,
  .rx_dma = &ACCESSORY_UART_RX_DMA_REQUEST
};
UARTDevice * const ACCESSORY_UART = &ACCESSORY_UART_DEVICE;
IRQ_MAP(UART4, uart_irq_handler, ACCESSORY_UART);

static UARTDeviceState s_bt_bootrom_uart_state;
static UARTDevice BT_BOOTROM_UART_DEVICE = {
  .state = &s_bt_bootrom_uart_state,
#if BOARD_ROBERT_BB || BOARD_CUTTS_BB
  .do_swap_rx_tx = true,
#elif BOARD_ROBERT_BB2 || BOARD_ROBERT_EVT
  .do_swap_rx_tx = false,
#else
#error "Unknown board"
#endif
  .rx_gpio = { GPIOE, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF8_UART8 },
  .tx_gpio = { GPIOE, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF8_UART8 },
  .rcc_apb_periph = RCC_APB1Periph_UART8,
  .periph = UART8,
};

UARTDevice * const BT_TX_BOOTROM_UART = &BT_BOOTROM_UART_DEVICE;
UARTDevice * const BT_RX_BOOTROM_UART = &BT_BOOTROM_UART_DEVICE;

// I2C DEVICES

#if BOARD_CUTTS_BB
static I2CBusState I2C_TOUCH_ALS_BUS_STATE = {};

static const I2CBusHal I2C_TOUCH_ALS_BUS_HAL = {
  .i2c = I2C1,
  .clock_ctrl = RCC_APB1Periph_I2C1,
  .bus_mode = I2CBusMode_FastMode,
  .clock_speed = 400000,
  // TODO: These need to be measured. Just using PMIC_MAG values for now.
  .rise_time_ns = 150,
  .fall_time_ns = 6,
  .ev_irq_channel = I2C1_EV_IRQn,
  .er_irq_channel = I2C1_ER_IRQn,
};

static const I2CBus I2C_TOUCH_ALS_BUS = {
  .state = &I2C_TOUCH_ALS_BUS_STATE,
  .hal = &I2C_TOUCH_ALS_BUS_HAL,
  .scl_gpio = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_6,
    .gpio_pin_source = GPIO_PinSource6,
    .gpio_af = GPIO_AF4_I2C1
  },
  .sda_gpio = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_9,
    .gpio_pin_source = GPIO_PinSource9,
    .gpio_af = GPIO_AF4_I2C1
  },
  .stop_mode_inhibitor = InhibitorI2C1,
  .name = "I2C_TOUCH_ALS"
};
#endif

static I2CBusState I2C_HRM_BUS_STATE = {};

static const I2CBusHal I2C_HRM_BUS_HAL = {
  .i2c = I2C2,
  .clock_ctrl = RCC_APB1Periph_I2C2,
  .bus_mode = I2CBusMode_FastMode,
  .clock_speed = 400000,
#if BOARD_ROBERT_BB || BOARD_CUTTS_BB
  // TODO: These need to be measured. Just using PMIC_MAG values for now.
  .rise_time_ns = 150,
  .fall_time_ns = 6,
#elif BOARD_ROBERT_BB2
  // TODO: These need to be measured. Just using PMIC_MAG values for now.
  .rise_time_ns = 150,
  .fall_time_ns = 6,
#elif BOARD_ROBERT_EVT
  // TODO: These need to be measured. Just using PMIC_MAG values for now.
  .rise_time_ns = 70,
  .fall_time_ns = 5,
#else
#error "Unknown board"
#endif
  .ev_irq_channel = I2C2_EV_IRQn,
  .er_irq_channel = I2C2_ER_IRQn,
};

static const I2CBus I2C_HRM_BUS = {
  .state = &I2C_HRM_BUS_STATE,
  .hal = &I2C_HRM_BUS_HAL,
  .scl_gpio = {
    .gpio = GPIOF,
    .gpio_pin = GPIO_Pin_1,
    .gpio_pin_source = GPIO_PinSource1,
    .gpio_af = GPIO_AF4_I2C2
  },
  .sda_gpio = {
    .gpio = GPIOF,
    .gpio_pin = GPIO_Pin_0,
    .gpio_pin_source = GPIO_PinSource0,
    .gpio_af = GPIO_AF4_I2C2
  },
  .stop_mode_inhibitor = InhibitorI2C2,
  .name = "I2C_HRM"
};

#if BOARD_CUTTS_BB
static I2CBusState I2C_NFC_BUS_STATE = {};

static const I2CBusHal I2C_NFC_BUS_HAL = {
  .i2c = I2C3,
  .clock_ctrl = RCC_APB1Periph_I2C3,
  .bus_mode = I2CBusMode_FastMode,
  .clock_speed = 400000,
  // TODO: These need to be measured. Just using PMIC_MAG values for now.
  .rise_time_ns = 150,
  .fall_time_ns = 6,
  .ev_irq_channel = I2C3_EV_IRQn,
  .er_irq_channel = I2C3_ER_IRQn,
};

static const I2CBus I2C_NFC_BUS = {
  .state = &I2C_NFC_BUS_STATE,
  .hal = &I2C_NFC_BUS_HAL,
  .scl_gpio = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_8,
    .gpio_pin_source = GPIO_PinSource8,
    .gpio_af = GPIO_AF4_I2C3
  },
  .sda_gpio = {
    .gpio = GPIOH,
    .gpio_pin = GPIO_Pin_8,
    .gpio_pin_source = GPIO_PinSource8,
    .gpio_af = GPIO_AF4_I2C3
  },
  .stop_mode_inhibitor = InhibitorI2C3,
  .name = "I2C_NFC"
};
#endif

static I2CBusState I2C_PMIC_MAG_BUS_STATE = {};
static const I2CBusHal I2C_PMIC_MAG_BUS_HAL = {
  .i2c = I2C4,
  .clock_ctrl = RCC_APB1Periph_I2C4,
  .bus_mode = I2CBusMode_FastMode,
  .clock_speed = 400000,
#if BOARD_ROBERT_BB || BOARD_CUTTS_BB
  // These rise and fall times were measured.
  .rise_time_ns = 150,
  .fall_time_ns = 6,
#elif BOARD_ROBERT_BB2
  // TODO: These rise and fall times are based on robert_bb and should be measured
  .rise_time_ns = 150,
  .fall_time_ns = 6,
#elif BOARD_ROBERT_EVT
  // TODO: These are calculated and could potentially be measured.
  .rise_time_ns = 70,
  .fall_time_ns = 5,
#else
#error "Unknown board"
#endif
  .ev_irq_channel = I2C4_EV_IRQn,
  .er_irq_channel = I2C4_ER_IRQn,
};

static const I2CBus I2C_PMIC_MAG_BUS = {
  .state = &I2C_PMIC_MAG_BUS_STATE,
  .hal = &I2C_PMIC_MAG_BUS_HAL,
  .scl_gpio = {
    .gpio = GPIOF,
    .gpio_pin = GPIO_Pin_14,
    .gpio_pin_source = GPIO_PinSource14,
    .gpio_af = GPIO_AF4_I2C4
  },
  .sda_gpio = {
    .gpio = GPIOF,
    .gpio_pin = GPIO_Pin_15,
    .gpio_pin_source = GPIO_PinSource15,
    .gpio_af = GPIO_AF4_I2C4
  },
  .stop_mode_inhibitor = InhibitorI2C4,
  .name = "I2C_PMIC_MAG"
};

#if BOARD_CUTTS_BB
static const I2CSlavePort I2C_SLAVE_EWD1000 = {
  .bus = &I2C_TOUCH_ALS_BUS,
  .address = 0x2A
};
#endif

static const I2CSlavePort I2C_SLAVE_MAX14690 = {
  .bus = &I2C_PMIC_MAG_BUS,
  .address = 0x50
};

static const I2CSlavePort I2C_SLAVE_MAG3110 = {
  .bus = &I2C_PMIC_MAG_BUS,
  .address = 0x0e << 1
};

static const I2CSlavePort I2C_SLAVE_AS7000 = {
  .bus = &I2C_HRM_BUS,
  .address = 0x60
};

I2CSlavePort * const I2C_MAX14690 = &I2C_SLAVE_MAX14690;
I2CSlavePort * const I2C_MAG3110 = &I2C_SLAVE_MAG3110;
I2CSlavePort * const I2C_AS7000 = &I2C_SLAVE_AS7000;

IRQ_MAP(I2C2_EV, i2c_hal_event_irq_handler, &I2C_HRM_BUS);
IRQ_MAP(I2C2_ER, i2c_hal_error_irq_handler, &I2C_HRM_BUS);
IRQ_MAP(I2C4_EV, i2c_hal_event_irq_handler, &I2C_PMIC_MAG_BUS);
IRQ_MAP(I2C4_ER, i2c_hal_error_irq_handler, &I2C_PMIC_MAG_BUS);
#if BOARD_CUTTS_BB
IRQ_MAP(I2C1_EV, i2c_hal_event_irq_handler, &I2C_TOUCH_ALS_BUS);
IRQ_MAP(I2C1_ER, i2c_hal_error_irq_handler, &I2C_TOUCH_ALS_BUS);
#endif


// HRM DEVICE
static HRMDeviceState s_hrm_state;
static HRMDevice HRM_DEVICE = {
  .state = &s_hrm_state,
  .handshake_int = { EXTI_PortSourceGPIOI, 10 },
  .int_gpio = {
    .gpio = GPIOI,
    .gpio_pin = GPIO_Pin_10
  },
  .en_gpio = {
    .gpio = GPIOF,
    .gpio_pin = GPIO_Pin_3,
    .active_high = false,
  },
  .i2c_slave = &I2C_SLAVE_AS7000,
};
HRMDevice * const HRM = &HRM_DEVICE;

#if BOARD_CUTTS_BB
static const TouchSensor EWD1000_DEVICE = {
  .i2c = &I2C_SLAVE_EWD1000,
  .int_gpio = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_7,
  },
  .int_exti = {
    .exti_port_source = EXTI_PortSourceGPIOB,
    .exti_line = 7,
  },
  .reset_gpio = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_5,
    .active_high = true,
  },
};

TouchSensor * const EWD1000 = &EWD1000_DEVICE;
#endif

// VOLTAGE MONITOR DEVICES

static const VoltageMonitorDevice VOLTAGE_MONITOR_ALS_DEVICE = {
  .adc = ADC3,
  .adc_channel = ADC_Channel_14,
  .clock_ctrl = RCC_APB2Periph_ADC3,
  .input = {
    .gpio = GPIOF,
    .gpio_pin = GPIO_Pin_4,
  },
};

static const VoltageMonitorDevice VOLTAGE_MONITOR_BATTERY_DEVICE = {
  .adc = ADC1,
  .adc_channel = ADC_Channel_9,
  .clock_ctrl = RCC_APB2Periph_ADC1,
  .input = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_1,
  }
};

static const VoltageMonitorDevice VOLTAGE_MONITOR_TEMPERATURE_DEVICE = {
  .adc = ADC1,
  .adc_channel = ADC_Channel_TempSensor,
  .clock_ctrl = RCC_APB2Periph_ADC1,
  // .input not applicable
};

VoltageMonitorDevice * const VOLTAGE_MONITOR_ALS = &VOLTAGE_MONITOR_ALS_DEVICE;
VoltageMonitorDevice * const VOLTAGE_MONITOR_BATTERY = &VOLTAGE_MONITOR_BATTERY_DEVICE;
VoltageMonitorDevice * const VOLTAGE_MONITOR_TEMPERATURE = &VOLTAGE_MONITOR_TEMPERATURE_DEVICE;


// Temperature sensor

const AnalogTemperatureSensor TEMPERATURE_SENSOR_DEVICE = {
  .voltage_monitor = &VOLTAGE_MONITOR_TEMPERATURE_DEVICE,
  .millivolts_ref = 760,
  .millidegrees_ref = 25000,
  .slope_numerator = 5,
  .slope_denominator = 2000,
};
const AnalogTemperatureSensor * const TEMPERATURE_SENSOR = &TEMPERATURE_SENSOR_DEVICE;


//
// SPI Bus configuration
//

static SPIBusState DIALOG_SPI_BUS_STATE = { };
static const SPIBus DIALOG_SPI_BUS = {
  .state = &DIALOG_SPI_BUS_STATE,
  .spi = SPI4,
  .spi_sclk =  { GPIOE, GPIO_Pin_12, GPIO_PinSource12, GPIO_AF5_SPI5 },
  .spi_miso = { GPIOE, GPIO_Pin_13, GPIO_PinSource13, GPIO_AF5_SPI5 },
  .spi_mosi = { GPIOE, GPIO_Pin_14, GPIO_PinSource14, GPIO_AF5_SPI5 },
  .spi_sclk_speed = GPIO_Speed_50MHz,
  // DA14680_FS v1.4 page 89:
  // "In slave mode the internal SPI clock must be more than four times the SPIx_CLK"
  // The system clock is 16MHz, so don't use more than 4MHz.
  .spi_clock_speed_hz = MHZ_TO_HZ(4)
};

static SPIBusState BMI160_SPI_BUS_STATE = {};
static const SPIBus BMI160_SPI_BUS = {
  .state = &BMI160_SPI_BUS_STATE,
  .spi = SPI2,
  .spi_sclk = {
    .gpio = GPIOI,
    .gpio_pin = GPIO_Pin_1,
    .gpio_pin_source = GPIO_PinSource1,
    .gpio_af = GPIO_AF5_SPI2
  },
  .spi_miso = {
    .gpio = GPIOI,
    .gpio_pin = GPIO_Pin_2,
    .gpio_pin_source = GPIO_PinSource2,
    .gpio_af = GPIO_AF5_SPI2
  },
  .spi_mosi = {
    .gpio = GPIOI,
    .gpio_pin = GPIO_Pin_3,
    .gpio_pin_source = GPIO_PinSource3,
    .gpio_af = GPIO_AF5_SPI2
  },
  .spi_sclk_speed = GPIO_Speed_25MHz,
  .spi_clock_speed_hz = MHZ_TO_HZ(5),
};

static SPIBusState ICE40LP_SPI_BUS_STATE = {};
static const SPIBus ICE40LP_SPI_BUS = {
  .state = &ICE40LP_SPI_BUS_STATE,
  .spi = SPI6,
  .spi_sclk = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_5,
    .gpio_pin_source = GPIO_PinSource5,
    .gpio_af = GPIO_AF8_SPI6
  },
  .spi_miso = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_6,
    .gpio_pin_source = GPIO_PinSource6,
    .gpio_af = GPIO_AF8_SPI6
  },
  .spi_mosi = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_7,
    .gpio_pin_source = GPIO_PinSource7,
    .gpio_af = GPIO_AF8_SPI6
  },
  .spi_sclk_speed = GPIO_Speed_25MHz,
  .spi_clock_speed_hz = MHZ_TO_HZ(16),
};

//
// SPI Slave port configuration
//

static SPISlavePortState BMI160_SPI_SLAVE_PORT_STATE = {};
static SPISlavePort BMI160_SPI_SLAVE_PORT = {
  .slave_state = &BMI160_SPI_SLAVE_PORT_STATE,
  .spi_bus = &BMI160_SPI_BUS,
  .spi_scs  = {
    .gpio = GPIOI,
    .gpio_pin = GPIO_Pin_0,
    .active_high = false
  },
  .spi_direction = SpiDirection_2LinesFullDuplex,
  .spi_cpol = SpiCPol_Low,
  .spi_cpha = SpiCPha_1Edge,
  .spi_first_bit = SpiFirstBit_MSB,
};
SPISlavePort * const BMI160_SPI = &BMI160_SPI_SLAVE_PORT;

static SPISlavePortState ICE40LP_SPI_SLAVE_PORT_STATE = {};
static SPISlavePort ICE40LP_SPI_SLAVE_PORT = {
  .slave_state = &ICE40LP_SPI_SLAVE_PORT_STATE,
  .spi_bus = &ICE40LP_SPI_BUS,
  .spi_scs  = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_4,
    .active_high = false
  },
  .spi_direction = SpiDirection_1LineTx,
  .spi_cpol = SpiCPol_High,
  .spi_cpha = SpiCPha_2Edge,
  .spi_first_bit = SpiFirstBit_MSB,
  .tx_dma = &ICE40LP_SPI_TX_DMA_REQUEST
};

static SPISlavePortState DIALOG_SPI_SLAVE_PORT_STATE = {};
static SPISlavePort DIALOG_SPI_SLAVE_PORT = {
  .slave_state = &DIALOG_SPI_SLAVE_PORT_STATE,
  .spi_bus = &DIALOG_SPI_BUS,
  .spi_scs = { GPIOE, GPIO_Pin_11, false },
  .spi_direction = SpiDirection_2LinesFullDuplex,
  .spi_cpol = SpiCPol_Low,
  .spi_cpha = SpiCPha_1Edge,
  .spi_first_bit = SpiFirstBit_MSB,
  .rx_dma = &DIALOG_SPI_RX_DMA_REQUEST,
  .tx_dma = &DIALOG_SPI_TX_DMA_REQUEST
};
SPISlavePort * const DIALOG_SPI = &DIALOG_SPI_SLAVE_PORT;



//
// iCE40LP configuration
//
static ICE40LPDeviceState s_ice40lp_state;
static ICE40LPDevice ICE40LP_DEVICE = {
  .state = &s_ice40lp_state,

  .spi_port = &ICE40LP_SPI_SLAVE_PORT,
  .base_spi_frequency = MHZ_TO_HZ(16),
  .fast_spi_frequency = MHZ_TO_HZ(32),
  .creset = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_3,
    .active_high = true,
  },
  .cdone = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_2,
  },
  .busy = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_0,
  },
  .cdone_exti = {
    .exti_port_source = EXTI_PortSourceGPIOB,
    .exti_line = 2,
  },
  .busy_exti = {
    .exti_port_source = EXTI_PortSourceGPIOB,
    .exti_line = 0,
  },
#if BOARD_CUTTS_BB
  .use_6v6_rail = true,
#elif BOARD_ROBERT_BB || BOARD_ROBERT_BB2 || BOARD_ROBERT_EVT
  .use_6v6_rail = false,
#else
#error "Unknown board"
#endif
};
ICE40LPDevice * const ICE40LP = &ICE40LP_DEVICE;


// QSPI
static QSPIPortState s_qspi_port_state;
static QSPIPort QSPI_PORT = {
  .state = &s_qspi_port_state,
  .clock_speed_hz = MHZ_TO_HZ(72),
  .auto_polling_interval = 16,
  .clock_ctrl = RCC_AHB3Periph_QSPI,
  .cs_gpio = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_10,
    .gpio_pin_source = GPIO_PinSource10,
    .gpio_af = GPIO_AF9_QUADSPI,
  },
  .clk_gpio = {
    .gpio = GPIOF,
    .gpio_pin = GPIO_Pin_10,
    .gpio_pin_source = GPIO_PinSource10,
    .gpio_af = GPIO_AF9_QUADSPI,
  },
  .data_gpio = {
    {
      .gpio = GPIOD,
      .gpio_pin = GPIO_Pin_11,
      .gpio_pin_source = GPIO_PinSource11,
      .gpio_af = GPIO_AF9_QUADSPI,
    },
    {
      .gpio = GPIOC,
      .gpio_pin = GPIO_Pin_10,
      .gpio_pin_source = GPIO_PinSource10,
      .gpio_af = GPIO_AF9_QUADSPI,
    },
    {
#if BOARD_ROBERT_BB || BOARD_ROBERT_BB2 || BOARD_CUTTS_BB
      .gpio = GPIOF,
      .gpio_pin = GPIO_Pin_7,
      .gpio_pin_source = GPIO_PinSource7,
      .gpio_af = GPIO_AF9_QUADSPI,
#elif BOARD_ROBERT_EVT
      .gpio = GPIOE,
      .gpio_pin = GPIO_Pin_2,
      .gpio_pin_source = GPIO_PinSource2,
      .gpio_af = GPIO_AF9_QUADSPI,
#else
#error "Unknown board"
#endif
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
  .default_fast_read_ddr_enabled = true,
#if BOARD_ROBERT_BB || BOARD_ROBERT_BB2 || BOARD_CUTTS_BB
  .reset_gpio = { GPIO_Port_NULL },
#elif BOARD_ROBERT_EVT
  .reset_gpio = {
    .gpio = GPIOE,
    .gpio_pin = GPIO_Pin_15,
    .active_high = false,
  },
#else
#error "Unknown error"
#endif
};
QSPIFlash * const QSPI_FLASH = &QSPI_FLASH_DEVICE;


void board_early_init(void) {
  spi_slave_port_init(ICE40LP->spi_port);
}

void board_init(void) {
#if BOARD_CUTTS_BB
  i2c_init(&I2C_TOUCH_ALS_BUS);
  i2c_init(&I2C_NFC_BUS);
#endif
  i2c_init(&I2C_HRM_BUS);
  i2c_init(&I2C_PMIC_MAG_BUS);
  spi_slave_port_init(BMI160_SPI);

  voltage_monitor_device_init(VOLTAGE_MONITOR_ALS);
  voltage_monitor_device_init(VOLTAGE_MONITOR_BATTERY);

  qspi_init(QSPI, BOARD_NOR_FLASH_SIZE);
}
