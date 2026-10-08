/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include <pbl/drivers/crc.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/irq.h>
#include <pbl/logging/logging.h>

#include <bf0_hal.h>
#include <bf0_hal_crc.h>

PBL_LOG_MODULE_DEFINE(driver_crc_sf32lb, CONFIG_DRIVER_CRC_LOG_LEVEL);

#define CRC32_POLY 0x04C11DB7U

#define CR_CRC32                                                                                 \
  ((HAL_CRC_POLYSIZE_32 << CRC_CR_POLYSIZE_Pos) | (HAL_CRC_DATASIZE_32 << CRC_CR_DATASIZE_Pos) | \
   (1U << CRC_CR_REV_IN_Pos) | CRC_CR_REV_OUT)
#define CR_CRC32_LEGACY \
  ((HAL_CRC_POLYSIZE_32 << CRC_CR_POLYSIZE_Pos) | (HAL_CRC_DATASIZE_32 << CRC_CR_DATASIZE_Pos))

// Longest run fed to the unit with interrupts locked
#define BURST_LEN 256U

enum algo {
  ALGO_CRC32,
  ALGO_CRC32_LEGACY,
};

enum state {
  STATE_UNTESTED,
  STATE_OK,
  STATE_BROKEN,
};

static volatile enum state s_state = STATE_UNTESTED;

static uint32_t prv_burst(enum algo algo, uint32_t value, const uint8_t *data, size_t len) {
  CRC_TypeDef *const crc = hwp_crc1;

  // The unit keeps its register unreflected: a reflected CRC-32 resumes from the bit-reversed
  // complement of its output.
  crc->POL = CRC32_POLY;
  if (algo == ALGO_CRC32) {
    crc->INIT = __RBIT(~value);
    crc->CR = CR_CRC32;
  } else {
    crc->INIT = value;
    crc->CR = CR_CRC32_LEGACY;
  }
  crc->CR |= CRC_CR_RESET;

  for (size_t i = 0; i < len; i += 4) {
    uint32_t word;
    memcpy(&word, &data[i], sizeof(word));
    // The unit consumes words in memory order; the legacy checksum takes each one MSB first.
    crc->DR = (algo == ALGO_CRC32) ? word : PBL_BSWAP32(word);
    while ((crc->SR & CRC_SR_DONE) == 0) {
    }
  }

  return (algo == ALGO_CRC32) ? ~crc->DR : crc->DR;
}

static size_t prv_compute(enum algo algo, uint32_t *value, const void *data, size_t len) {
  const uint8_t *bytes = data;
  const size_t total = len & ~(size_t)3;
  uint32_t result = *value;

  for (size_t done = 0; done < total;) {
    const size_t burst = (total - done < BURST_LEN) ? (total - done) : BURST_LEN;
    pbl_irq_lock();
    HAL_RCC_EnableModule(RCC_MOD_CRC1);
    result = prv_burst(algo, result, &bytes[done], burst);
    HAL_RCC_DisableModule(RCC_MOD_CRC1);
    pbl_irq_unlock();
    done += burst;
  }

  *value = result;
  return total;
}

static bool prv_check(enum algo algo, uint32_t init, uint32_t expected) {
  static const char vector[] = "0123456789abcdef";
  const size_t len = sizeof(vector) - 1;

  uint32_t whole = init;
  prv_compute(algo, &whole, vector, len);

  uint32_t resumed = init;
  prv_compute(algo, &resumed, vector, len / 2);
  prv_compute(algo, &resumed, &vector[len / 2], len / 2);

  if ((whole != expected) || (resumed != expected)) {
    PBL_LOG_ERR("CRC unit self-test failed (algo %d): 0x%08" PRIx32 "/0x%08" PRIx32
                ", expected 0x%08" PRIx32,
                algo, whole, resumed, expected);
    return false;
  }
  return true;
}

static bool prv_ready(void) {
  if (s_state == STATE_UNTESTED) {
    const bool ok = prv_check(ALGO_CRC32, 0, 0x68c4f033U) &&
                    prv_check(ALGO_CRC32_LEGACY, 0xffffffffU, 0x50b663c0U);
    s_state = ok ? STATE_OK : STATE_BROKEN;
  }
  return s_state == STATE_OK;
}

size_t pbl_crc_hw_crc32(uint32_t *crc, const void *data, size_t len) {
  return prv_ready() ? prv_compute(ALGO_CRC32, crc, data, len) : 0;
}

size_t pbl_crc_hw_crc32_legacy(uint32_t *reg, const void *data, size_t len) {
  return prv_ready() ? prv_compute(ALGO_CRC32_LEGACY, reg, data, len) : 0;
}
