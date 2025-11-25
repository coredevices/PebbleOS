/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "board/board.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "drivers/spi.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#include <mcu.h>

bool bma255_selftest(void);

void bma255_gpio_init(void);

void bma255_enable_spi_clock(void);

void bma255_disable_spi_clock(void);

uint8_t bma255_send_and_receive_byte(uint8_t byte);

void bma255_send_byte(uint8_t byte);

void bma255_prepare_txn(uint8_t address);

void bma255_end_txn(void);

void bma255_burst_read(uint8_t address, uint8_t *data, size_t len);

uint8_t bma255_read_register(uint8_t address);

void bma255_write_register(uint8_t address, uint8_t data);

void bma255_read_modify_write(uint8_t reg, uint8_t value, uint8_t mask);
