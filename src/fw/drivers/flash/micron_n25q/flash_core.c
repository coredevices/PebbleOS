/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/flash.h"
#include "kernel/util/delay.h"
#include "system/passert.h"
#include "util/units.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "drivers/flash/micron_n25q/flash_private.h"

void enable_flash_spi_clock(void) {
  periph_config_enable(FLASH_SPI, FLASH_SPI_CLOCK);
}

void disable_flash_spi_clock(void) {
  periph_config_disable(FLASH_SPI, FLASH_SPI_CLOCK);
}

// IMPORTANT: This method is also used by the core dump logic in order to re-initialize the flash hardware
// to prepare for writing the core dump. For this reason, it can NOT use any FreeRTOS functions, mess with
// the interrupt priority, primask, etc.
void flash_hw_init(void) {
  // Connect PA5 to SPI1_SCLK
  GPIO_PinAFConfig(FLASH_GPIO, GPIO_PinSource5, GPIO_AF_SPI1);

  // Connect PA6 to SPI1_MISO
  GPIO_PinAFConfig(FLASH_GPIO, GPIO_PinSource6, GPIO_AF_SPI1);

  // Connect PA7 to SPI1_MOSI
  GPIO_PinAFConfig(FLASH_GPIO, GPIO_PinSource7, GPIO_AF_SPI1);

  GPIO_InitTypeDef gpio_cfg;
  gpio_cfg.GPIO_OType = GPIO_OType_PP;
  gpio_cfg.GPIO_PuPd = GPIO_PuPd_NOPULL;
  gpio_cfg.GPIO_Mode = GPIO_Mode_AF;
  gpio_cfg.GPIO_Speed = GPIO_Speed_50MHz;
  gpio_cfg.GPIO_Pin = FLASH_PIN_MISO | FLASH_PIN_MOSI;
  GPIO_Init(FLASH_GPIO, &gpio_cfg);

  // Configure the SCLK pin to have a weak pull-down to put it in a known state
  // when SCS is toggled
  gpio_cfg.GPIO_PuPd = GPIO_PuPd_DOWN;
  gpio_cfg.GPIO_Pin = FLASH_PIN_SCLK;
  GPIO_Init(FLASH_GPIO, &gpio_cfg);

  // Configure SCS to be controlled in software; pull up to high when inactive
  gpio_cfg.GPIO_Mode = GPIO_Mode_OUT;
  gpio_cfg.GPIO_Pin = FLASH_PIN_SCS;
  gpio_cfg.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_Init(FLASH_GPIO, &gpio_cfg);

  // Set up a SPI bus on SPI1
  SPI_InitTypeDef spi_cfg;
  SPI_I2S_DeInit(FLASH_SPI);
  spi_cfg.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
  spi_cfg.SPI_Mode = SPI_Mode_Master;
  spi_cfg.SPI_DataSize = SPI_DataSize_8b;
  spi_cfg.SPI_CPOL = SPI_CPOL_Low;
  spi_cfg.SPI_CPHA = SPI_CPHA_1Edge;
  spi_cfg.SPI_NSS = SPI_NSS_Soft;
  spi_cfg.SPI_BaudRatePrescaler = spi_find_prescaler(MHZ_TO_HZ(54), FLASH_SPI_CLOCK_PERIPH); // max read freq for the flash
  spi_cfg.SPI_FirstBit = SPI_FirstBit_MSB;
  spi_cfg.SPI_CRCPolynomial = 7;
  SPI_Init(FLASH_SPI, &spi_cfg);

  SPI_Cmd(FLASH_SPI, ENABLE);
}

void flash_start(void) {
  periph_config_acquire_lock();
  gpio_use(FLASH_GPIO);

  // Init the hardware
  flash_hw_init();

  gpio_release(FLASH_GPIO);
  periph_config_release_lock();
}

void flash_start_cmd(void) {
  gpio_use(FLASH_GPIO);
  GPIO_ResetBits(FLASH_GPIO, FLASH_PIN_SCS);
  gpio_release(FLASH_GPIO);
}

void flash_end_cmd(void) {
  gpio_use(FLASH_GPIO);
  GPIO_SetBits(FLASH_GPIO, FLASH_PIN_SCS);
  gpio_release(FLASH_GPIO);

  // 50ns required between SCS going high and low again, so just delay here to be safe
  delay_us(1);
}

uint8_t flash_send_and_receive_byte(uint8_t byte) {
  // Ensure that there are no other write operations in progress
  while (SPI_I2S_GetFlagStatus(FLASH_SPI, SPI_I2S_FLAG_TXE) == RESET);
  // Send the byte on the SPI bus
  SPI_I2S_SendData(FLASH_SPI, byte);

  // Wait for the response byte to be received
  while (SPI_I2S_GetFlagStatus(FLASH_SPI, SPI_I2S_FLAG_RXNE) == RESET);
  // Return the byte
  return SPI_I2S_ReceiveData(FLASH_SPI);
}

void flash_write_enable(void) {
  flash_start_cmd();
  flash_send_and_receive_byte(FLASH_CMD_WRITE_ENABLE);
  flash_end_cmd();
}

void flash_send_24b_address(uint32_t start_addr) {
  // Ensure the high bits are not set.
  PBL_ASSERTN(!(start_addr & 0xFF000000));

  flash_send_and_receive_byte((start_addr & 0xFF0000) >> 16);
  flash_send_and_receive_byte((start_addr & 0x00FF00) >> 8);
  flash_send_and_receive_byte((start_addr & 0x0000FF));
}

uint8_t flash_read_next_byte(void) {
  uint8_t result = flash_send_and_receive_byte(FLASH_CMD_DUMMY);
  return result;
}

void flash_wait_for_write_bounded(volatile int cycles_to_wait) {
  flash_start_cmd();

  flash_send_and_receive_byte(FLASH_CMD_READ_STATUS_REG);

  uint8_t status_register = 0;
  do {
    if (cycles_to_wait-- < 1) {
      break;
    }
    status_register = flash_read_next_byte();
  } while (status_register & N25QStatusBit_WriteInProgress);

  flash_end_cmd();
}

void flash_wait_for_write(void) {
  flash_start_cmd();

  flash_send_and_receive_byte(FLASH_CMD_READ_STATUS_REG);

  uint8_t status_register = 0;
  do {
    status_register = flash_read_next_byte();
  } while (status_register & N25QStatusBit_WriteInProgress);

  flash_end_cmd();
}

bool flash_sector_is_erased(uint32_t sector_addr) {
  const uint32_t bufsize = 128;
  uint8_t buffer[bufsize];
  sector_addr &= SECTOR_ADDR_MASK;
  for (uint32_t offset = 0; offset < SECTOR_SIZE_BYTES; offset += bufsize) {
    flash_read_bytes(buffer, sector_addr + offset, bufsize);
    for (uint32_t i = 0; i < bufsize; i++) {
      if (buffer[i] != 0xff) {
        return false;
      }
    }
  }
  return true;
}

uint32_t flash_whoami(void) {
  assert_usable_state();

  flash_lock();

  if (!flash_is_enabled()) {
    flash_unlock();
    return 0;
  }

  enable_flash_spi_clock();
  handle_sleep_when_idle_begin();

  flash_wait_for_write_bounded(64000000);

  flash_start_cmd();
  flash_send_and_receive_byte(FLASH_CMD_READ_ID);
  uint32_t manufacturer = flash_read_next_byte();
  uint32_t type = flash_read_next_byte();
  uint32_t capacity = flash_read_next_byte();
  flash_end_cmd();

  disable_flash_spi_clock();
  flash_unlock();

  return ((manufacturer << 16) | (type << 8) | capacity);
}

bool check_whoami(uint32_t spi_flash_id) {
  return spi_flash_id == EXPECTED_SPI_FLASH_ID_32MBIT ||
    spi_flash_id == EXPECTED_SPI_FLASH_ID_64MBIT;
}

bool flash_is_whoami_correct(void) {
  uint32_t spi_flash_id = flash_whoami();
  return check_whoami(spi_flash_id);
}

void flash_switch_mode(FlashModeType mode) {
}

