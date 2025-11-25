/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/crc.h"

#include "drivers/flash.h"
#include "drivers/periph_config.h"
#include "system/passert.h"

#if defined(MICRO_FAMILY_STM32F2)
#include "stm32f2xx_crc.h"
#include "stm32f2xx_rcc.h"
#elif defined(MICRO_FAMILY_STM32F4)
#include "stm32f4xx_crc.h"
#include "stm32f4xx_rcc.h"
#endif

#include <inttypes.h>

static bool s_initialized = false;
static bool s_clock_running = false;

static void enable_crc_clock(void) {
  // save the state so that if stop mode interrupts things, we resume cleanly
  s_clock_running = true;

  periph_config_enable(RCC_AHB1PeriphClockCmd, RCC_AHB1Periph_CRC);
}

static void disable_crc_clock(void) {
  // save the state so that if stop mode interrupts things, we resume cleanly
  s_clock_running = false;

  periph_config_disable(RCC_AHB1PeriphClockCmd, RCC_AHB1Periph_CRC);
}

void crc_init(void) {
  if (s_initialized) {
    return;
  }

  s_initialized = true;
}

void crc_calculate_incremental_start(void) {
  PBL_ASSERTN(s_initialized);

  enable_crc_clock();
  CRC_ResetDR();
}

static void crc_calculate_incremental_words(const uint32_t* data, unsigned int data_length) {
  PBL_ASSERTN(s_initialized);

  CRC_CalcBlockCRC((uint32_t*) data, data_length);
}

static uint32_t crc_calculate_incremental_remaining_bytes(const uint8_t* data, unsigned int data_length) {
  PBL_ASSERTN(s_initialized);
  uint32_t crc_value;

  if (data_length >= 4) {
    const unsigned int num_words = data_length / 4;
    crc_calculate_incremental_words((uint32_t*) data, num_words);

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

void crc_calculate_incremental_stop(void) {
  PBL_ASSERTN(s_initialized);

  disable_crc_clock();
}

uint32_t crc_calculate_bytes(const uint8_t* data, unsigned int data_length) {
  crc_calculate_incremental_start();

  // First calculate the CRC of the whole words, since the hardware works 4
  // bytes at a time.
  uint32_t* data_words = (uint32_t*) data;
  const unsigned int num_words = data_length / 4;
  crc_calculate_incremental_words(data_words, num_words);

  const unsigned int num_remaining_bytes = data_length % 4;
  const uint32_t res = crc_calculate_incremental_remaining_bytes(data + (num_words * 4), num_remaining_bytes);
  crc_calculate_incremental_stop();

  return (res);
}

uint32_t crc_calculate_flash(uint32_t address, unsigned int num_bytes) {
  crc_calculate_incremental_start();
  const unsigned int chunk_size = 128;

  uint8_t buffer[chunk_size];
  while (num_bytes > chunk_size) {

    flash_read_bytes(buffer, address, chunk_size);
    crc_calculate_incremental_words((const uint32_t*) buffer, chunk_size / 4);

    num_bytes -= chunk_size;
    address += chunk_size;
  }

  flash_read_bytes(buffer, address, num_bytes);
  const uint32_t res = crc_calculate_incremental_remaining_bytes(buffer, num_bytes);
  crc_calculate_incremental_stop();

  return (res);
}

uint8_t crc8_calculate_bytes(const uint8_t *data, unsigned int data_len) {
  // Optimal polynomial chosen based on
  // http://users.ece.cmu.edu/~koopman/roses/dsn04/koopman04_crc_poly_embedded.pdf
  // Note that this is different than the standard CRC-8 polynomial, because the
  // standard CRC-8 polynomial is not particularly good.

  // nibble lookup table for (x^8 + x^5 + x^3 + x^2 + x + 1)
  static const uint8_t lookup_table[] =
      { 0, 47, 94, 113, 188, 147, 226, 205, 87, 120, 9, 38, 235, 196,
        181, 154 };

  uint16_t crc = 0;
  for (int i = data_len * 2; i > 0; i--) {
    uint8_t nibble = data[(i - 1)/ 2];
    if (i % 2 == 0) {
      nibble >>= 4;
    }
    int index = nibble ^ (crc >> 4);
    crc = lookup_table[index & 0xf] ^ (crc << 4);
  }
  return crc;
}
