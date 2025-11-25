/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

bool display_busy(void);
void display_start(void);
bool display_program(const uint8_t *fpga_bitstream, uint32_t bitstream_size);
void display_write_cmd(uint8_t cmd, uint8_t *arg, uint32_t arg_len);
void display_power_enable(void);
void display_power_disable(void);
