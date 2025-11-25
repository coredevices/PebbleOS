/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/gpio.h"

#include <stdbool.h>

// GPIO constants
#define DISP_SPI  SPI6
#define DISP_GPIO GPIOG

#define DISPLAY_SPI_CLOCK_PERIPH SpiPeriphClockAPB2
#define DISPLAY_SPI_CLOCK RCC_APB2Periph_SPI6
#define DISP_PIN_SCS      GPIO_Pin_8
#define DISP_PIN_CDONE    GPIO_Pin_9
#define DISP_PIN_BUSY     GPIO_Pin_10
#define DISP_PIN_SO       GPIO_Pin_12
#define DISP_PIN_SCLK     GPIO_Pin_13
#define DISP_PIN_SI       GPIO_Pin_14
#define DISP_PIN_CRESET   GPIO_Pin_15

#define GPIO_PINSOURCE_SCK  GPIO_PinSource13
#define GPIO_PINSOURCE_MOSI GPIO_PinSource14
#define GPIO_PINSOURCE_MISO GPIO_PinSource12


bool display_busy(void);
void display_start(void);
bool display_program(const uint8_t *fpga_bitstream, uint32_t bitstream_size);
void display_write_byte(uint8_t d);
uint8_t display_write_and_read_byte(uint8_t d);
void display_power_enable(void);
void display_power_disable(void);
