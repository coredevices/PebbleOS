/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>

#include "drivers/flash.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "stm32f2xx_gpio.h"
#include "util/delay.h"

static const uint32_t EXPECTED_SPI_FLASH_ID_32MBIT = 0x20bb16;
static const uint32_t EXPECTED_SPI_FLASH_ID_64MBIT = 0x20bb17;

// Serial-flash commands
static const uint8_t FLASH_CMD_WRITE_ENABLE = 0x06;
static const uint8_t FLASH_CMD_WRITE_DISABLE = 0x04;
static const uint8_t FLASH_CMD_READ_STATUS_REG = 0x05;
static const uint8_t FLASH_CMD_READ = 0x03;
static const uint8_t FLASH_CMD_READ_ID = 0x9F;
static const uint8_t FLASH_CMD_DEEP_SLEEP = 0xB9;
static const uint8_t FLASH_CMD_WAKE = 0xAB;
static const uint8_t FLASH_CMD_DUMMY = 0xA9;

static const struct {
  SPI_TypeDef *const spi;
  GPIO_TypeDef *const spi_gpio;
  uint8_t scs_pin, sclk_pin, miso_pin, mosi_pin;
} FLASH_CONFIG = {
  .spi = SPI1,
  .spi_gpio = GPIOA,
  .scs_pin = 4,
  .sclk_pin = 5,
  .miso_pin = 6,
  .mosi_pin = 7,
};

static void prv_enable_flash_spi_clock(void) {
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
}

static void prv_disable_flash_spi_clock(void) {
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, DISABLE);
}

static void prv_flash_start(void) {
  gpio_use(FLASH_CONFIG.spi_gpio);

  // Enable the GPIOA clock
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

  uint8_t altfunc = GPIO_AF_SPI1;

  // Connect pins to their SPI functionality
  GPIO_PinAFConfig(FLASH_CONFIG.spi_gpio, FLASH_CONFIG.sclk_pin, altfunc);
  GPIO_PinAFConfig(FLASH_CONFIG.spi_gpio, FLASH_CONFIG.miso_pin, altfunc);
  GPIO_PinAFConfig(FLASH_CONFIG.spi_gpio, FLASH_CONFIG.mosi_pin, altfunc);

  // Setup MISO/MOSI
  GPIO_InitTypeDef gpio_cfg;
  gpio_cfg.GPIO_OType = GPIO_OType_PP;
  gpio_cfg.GPIO_PuPd = GPIO_PuPd_NOPULL;
  gpio_cfg.GPIO_Mode = GPIO_Mode_AF;
  gpio_cfg.GPIO_Speed = GPIO_Speed_50MHz;
  gpio_cfg.GPIO_Pin = (1 << FLASH_CONFIG.miso_pin) | (1 << FLASH_CONFIG.mosi_pin);
  GPIO_Init(FLASH_CONFIG.spi_gpio, &gpio_cfg);

  // Configure the SCLK pin to have a weak pull-down to put it in a known state
  // when SCS is toggled
  gpio_cfg.GPIO_PuPd = GPIO_PuPd_DOWN;
  gpio_cfg.GPIO_Pin = (1 << FLASH_CONFIG.sclk_pin);
  GPIO_Init(FLASH_CONFIG.spi_gpio, &gpio_cfg);

  // Configure SCS to be controlled in software; pull up to high when inactive
  gpio_cfg.GPIO_Mode = GPIO_Mode_OUT;
  gpio_cfg.GPIO_Pin = (1 << FLASH_CONFIG.scs_pin);
  gpio_cfg.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_Init(FLASH_CONFIG.spi_gpio, &gpio_cfg);

  // Set up a SPI bus on SPI1
  SPI_InitTypeDef spi_cfg;
  SPI_I2S_DeInit(FLASH_CONFIG.spi);
  spi_cfg.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
  spi_cfg.SPI_Mode = SPI_Mode_Master;
  spi_cfg.SPI_DataSize = SPI_DataSize_8b;
  spi_cfg.SPI_CPOL = SPI_CPOL_Low;
  spi_cfg.SPI_CPHA = SPI_CPHA_1Edge;
  spi_cfg.SPI_NSS = SPI_NSS_Soft;
  // APB2 is at 16MHz, max is 54MHz, so we want the smallest prescaler
  spi_cfg.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
  spi_cfg.SPI_FirstBit = SPI_FirstBit_MSB;
  spi_cfg.SPI_CRCPolynomial = 7;
  SPI_Init(FLASH_CONFIG.spi, &spi_cfg);

  SPI_Cmd(FLASH_CONFIG.spi, ENABLE);

  gpio_release(FLASH_CONFIG.spi_gpio);
}

static void prv_flash_start_cmd(void) {
  gpio_use(FLASH_CONFIG.spi_gpio);
  GPIO_ResetBits(FLASH_CONFIG.spi_gpio, 1 << FLASH_CONFIG.scs_pin);
  gpio_release(FLASH_CONFIG.spi_gpio);
}

static void prv_flash_end_cmd(void) {
  gpio_use(FLASH_CONFIG.spi_gpio);
  GPIO_SetBits(FLASH_CONFIG.spi_gpio, 1 << FLASH_CONFIG.scs_pin);
  gpio_release(FLASH_CONFIG.spi_gpio);

  // 50ns required between SCS going high and low again, so just delay here to be safe
  delay_us(1);
}

static uint8_t prv_flash_send_and_receive_byte(uint8_t byte) {
  // Ensure that there are no other write operations in progress
  while (SPI_I2S_GetFlagStatus(FLASH_CONFIG.spi, SPI_I2S_FLAG_TXE) == RESET) {}
  // Send the byte on the SPI bus
  SPI_I2S_SendData(FLASH_CONFIG.spi, byte);

  // Wait for the response byte to be received
  while (SPI_I2S_GetFlagStatus(FLASH_CONFIG.spi, SPI_I2S_FLAG_RXNE) == RESET) {}
  // Return the byte
  return SPI_I2S_ReceiveData(FLASH_CONFIG.spi);
}

static void prv_flash_send_24b_address(uint32_t start_addr) {
  // Ensure the high bits are not set.
  prv_flash_send_and_receive_byte((start_addr & 0xFF0000) >> 16);
  prv_flash_send_and_receive_byte((start_addr & 0x00FF00) >> 8);
  prv_flash_send_and_receive_byte((start_addr & 0x0000FF));
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

static void prv_flash_wait_for_write(void) {
  prv_flash_start_cmd();

  prv_flash_send_and_receive_byte(FLASH_CMD_READ_STATUS_REG);

  uint8_t status_register = 0;
  do {
    status_register = prv_flash_read_next_byte();
  } while (status_register & 0x1);

  prv_flash_end_cmd();
}

static void prv_flash_deep_sleep_exit(void) {
  prv_flash_start_cmd();
  prv_flash_send_and_receive_byte(FLASH_CMD_WAKE);
  prv_flash_end_cmd();

  // wait a sufficient amount of time to enter standby mode
  // It appears violating these timing conditions can lead to
  // random bit corruptions on flash writes!
  delay_us(100);
}

static uint32_t prv_flash_whoami(void) {
  prv_enable_flash_spi_clock();

  prv_flash_wait_for_write_bounded(64000000);

  prv_flash_start_cmd();
  prv_flash_send_and_receive_byte(FLASH_CMD_READ_ID);
  uint32_t manufacturer = prv_flash_read_next_byte();
  uint32_t type = prv_flash_read_next_byte();
  uint32_t capacity = prv_flash_read_next_byte();
  prv_flash_end_cmd();

  prv_disable_flash_spi_clock();

  return ((manufacturer << 16) | (type << 8) | capacity);
}

static bool prv_check_whoami(uint32_t spi_flash_id) {
  return spi_flash_id == EXPECTED_SPI_FLASH_ID_32MBIT ||
    spi_flash_id == EXPECTED_SPI_FLASH_ID_64MBIT;
}

static bool prv_is_whoami_correct(void) {
  uint32_t spi_flash_id = prv_flash_whoami();
  return prv_check_whoami(spi_flash_id);
}

void flash_init(void) {
  prv_enable_flash_spi_clock();

  prv_flash_start();

  // Assume that last time we shut down we were asleep. Come back out.
  prv_flash_deep_sleep_exit();

  prv_disable_flash_spi_clock();

  prv_flash_whoami();
}

bool flash_sanity_check(void) {
  return prv_is_whoami_correct();
}

void flash_read_bytes(uint8_t* buffer, uint32_t start_addr, uint32_t buffer_size) {
  if (!buffer_size) {
    return;
  }

  prv_enable_flash_spi_clock();
  prv_flash_wait_for_write();

  prv_flash_start_cmd();

  prv_flash_send_and_receive_byte(FLASH_CMD_READ);
  prv_flash_send_24b_address(start_addr);

  while (buffer_size--) {
    *buffer = prv_flash_read_next_byte();
    buffer++;
  }

  prv_flash_end_cmd();

  prv_disable_flash_spi_clock();
}
