/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "drivers/rtc.h"
#include "drivers/spi.h"
#include "kernel/util/sleep.h"
#include "util/units.h"


#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#include <mcu.h>

#include "bma255_private.h"
#include "bma255_regs.h"

#define BMA255_SPI SPI3
static const uint32_t BMA255_PERIPH_CLOCK = RCC_APB1Periph_SPI3;
static const SpiPeriphClock BMA255_SPI_CLOCK = SpiPeriphClockAPB1;

static const AfConfig BMA255_SCLK_CONFIG = { GPIOB, GPIO_Pin_12, GPIO_PinSource12, GPIO_AF7_SPI3 };
static const AfConfig BMA255_MISO_CONFIG = { GPIOC, GPIO_Pin_11, GPIO_PinSource11, GPIO_AF_SPI3 };
static const AfConfig BMA255_MOSI_CONFIG = { GPIOC, GPIO_Pin_12, GPIO_PinSource12, GPIO_AF_SPI3 };
static const OutputConfig BMA255_SCS_CONFIG = { GPIOA, GPIO_Pin_4, false };

// Need to wait a minimum of 450µs after a write.
// Due to RTC resolution, we need to make sure that the tick counter has
// incremented twice so we can be certain at least one full tick-period has elapsed.
#define MIN_TICKS_AFTER_WRITE 2
static RtcTicks s_last_write_ticks = 0;
_Static_assert(RTC_TICKS_HZ < (1000000 / 450), "Tick period < 450µs");

void bma255_gpio_init(void) {
  periph_config_acquire_lock();

  gpio_af_init(&BMA255_SCLK_CONFIG, GPIO_OType_PP, GPIO_Speed_50MHz, GPIO_PuPd_NOPULL);
  gpio_af_init(&BMA255_MISO_CONFIG, GPIO_OType_PP, GPIO_Speed_50MHz, GPIO_PuPd_NOPULL);
  gpio_af_init(&BMA255_MOSI_CONFIG, GPIO_OType_PP, GPIO_Speed_50MHz, GPIO_PuPd_NOPULL);
  gpio_output_init(&BMA255_SCS_CONFIG, GPIO_OType_PP, GPIO_Speed_50MHz);

  SPI_InitTypeDef spi_cfg;
  SPI_I2S_DeInit(BMA255_SPI);
  spi_cfg.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
  spi_cfg.SPI_Mode = SPI_Mode_Master;
  spi_cfg.SPI_DataSize = SPI_DataSize_8b;
  spi_cfg.SPI_CPOL = SPI_CPOL_Low;
  spi_cfg.SPI_CPHA = SPI_CPHA_1Edge;
  spi_cfg.SPI_NSS = SPI_NSS_Soft;
  // Max SCLK frequency for the BMA255 is 10 MHz
  spi_cfg.SPI_BaudRatePrescaler = spi_find_prescaler(MHZ_TO_HZ(5), BMA255_SPI_CLOCK);
  spi_cfg.SPI_FirstBit = SPI_FirstBit_MSB;
  spi_cfg.SPI_CRCPolynomial = 7;

  bma255_enable_spi_clock();
  SPI_Init(BMA255_SPI, &spi_cfg);
  SPI_Cmd(BMA255_SPI, ENABLE);
  bma255_disable_spi_clock();

  periph_config_release_lock();
}

void bma255_enable_spi_clock(void) {
  periph_config_enable(BMA255_SPI, BMA255_PERIPH_CLOCK);
}

void bma255_disable_spi_clock(void) {
  periph_config_disable(BMA255_SPI, BMA255_PERIPH_CLOCK);
}

uint8_t bma255_send_and_receive_byte(uint8_t byte) {
  // Ensure that there are no other write operations in progress
  while (SPI_I2S_GetFlagStatus(SPI3, SPI_I2S_FLAG_TXE) == RESET) {}
  // Send the byte on the SPI bus
  SPI_I2S_SendData(BMA255_SPI, byte);

  // Wait for the response byte to be received
  while (SPI_I2S_GetFlagStatus(BMA255_SPI, SPI_I2S_FLAG_RXNE) == RESET) {}
  // Return the byte
  return SPI_I2S_ReceiveData(BMA255_SPI);
}

void bma255_send_byte(uint8_t byte) {
  // Ensure that there are no other write operations in progress
  while (SPI_I2S_GetFlagStatus(SPI3, SPI_I2S_FLAG_TXE) == RESET) {}
  // Send the byte on the SPI bus
  SPI_I2S_SendData(BMA255_SPI, byte);
}

//! Set SCS for transaction, start spi clock, and send address
void bma255_prepare_txn(uint8_t address) {
  while (rtc_get_ticks() < (s_last_write_ticks + MIN_TICKS_AFTER_WRITE)) {
    psleep(1);
  }

  gpio_output_set(&BMA255_SCS_CONFIG, true);
  bma255_enable_spi_clock();
  bma255_send_and_receive_byte(address);
}

//! Disables spi clock and sets SCS to end txn
void bma255_end_txn(void) {
  bma255_disable_spi_clock();
  gpio_output_set(&BMA255_SCS_CONFIG, false);
}

void bma255_burst_read(uint8_t address, uint8_t *data, size_t len) {
  const uint8_t reg = address | BMA255_READ_FLAG;

  bma255_prepare_txn(reg);
  for (size_t i = 0; i < len; ++i) {
    data[i] = bma255_send_and_receive_byte(0);
  }
  bma255_end_txn();
}

uint8_t bma255_read_register(uint8_t address) {
  const uint8_t reg = address | BMA255_READ_FLAG;

  bma255_prepare_txn(reg);
  // Read data
  uint8_t data = bma255_send_and_receive_byte(0);
  bma255_end_txn();

  return data;
}

void bma255_write_register(uint8_t address, uint8_t data) {
  const uint8_t reg = address | BMA255_WRITE_FLAG;

  bma255_prepare_txn(reg);
  bma255_send_and_receive_byte(data);
  bma255_end_txn();

  s_last_write_ticks = rtc_get_ticks();
}

void bma255_read_modify_write(uint8_t reg, uint8_t value, uint8_t mask) {
  uint8_t new_val = bma255_read_register(reg);
  new_val &= ~mask;
  new_val |= value;
  bma255_write_register(reg, new_val);
}
