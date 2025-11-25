/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include "drivers/flash.h"
#include "drivers/flash/micron_n25q/flash_private.h"
#include "drivers/watchdog.h"
#include "flash_region/flash_region.h"
#include "kernel/util/delay.h"
#include "util/math.h"

#include "kernel/core_dump_private.h"

//! We have our own flash driver for coredump support because it must not use
//! any FreeRTOS constructs & we want to keep it as simple as possible. In
//! addition we want the flexibility to be able to reset the flash driver to
//! get it into a working state

static void prv_flash_start_cmd(void) {
  GPIO_ResetBits(FLASH_GPIO, FLASH_PIN_SCS);
}

static void prv_flash_end_cmd(void) {
  GPIO_SetBits(FLASH_GPIO, FLASH_PIN_SCS);

  // 50ns required between SCS going high and low again, so just delay here to be safe
  delay_us(1);
}

static uint8_t prv_flash_send_and_receive_byte(uint8_t byte) {
  // Ensure that there are no other write operations in progress
  while (SPI_I2S_GetFlagStatus(FLASH_SPI, SPI_I2S_FLAG_TXE) == RESET) {
    continue;
  }
  // Send the byte on the SPI bus
  SPI_I2S_SendData(FLASH_SPI, byte);

  // Wait for the response byte to be received
  while (SPI_I2S_GetFlagStatus(FLASH_SPI, SPI_I2S_FLAG_RXNE) == RESET) {
    continue;
  }
  // Return the byte
  return SPI_I2S_ReceiveData(FLASH_SPI);
}

static uint8_t prv_flash_read_next_byte(void) {
  uint8_t result = prv_flash_send_and_receive_byte(FLASH_CMD_DUMMY);
  return result;
}

static void prv_flash_wait_for_write_bounded(volatile int cycles_to_wait) {
  prv_flash_start_cmd();

  prv_flash_send_and_receive_byte(FLASH_CMD_READ_STATUS_REG);

  uint8_t status_register = 0;
  do {
    if (cycles_to_wait-- < 1) {
      break;
    }
    status_register = prv_flash_read_next_byte();
  } while (status_register & 0x1);

  prv_flash_end_cmd();
}

// Init the flash hardware
void cd_flash_init(void) {
  // Enable the SPI clock
  RCC_APB2PeriphClockCmd(FLASH_SPI_CLOCK, ENABLE);

  // Enable the GPIO clock
  uint8_t idx = ((((uint32_t)FLASH_GPIO) - AHB1PERIPH_BASE) / 0x0400);
  SET_BIT(RCC->AHB1ENR, (0x1 << idx));

  // Init the flash hardware
  flash_hw_init();

  // Make sure we are not in deep sleep
  prv_flash_start_cmd();
  prv_flash_send_and_receive_byte(FLASH_CMD_WAKE);
  prv_flash_end_cmd();

  // See if we can successfully access the flash
  // TODO: Will we successfully recover if the flash HW was left midstream in a command from
  //  before?
  prv_flash_wait_for_write_bounded(64000000);
  prv_flash_start_cmd();
  prv_flash_send_and_receive_byte(FLASH_CMD_READ_ID);
  uint32_t manufacturer = prv_flash_read_next_byte();
  uint32_t type = prv_flash_read_next_byte();
  uint32_t capacity = prv_flash_read_next_byte();
  prv_flash_end_cmd();

  // If we can't ready the flash info correctly, bail
  CD_ASSERTN(manufacturer == 0x20 && type == 0xbb && (capacity >= 0x16));
}

static void prv_flash_write_enable(void) {
  prv_flash_start_cmd();
  prv_flash_send_and_receive_byte(FLASH_CMD_WRITE_ENABLE);
  prv_flash_end_cmd();
}

static void prv_flash_send_24b_address(uint32_t start_addr) {
  // Ensure the high bits are not set.
  prv_flash_send_and_receive_byte((start_addr & 0xFF0000) >> 16);
  prv_flash_send_and_receive_byte((start_addr & 0x00FF00) >> 8);
  prv_flash_send_and_receive_byte((start_addr & 0x0000FF));
}

static void prv_flash_wait_for_write(void) {
  prv_flash_start_cmd();

  prv_flash_send_and_receive_byte(FLASH_CMD_READ_STATUS_REG);

  uint8_t status_register = 0;
  do {
    status_register = prv_flash_read_next_byte();
  } while (status_register & 0x1);

  prv_flash_end_cmd();
}

static void prv_flash_write_page(const uint8_t* buffer, uint32_t start_addr, uint16_t buffer_size) {
  // Ensure that we're not trying to write more data than a single page (256 bytes)
  CD_ASSERTN(buffer_size <= FLASH_PAGE_SIZE);
  CD_ASSERTN(buffer_size);

  // Writing a zero-length buffer is a no-op.
  if (buffer_size < 1) {
    return;
  }

  prv_flash_write_enable();
  prv_flash_start_cmd();
  prv_flash_send_and_receive_byte(FLASH_CMD_PAGE_PROGRAM);
  prv_flash_send_24b_address(start_addr);

  while (buffer_size--) {
    prv_flash_send_and_receive_byte(*buffer);
    buffer++;
  }
  prv_flash_end_cmd();
  prv_flash_wait_for_write();
}

void cd_flash_read_bytes(void* buffer_ptr, uint32_t start_addr, uint32_t buffer_size) {
  uint8_t* buffer = buffer_ptr;
  prv_flash_wait_for_write();
  prv_flash_start_cmd();
  prv_flash_send_and_receive_byte(FLASH_CMD_READ);
  prv_flash_send_24b_address(start_addr);

  while (buffer_size--) {
    *buffer = prv_flash_read_next_byte();
    buffer++;
  }
  prv_flash_end_cmd();
}

uint32_t cd_flash_write_bytes(const void* buffer_ptr, uint32_t start_addr, uint32_t buffer_size) {
  CD_ASSERTN((start_addr + buffer_size <= CORE_DUMP_FLASH_END) &&
      (int)start_addr >= CORE_DUMP_FLASH_START);

  const uint8_t* buffer = buffer_ptr;
  const uint32_t total_bytes = buffer_size;
  uint32_t first_page_available_bytes = FLASH_PAGE_SIZE - (start_addr % FLASH_PAGE_SIZE);
  uint32_t bytes_to_write = MIN(buffer_size, first_page_available_bytes);

  while (bytes_to_write) {
    prv_flash_write_page(buffer, start_addr, bytes_to_write);
    start_addr += bytes_to_write;
    buffer += bytes_to_write;
    buffer_size -= bytes_to_write;
    bytes_to_write = MIN(buffer_size, FLASH_PAGE_SIZE);
  }
  watchdog_feed();
  return total_bytes;
}

static void prv_flash_erase_sector(uint32_t sector_addr) {
  prv_flash_write_enable();

  prv_flash_start_cmd();
  prv_flash_send_and_receive_byte(FLASH_CMD_ERASE_SECTOR);
  prv_flash_send_24b_address(sector_addr);
  prv_flash_end_cmd();

  prv_flash_wait_for_write();
}

static void prv_flash_erase_subsector(uint32_t sector_addr) {
  prv_flash_write_enable();

  prv_flash_start_cmd();
  prv_flash_send_and_receive_byte(FLASH_CMD_ERASE_SUBSECTOR);
  prv_flash_send_24b_address(sector_addr);
  prv_flash_end_cmd();

  prv_flash_wait_for_write();
}

// Erase a region comprised of 1 or more sub-sectors. This will erase sectors at a time if
// the address and size allow.
void cd_flash_erase_region(uint32_t start_addr, uint32_t total_bytes) {
  CD_ASSERTN(((start_addr & SUBSECTOR_ADDR_MASK) == start_addr)
              && ((total_bytes & SUBSECTOR_ADDR_MASK) == total_bytes));

  while (total_bytes > 0) {
    if (((start_addr & SECTOR_ADDR_MASK) == start_addr) && (total_bytes >= SECTOR_SIZE_BYTES)) {
      prv_flash_erase_sector(start_addr);
      total_bytes -= SECTOR_SIZE_BYTES;
      start_addr += SECTOR_SIZE_BYTES;
    } else {
      prv_flash_erase_subsector(start_addr);
      total_bytes -= SUBSECTOR_SIZE_BYTES;
      start_addr += SUBSECTOR_SIZE_BYTES;
    }
    watchdog_feed();
  }
}
