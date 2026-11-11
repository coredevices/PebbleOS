/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>
#include <string.h>

#include <pbl/device.h>
#include <pbl/devicetree.h>
#include <pbl/drivers/crc.h>
#include <pbl/drivers/crc/sf32lb.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bitops.h>

#include <hardware/sf32lb52_crc.h>

PBL_LOG_MODULE_DEFINE(driver_crc_sf32lb, CONFIG_DRIVER_CRC_LOG_LEVEL);

#define CRC32_POLY 0x04C11DB7U

#define CR_CRC32_LEGACY                                       \
  (PBL_FIELD_PREP(CRC_CR_POLYSIZE_MASK, CRC_CR_POLYSIZE_32) | \
   PBL_FIELD_PREP(CRC_CR_DATASIZE_MASK, CRC_CR_DATASIZE_32))
#define CR_CRC32 \
  (CR_CRC32_LEGACY | PBL_FIELD_PREP(CRC_CR_REV_IN_MASK, CRC_CR_REV_IN_BYTE) | CRC_CR_REV_OUT)

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

extern const struct pbl_crc_sf32lb pbl_crc_sf32lb_inst;

static const struct pbl_crc_sf32lb *const s_crc = &pbl_crc_sf32lb_inst;
static volatile enum state s_state = STATE_UNTESTED;

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(s_crc->base + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, s_crc->base + offset);
}

static uint32_t prv_burst(enum algo algo, uint32_t value, const uint8_t *data, size_t len) {
  // The unit keeps its register unreflected: a reflected CRC-32 resumes from the bit-reversed
  // complement of its output.
  const uint32_t cr = (algo == ALGO_CRC32) ? CR_CRC32 : CR_CRC32_LEGACY;

  prv_putreg(SF32LB52_CRC_POL_OFFSET, CRC32_POLY);
  prv_putreg(SF32LB52_CRC_INIT_OFFSET, (algo == ALGO_CRC32) ? pbl_bitrev32(~value) : value);
  prv_putreg(SF32LB52_CRC_CR_OFFSET, cr);
  prv_putreg(SF32LB52_CRC_CR_OFFSET, cr | CRC_CR_RESET);

  for (size_t i = 0; i < len; i += 4) {
    uint32_t word;
    memcpy(&word, &data[i], sizeof(word));
    // The unit consumes words in memory order; the legacy checksum takes each one MSB first.
    prv_putreg(SF32LB52_CRC_DR_OFFSET, (algo == ALGO_CRC32) ? word : PBL_BSWAP32(word));
    while ((prv_getreg(SF32LB52_CRC_SR_OFFSET) & CRC_SR_DONE) == 0U) {
    }
  }

  value = prv_getreg(SF32LB52_CRC_DR_OFFSET);

  return (algo == ALGO_CRC32) ? ~value : value;
}

static size_t prv_compute(enum algo algo, uint32_t *value, const void *data, size_t len) {
  const uint8_t *bytes = data;
  const size_t total = len & ~(size_t)3;
  size_t done = 0;

  while (done < total) {
    const size_t burst = (total - done < BURST_LEN) ? (total - done) : BURST_LEN;

    pbl_irq_lock();
    if (pbl_clk_enable(&s_crc->clk) != 0) {
      pbl_irq_unlock();
      break;
    }
    *value = prv_burst(algo, *value, &bytes[done], burst);
    pbl_clk_disable(&s_crc->clk);
    pbl_irq_unlock();
    done += burst;
  }

  return done;
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
    if (pbl_device_init(&s_crc->clk.ctrl->dev) != 0) {
      s_state = STATE_BROKEN;
      return false;
    }

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

#include PBL_DT_INSTANCES(sifli_sf32lb52_crc)
