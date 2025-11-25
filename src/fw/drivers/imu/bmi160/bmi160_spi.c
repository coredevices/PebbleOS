/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdbool.h>

#include "kernel/util/sleep.h"
#include "drivers/spi.h"
#include "board/board.h"

#include "bmi160_private.h"
#include "bmi160_regs.h"

void bmi160_begin_burst(uint8_t addr) {
  spi_ll_slave_acquire(BMI160_SPI);
  spi_ll_slave_scs_assert(BMI160_SPI);
  spi_ll_slave_read_write(BMI160_SPI, addr);
}

void bmi160_end_burst(void) {
  spi_ll_slave_scs_deassert(BMI160_SPI);
  spi_ll_slave_release(BMI160_SPI);
}

uint8_t bmi160_read_reg(uint8_t reg) {
  uint8_t value;
  // Registers are read when the address MSB=1
  reg |= BMI160_READ_FLAG;
  SPIScatterGather sg_info[2] = {
    {.sg_len = 1, .sg_out = &reg, .sg_in = NULL}, // address
    {.sg_len = 1, .sg_out = NULL, .sg_in = &value} // 8 bit register read
  };
  spi_slave_burst_read_write_scatter(BMI160_SPI, sg_info, ARRAY_LENGTH(sg_info));
  return value;
}

uint16_t bmi160_read_16bit_reg(uint8_t reg) {
  // 16-bit registers are in little-endian format
  uint16_t value;
  reg |= BMI160_READ_FLAG;
  SPIScatterGather sg_info[2] = {
    {.sg_len = 1, .sg_out = &reg, .sg_in = NULL}, // address
    {.sg_len = 2, .sg_out = NULL, .sg_in = &value} // 16 bit register read
  };
  spi_slave_burst_read_write_scatter(BMI160_SPI, sg_info, ARRAY_LENGTH(sg_info));
  return value;
}

void bmi160_write_reg(uint8_t reg, uint8_t value) {
  reg &= BMI160_REG_MASK;
  SPIScatterGather sg_info[2] = {
    {.sg_len = 1, .sg_out = &reg, .sg_in = NULL}, // address
    {.sg_len = 1, .sg_out = &value, .sg_in = NULL} // register write
  };
  spi_slave_burst_read_write_scatter(BMI160_SPI, sg_info, ARRAY_LENGTH(sg_info));
}

void bmi160_enable_spi_mode(void) {
  // The BMI160 needs a rising edge on the SCS pin to switch into SPI mode.
  // The datasheet recommends performing a read to register 0x7F (reserved)
  // to put the chip into SPI mode.
  bmi160_read_reg(0x7f);

  psleep(2);  // Necessary on cold boots; not sure why
}
