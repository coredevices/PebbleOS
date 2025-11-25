/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"

#include "drivers/i2c_definitions.h"
#include "drivers/stm32f2/dma_definitions.h"
#include "drivers/stm32f2/i2c_hal_definitions.h"
#include "drivers/stm32f2/uart_definitions.h"
#include "drivers/voltage_monitor.h"

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
CREATE_DMA_STREAM(1, 4); // DMA1_STREAM4_DEVICE - Sharp SPI TX

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

static DMARequestState s_sharp_spi_tx_dma_request_state;
static DMARequest SHARP_SPI_TX_DMA_REQUEST = {
  .state = &s_sharp_spi_tx_dma_request_state,
  .stream = &DMA1_STREAM4_DEVICE,
  .channel = 0,
  .irq_priority = 0x0f,
  .priority = DMARequestPriority_VeryHigh,
  .type = DMARequestType_MemoryToPeripheral,
  .data_size = DMARequestDataSize_Byte,
};
DMARequest * const SHARP_SPI_TX_DMA = &SHARP_SPI_TX_DMA_REQUEST;


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


// I2C DEVICES

static I2CBusState I2C_MAIN_BUS_STATE = {};

static const I2CBusHal I2C_MAIN_BUS_HAL = {
  .i2c = I2C1,
  .clock_ctrl = RCC_APB1Periph_I2C1,
  .clock_speed = 400000,
  .duty_cycle = I2CDutyCycle_16_9,
  .ev_irq_channel = I2C1_EV_IRQn,
  .er_irq_channel = I2C1_ER_IRQn,
};

static const I2CBus I2C_MAIN_BUS = {
  .state = &I2C_MAIN_BUS_STATE,
  .hal = &I2C_MAIN_BUS_HAL,
  .scl_gpio = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_8,
    .gpio_pin_source = GPIO_PinSource8,
    .gpio_af = GPIO_AF_I2C1
  },
  .sda_gpio = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_7,
    .gpio_pin_source = GPIO_PinSource7,
    .gpio_af = GPIO_AF_I2C1
  },
  .stop_mode_inhibitor = InhibitorI2C1,
  .name = "I2C_MAIN"
};

extern void i2c_rail_ctl_pin(I2CBus *device, bool enable);

static I2CBusState I2C_2V5_BUS_STATE = {};

static const I2CBusHal I2C_2V5_BUS_HAL = {
  .i2c = I2C2,
  .clock_ctrl = RCC_APB1Periph_I2C2,
  .clock_speed = 400000,
  .duty_cycle = I2CDutyCycle_2,
  .ev_irq_channel = I2C2_EV_IRQn,
  .er_irq_channel = I2C2_ER_IRQn,
};

static const I2CBus I2C_2V5_BUS = {
  .state = &I2C_2V5_BUS_STATE,
  .hal = &I2C_2V5_BUS_HAL,
  .scl_gpio = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_10,
    .gpio_pin_source = GPIO_PinSource10,
    .gpio_af = GPIO_AF_I2C2
  },
  .sda_gpio = {
    .gpio = GPIOB,
    .gpio_pin = GPIO_Pin_11,
    .gpio_pin_source = GPIO_PinSource11,
    .gpio_af = GPIO_AF_I2C2
  },
  .stop_mode_inhibitor = InhibitorI2C2,
  .rail_gpio = {
    .gpio = GPIOH,
    .gpio_pin = GPIO_Pin_0,
    .active_high = true
  },
  .rail_ctl_fn = i2c_rail_ctl_pin,
  .name = "I2C_2V5"
};

static const I2CSlavePort I2C_SLAVE_LIS3DH = {
  .bus = &I2C_MAIN_BUS,
  .address = 0x32
};

static const I2CSlavePort I2C_SLAVE_MFI = {
  .bus = &I2C_2V5_BUS,
  .address = 0x20
};

static const I2CSlavePort I2C_SLAVE_MAG3110 = {
  .bus = &I2C_2V5_BUS,
  .address = 0x1C
};

I2CSlavePort * const I2C_LIS3DH = &I2C_SLAVE_LIS3DH;
I2CSlavePort * const I2C_MFI = &I2C_SLAVE_MFI;
I2CSlavePort * const I2C_MAG3110 = &I2C_SLAVE_MAG3110;

IRQ_MAP(I2C1_EV, i2c_hal_event_irq_handler, &I2C_MAIN_BUS);
IRQ_MAP(I2C1_ER, i2c_hal_error_irq_handler, &I2C_MAIN_BUS);
IRQ_MAP(I2C2_EV, i2c_hal_event_irq_handler, &I2C_2V5_BUS);
IRQ_MAP(I2C2_ER, i2c_hal_error_irq_handler, &I2C_2V5_BUS);


// VOLTAGE MONITOR DEVICES
static const VoltageMonitorDevice VOLTAGE_MONITOR_ALS_DEVICE = {
  .adc = ADC2,
  .adc_channel = ADC_Channel_12,
  .clock_ctrl = RCC_APB2Periph_ADC2,
  .input = {
    .gpio = GPIOC,
    .gpio_pin = GPIO_Pin_2,
  },
};

static const VoltageMonitorDevice VOLTAGE_MONITOR_BATTERY_DEVICE = {
  .adc = ADC2,
  .adc_channel = ADC_Channel_10,
  .clock_ctrl = RCC_APB2Periph_ADC2,
  .input = {
    .gpio = GPIOC,
    .gpio_pin = GPIO_Pin_0,
  },
};

VoltageMonitorDevice * const VOLTAGE_MONITOR_ALS = &VOLTAGE_MONITOR_ALS_DEVICE;
VoltageMonitorDevice * const VOLTAGE_MONITOR_BATTERY = &VOLTAGE_MONITOR_BATTERY_DEVICE;

void board_early_init(void) {
}

void board_init(void) {
  i2c_init(&I2C_MAIN_BUS);
  i2c_init(&I2C_2V5_BUS);

  voltage_monitor_device_init(VOLTAGE_MONITOR_ALS);
  voltage_monitor_device_init(VOLTAGE_MONITOR_BATTERY);
}
