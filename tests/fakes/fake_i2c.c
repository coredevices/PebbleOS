/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_i2c.h"
#include "drivers/i2c.h"

#include <string.h>

#define REG_COUNT 256
#define WRITE_LOG_MAX 256

typedef struct {
  uint8_t reg;
  uint8_t value;
} WriteLogEntry;

static uint8_t s_registers[REG_COUNT];
static bool s_fail;
static FakeI2CWriteHook s_write_hook;
static void *s_write_hook_context;
static WriteLogEntry s_write_log[WRITE_LOG_MAX];
static int s_write_log_count;

// Provide the I2C_BMP390 symbol the driver expects
static I2CSlavePort s_fake_slave;
I2CSlavePort *const I2C_BMP390 = &s_fake_slave;

void fake_i2c_init(void) {
  memset(s_registers, 0, sizeof(s_registers));
  s_fail = false;
  s_write_hook = NULL;
  s_write_hook_context = NULL;
  s_write_log_count = 0;
}

void fake_i2c_set_register(uint8_t reg, uint8_t value) {
  s_registers[reg] = value;
}

void fake_i2c_set_register_block(uint8_t reg, const uint8_t *data, uint32_t len) {
  for (uint32_t i = 0; i < len && (reg + i) < REG_COUNT; i++) {
    s_registers[reg + i] = data[i];
  }
}

uint8_t fake_i2c_get_register(uint8_t reg) {
  return s_registers[reg];
}

void fake_i2c_set_fail(bool fail) {
  s_fail = fail;
}

void fake_i2c_set_write_hook(FakeI2CWriteHook hook, void *context) {
  s_write_hook = hook;
  s_write_hook_context = context;
}

bool fake_i2c_was_written(uint8_t reg, uint8_t value) {
  for (int i = 0; i < s_write_log_count; i++) {
    if (s_write_log[i].reg == reg && s_write_log[i].value == value) {
      return true;
    }
  }
  return false;
}

uint8_t fake_i2c_last_written(uint8_t reg) {
  for (int i = s_write_log_count - 1; i >= 0; i--) {
    if (s_write_log[i].reg == reg) {
      return s_write_log[i].value;
    }
  }
  return 0;
}

// I2C API stubs

void i2c_use(I2CSlavePort *slave) {
  (void)slave;
}

void i2c_release(I2CSlavePort *slave) {
  (void)slave;
}

bool i2c_read_register(I2CSlavePort *slave, uint8_t register_address, uint8_t *result) {
  (void)slave;
  if (s_fail) return false;
  *result = s_registers[register_address];
  return true;
}

bool i2c_read_register_block(I2CSlavePort *slave, uint8_t register_address_start,
                             uint32_t read_size, uint8_t *result_buffer) {
  (void)slave;
  if (s_fail) return false;
  for (uint32_t i = 0; i < read_size && (register_address_start + i) < REG_COUNT; i++) {
    result_buffer[i] = s_registers[register_address_start + i];
  }
  return true;
}

bool i2c_write_register(I2CSlavePort *slave, uint8_t register_address, uint8_t value) {
  (void)slave;
  if (s_fail) return false;

  s_registers[register_address] = value;

  if (s_write_log_count < WRITE_LOG_MAX) {
    s_write_log[s_write_log_count].reg = register_address;
    s_write_log[s_write_log_count].value = value;
    s_write_log_count++;
  }

  if (s_write_hook) {
    s_write_hook(register_address, value, s_write_hook_context);
  }

  return true;
}
