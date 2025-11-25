/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "board/board.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "drivers/spi.h"

//! Ask the chip to accept input from the SPI bus. Required after issuing a soft reset.
void bmi160_enable_spi_mode(void);
void bmi160_begin_burst(uint8_t addr);
void bmi160_end_burst(void);

uint8_t bmi160_read_reg(uint8_t reg);
uint16_t bmi160_read_16bit_reg(uint8_t reg);
void bmi160_write_reg(uint8_t reg, uint8_t value);
