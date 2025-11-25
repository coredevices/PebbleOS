/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"

#include "drivers/i2c/i2c_definitions.h"
#include "drivers/i2c/i2c_hal_definitions.h"
#include "drivers/display/ice40lp_definitions.h"
#include "util/misc.h"

//
// iCE40LP configuration
//

static ICE40LPDevice ICE40LP_DEVICE = {
  .spi = {
    .periph = SPI6,
    .rcc_bit = RCC_APB2Periph_SPI6,
    .clk = {
      .gpio = GPIOA,
      .gpio_pin = GPIO_Pin_5,
      .gpio_pin_source = GPIO_PinSource5,
      .gpio_af = GPIO_AF8_SPI6
    },
    .mosi = {
      .gpio = GPIOA,
      .gpio_pin = GPIO_Pin_7,
      .gpio_pin_source = GPIO_PinSource7,
      .gpio_af = GPIO_AF8_SPI6
    },
    .scs  = {
      .gpio = GPIOA,
      .gpio_pin = GPIO_Pin_4,
      .active_high = false
    }
  },

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

  .use_6v6_rail = false,
};

ICE40LPDevice * const ICE40LP = &ICE40LP_DEVICE;


// I2C DEVICES

static I2CBusState I2C_PMIC_MAG_BUS_STATE = {};
static const I2CBusHal I2C_PMIC_MAG_BUS_HAL = {
  .i2c = I2C4,
  .clock_ctrl = RCC_APB1Periph_I2C4,
  .clock_speed = 400000,
  .duty_cycle = I2CDutyCycle_16_9,
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
  .name = "I2C_PMIC_MAG"
};

static const I2CSlavePort I2C_SLAVE_MAX14690 = {
  .bus = &I2C_PMIC_MAG_BUS,
  .address = 0x50
};

I2CSlavePort * const I2C_MAX14690 = &I2C_SLAVE_MAX14690;

IRQ_MAP(I2C4_EV, i2c_hal_event_irq_handler, &I2C_PMIC_MAG_BUS);
IRQ_MAP(I2C4_ER, i2c_hal_error_irq_handler, &I2C_PMIC_MAG_BUS);


void board_init(void) {
  i2c_init(&I2C_PMIC_MAG_BUS);
}
