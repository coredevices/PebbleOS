/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/crc.h"

#include "drivers/flash.h"
#include "drivers/periph_config.h"

#include "stm32f2xx_crc.h"
#include "stm32f2xx_rcc.h"

#include <inttypes.h>

static void prv_enable_crc_clock(void) {
  periph_config_enable(RCC_AHB1PeriphClockCmd, RCC_AHB1Periph_CRC);
}

static void prv_disable_crc_clock(void) {
  periph_config_disable(RCC_AHB1PeriphClockCmd, RCC_AHB1Periph_CRC);
}

static void prv_calculate_incremental_start(void) {
  prv_enable_crc_clock();
  CRC_ResetDR();
}

static void prv_calculate_incremental_words(const uint32_t* data, unsigned int data_length) {
  CRC_CalcBlockCRC((uint32_t*) data, data_length);
}

static uint32_t prv_calculate_incremental_remaining_bytes(const uint8_t* data,
                                                          unsigned int data_length) {
  uint32_t crc_value;

  if (data_length >= 4) {
    const unsigned int num_words = data_length / 4;
    prv_calculate_incremental_words((uint32_t*) data, num_words);

    data += num_words * 4;
    data_length -= num_words * 4;
  }

  if (data_length) {
    uint32_t last_word = 0;
    for (unsigned int i = 0; i < data_length; ++i) {
      last_word = (last_word << 8) | data[i];
    }
    crc_value = CRC_CalcCRC(last_word);
  } else {
    crc_value = CRC_GetCRC();
  }

  return crc_value;
}

static void prv_calculate_incremental_stop(void) {
  prv_disable_crc_clock();
}

uint32_t crc_calculate_bytes(const uint8_t* data, unsigned int data_length) {
  prv_calculate_incremental_start();

  // First calculate the CRC of the whole words, since the hardware works 4
  // bytes at a time.
  uint32_t* data_words = (uint32_t*) data;
  const unsigned int num_words = data_length / 4;
  prv_calculate_incremental_words(data_words, num_words);

  const unsigned int num_remaining_bytes = data_length % 4;
  const uint32_t res =
    prv_calculate_incremental_remaining_bytes(data + (num_words * 4), num_remaining_bytes);
  prv_calculate_incremental_stop();

  return (res);
}

uint32_t crc_calculate_flash(uint32_t address, unsigned int num_bytes) {
  prv_calculate_incremental_start();
  const unsigned int chunk_size = 128;

  uint8_t buffer[chunk_size];
  while (num_bytes > chunk_size) {
    flash_read_bytes(buffer, address, chunk_size);
    prv_calculate_incremental_words((const uint32_t*) buffer, chunk_size / 4);

    num_bytes -= chunk_size;
    address += chunk_size;
  }

  flash_read_bytes(buffer, address, num_bytes);
  const uint32_t res = prv_calculate_incremental_remaining_bytes(buffer, num_bytes);
  prv_calculate_incremental_stop();

  return (res);
}
