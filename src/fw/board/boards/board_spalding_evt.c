/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"

#include "drivers/display/ice40lp/ice40lp_definitions.h"
#include "drivers/i2c_definitions.h"
#include "drivers/stm32f2/dma_definitions.h"
#include "drivers/stm32f2/i2c_hal_definitions.h"
#include "drivers/stm32f2/spi_definitions.h"
#include "drivers/stm32f2/uart_definitions.h"
#include "drivers/temperature/analog.h"
#include "drivers/voltage_monitor.h"
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

CREATE_DMA_STREAM(1, 1); // DMA1_STREAM1_DEVICE - Debug UART RX
CREATE_DMA_STREAM(1, 3); // DMA1_STREAM3_DEVICE - Mic I2S RX
CREATE_DMA_STREAM(1, 6); // DMA1_STREAM6_DEVICE - Accessory UART RX
CREATE_DMA_STREAM(2, 0); // DMA2_STREAM0_DEVICE - Compositor DMA
CREATE_DMA_STREAM(2, 5); // DMA2_STREAM5_DEVICE - ICE40LP TX
CREATE_DMA_STREAM(2, 2); // DMA2_STREAM4_DEVICE - Bluetooth UART RX

// DMA Requests

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

static DMARequestState s_mic_i2s_rx_dma_request_state;
static DMARequest MIC_I2S_RX_DMA_REQUEST = {
  .state = &s_mic_i2s_rx_dma_request_state,
  .stream = &DMA1_STREAM3_DEVICE,
  .channel = 0,
  .irq_priority = 0x0f,
  .priority = DMARequestPriority_High,
  .type = DMARequestType_PeripheralToMemory,
  .data_size = DMARequestDataSize_HalfWord,
};
DMARequest * const MIC_I2S_RX_DMA = &MIC_I2S_RX_DMA_REQUEST;

static DMARequestState s_accessory_uart_dma_request_state;
static DMARequest ACCESSORY_UART_RX_DMA_REQUEST = {
  .state = &s_accessory_uart_dma_request_state,
  .stream = &DMA1_STREAM6_DEVICE,
  .channel = 5,
  .irq_priority = IRQ_PRIORITY_INVALID, // no interrupts
  .priority = DMARequestPriority_High,
  .type = DMARequestType_PeripheralToMemory,
  .data_size = DMARequestDataSize_Byte,
};

static DMARequestState s_compositor_dma_request_state;
static DMARequest COMPOSITOR_DMA_REQUEST = {
  .state = &s_compositor_dma_request_state,
  .stream = &DMA2_STREAM0_DEVICE,
  .channel = 0,
  .irq_priority = 11,
  .priority = DMARequestPriority_High,
  .type = DMARequestType_MemoryToMemory,
  .data_size = DMARequestDataSize_Byte,
};
DMARequest * const COMPOSITOR_DMA = &COMPOSITOR_DMA_REQUEST;

static DMARequestState s_ice40lp_spi_tx_dma_request_state;
static DMARequest ICE40LP_SPI_TX_DMA_REQUEST = {
  .state = &s_ice40lp_spi_tx_dma_request_state,
  .stream = &DMA2_STREAM5_DEVICE,
  .channel = 1,
  // Use the same priority as the EXTI handlers so that the DMA-complete
  // handler doesn't preempt the display BUSY (INTn) handler.
  .irq_priority = 0x0e,
  .priority = DMARequestPriority_VeryHigh,
  .type = DMARequestType_MemoryToPeripheral,
  .data_size = DMARequestDataSize_Byte,
};

static DMARequestState s_bluetooth_uart_rx_dma_request_state;
static DMARequest BLUETOOTH_UART_RX_DMA_REQUEST = {
  .state = &s_bluetooth_uart_rx_dma_request_state,
  .stream = &DMA2_STREAM2_DEVICE,
  .channel = 4,
  .irq_priority = 0x0e,
  .priority = DMARequestPriority_High,
  .type = DMARequestType_PeripheralToMemory,
  .data_size = DMARequestDataSize_Byte,
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
    .gpio = GPIOC,
    .gpio_pin = GPIO_Pin_10,
    .gpio_pin_source = GPIO_PinSource10,
    .gpio_af = GPIO_AF_USART3
  },
  .rx_gpio = {
    .gpio = GPIOC,
    .gpio_pin = GPIO_Pin_11,
    .gpio_pin_source = GPIO_PinSource11,
    .gpio_af = GPIO_AF_USART3
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
    .gpio = GPIOE,
    .gpio_pin = GPIO_Pin_1,
    .gpio_pin_source = GPIO_PinSource1,
    .gpio_af = GPIO_AF_UART8
  },
  .periph = UART8,
  .irq_channel = UART8_IRQn,
  .irq_priority = 0xb,
  .rcc_apb_periph = RCC_APB1Periph_UART8,
  .rx_dma = &ACCESSORY_UART_RX_DMA_REQUEST
};
UARTDevice * const ACCESSORY_UART = &ACCESSORY_UART_DEVICE;
IRQ_MAP(UART8, uart_irq_handler, ACCESSORY_UART);

static UARTDeviceState s_bluetooth_uart_state;
static UARTDevice BLUETOOTH_UART_DEVICE = {
  .state = &s_bluetooth_uart_state,
  .tx_gpio = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_9,
    .gpio_pin_source = GPIO_PinSource9,
    .gpio_af = GPIO_AF_USART1
  },
  .rx_gpio = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_10,
    .gpio_pin_source = GPIO_PinSource10,
    .gpio_af = GPIO_AF_USART1
  },
  .cts_gpio = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_11,
    .gpio_pin_source = GPIO_PinSource11,
    .gpio_af = GPIO_AF_USART1
  },
  .rts_gpio = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_12,
    .gpio_pin_source = GPIO_PinSource12,
    .gpio_af = GPIO_AF_USART1
  },
  .enable_flow_control = true,
  .periph = USART1,
  .irq_channel = USART1_IRQn,
  .irq_priority = 0xe,
  .rcc_apb_periph = RCC_APB2Periph_USART1,
  // .rx_dma = &BLUETOOTH_UART_RX_DMA_REQUEST
};
UARTDevice * const BLUETOOTH_UART = &BLUETOOTH_UART_DEVICE;
IRQ_MAP(USART1, uart_irq_handler, BLUETOOTH_UART);

// I2C DEVICES

static I2CBusState I2C_PMIC_MAG_BUS_STATE = {};

static const I2CBusHal I2C_PMIC_MAG_BUS_HAL = {
  .i2c = I2C1,
  .clock_ctrl = RCC_APB1Periph_I2C1,
  .clock_speed = 400000,
  .duty_cycle = I2CDutyCycle_16_9,
  .ev_irq_channel = I2C1_EV_IRQn,
  .er_irq_channel = I2C1_ER_IRQn,
};

static const I2CBus I2C_PMIC_MAG_BUS = {
  .state = &I2C_PMIC_MAG_BUS_STATE,
  .hal = &I2C_PMIC_MAG_BUS_HAL,
  .scl_gpio = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_6,
    .gpio_pin_source = GPIO_PinSource6,
    .gpio_af = GPIO_AF_I2C1
  },
  .sda_gpio = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_9,
    .gpio_pin_source = GPIO_PinSource9,
    .gpio_af = GPIO_AF_I2C1
  },
  .stop_mode_inhibitor = InhibitorI2C1,
  .name = "I2C_PMIC_MAG"
};

extern void i2c_rail_ctl_pmic(I2CBus *device, bool enable);

static const I2CSlavePort I2C_SLAVE_MAX14690 = {
  .bus = &I2C_PMIC_MAG_BUS,
  .address = 0x50
};

static const I2CSlavePort I2C_SLAVE_MAG3110 = {
  .bus = &I2C_PMIC_MAG_BUS,
  .address = 0x1C
};

I2CSlavePort * const I2C_MAX14690 = &I2C_SLAVE_MAX14690;
I2CSlavePort * const I2C_MAG3110 = &I2C_SLAVE_MAG3110;
I2CSlavePort * const I2C_MFI = NULL;

IRQ_MAP(I2C1_EV, i2c_hal_event_irq_handler, &I2C_PMIC_MAG_BUS);
IRQ_MAP(I2C1_ER, i2c_hal_error_irq_handler, &I2C_PMIC_MAG_BUS);


// VOLTAGE MONITOR DEVICES

static const VoltageMonitorDevice VOLTAGE_MONITOR_ALS_DEVICE = {
  .adc = ADC2,
  .adc_channel = ADC_Channel_2,
  .clock_ctrl = RCC_APB2Periph_ADC2,
  .input = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_2,
  },
};

static const VoltageMonitorDevice VOLTAGE_MONITOR_BATTERY_DEVICE = {
  .adc = ADC2,
  .adc_channel = ADC_Channel_1,
  .clock_ctrl = RCC_APB2Periph_ADC2,
  .input = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_1,
  },
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
// STM32F439 datasheet rev 5
// Section 6.3.22
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

static SPIBusState BMI160_SPI_BUS_STATE = {};
static const SPIBus BMI160_SPI_BUS = {
  .state = &BMI160_SPI_BUS_STATE,
  .spi = SPI1,
  .spi_sclk = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_5,
    .gpio_pin_source = GPIO_PinSource5,
    .gpio_af = GPIO_AF_SPI1
  },
  .spi_miso = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_6,
    .gpio_pin_source = GPIO_PinSource6,
    .gpio_af = GPIO_AF_SPI1
  },
  .spi_mosi = {
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_7,
    .gpio_pin_source = GPIO_PinSource7,
    .gpio_af = GPIO_AF_SPI1
  },
  .spi_sclk_speed = GPIO_Speed_50MHz,
  .spi_clock_speed_hz = MHZ_TO_HZ(5),
};

static SPIBusState ICE40LP_SPI_BUS_STATE = {};
static const SPIBus ICE40LP_SPI_BUS = {
  .state = &ICE40LP_SPI_BUS_STATE,
  .spi = SPI6,
  .spi_sclk = {
    .gpio = GPIOG,
    .gpio_pin = GPIO_Pin_13,
    .gpio_pin_source = GPIO_PinSource13,
    .gpio_af = GPIO_AF_SPI6
  },
  .spi_miso = {
    .gpio = GPIOG,
    .gpio_pin = GPIO_Pin_12,
    .gpio_pin_source = GPIO_PinSource12,
    .gpio_af = GPIO_AF_SPI6
  },
  .spi_mosi = {
    .gpio = GPIOG,
    .gpio_pin = GPIO_Pin_14,
    .gpio_pin_source = GPIO_PinSource14,
    .gpio_af = GPIO_AF_SPI6
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
    .gpio = GPIOA,
    .gpio_pin = GPIO_Pin_4,
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
    .gpio = GPIOG,
    .gpio_pin = GPIO_Pin_8,
    .active_high = false
  },
  .spi_direction = SpiDirection_1LineTx,
  .spi_cpol = SpiCPol_High,
  .spi_cpha = SpiCPha_2Edge,
  .spi_first_bit = SpiFirstBit_MSB,
  .tx_dma = &ICE40LP_SPI_TX_DMA_REQUEST
};

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
    .gpio = GPIOG,
    .gpio_pin = GPIO_Pin_15,
    .active_high = true,
  },
  .cdone = {
    .gpio = GPIOG,
    .gpio_pin = GPIO_Pin_9,
  },
  .busy = {
    .gpio = GPIOG,
    .gpio_pin = GPIO_Pin_10,
  },
  .cdone_exti = {
    .exti_port_source = EXTI_PortSourceGPIOG,
    .exti_line = 9,
  },
  .busy_exti = {
    .exti_port_source = EXTI_PortSourceGPIOG,
    .exti_line = 10,
  },
  .use_6v6_rail = true,
};
ICE40LPDevice * const ICE40LP = &ICE40LP_DEVICE;


void board_early_init(void) {
  spi_slave_port_init(ICE40LP->spi_port);
}

void board_init(void) {
  i2c_init(&I2C_PMIC_MAG_BUS);
  spi_slave_port_init(BMI160_SPI);

  voltage_monitor_device_init(VOLTAGE_MONITOR_ALS);
  voltage_monitor_device_init(VOLTAGE_MONITOR_BATTERY);
}
