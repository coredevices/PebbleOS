/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*FakeI2CWriteHook)(uint8_t reg, uint8_t value, void *context);

void fake_i2c_init(void);

void fake_i2c_set_register(uint8_t reg, uint8_t value);

void fake_i2c_set_register_block(uint8_t reg, const uint8_t *data, uint32_t len);

uint8_t fake_i2c_get_register(uint8_t reg);

void fake_i2c_set_fail(bool fail);

void fake_i2c_set_write_hook(FakeI2CWriteHook hook, void *context);

bool fake_i2c_was_written(uint8_t reg, uint8_t value);

uint8_t fake_i2c_last_written(uint8_t reg);
