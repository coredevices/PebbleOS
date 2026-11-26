/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/flash/nrf_rramc.h>
#include <pbl/kernel/barrier.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/nrf54l_rramc.h>

#ifdef CONFIG_MPSL
#include <mpsl_pebble.h>
#endif

#define ERASED_BYTE 0xFFU
#define LINE_SIZE   16U
// Lines the controller gathers before programming them
#define WRITEBUF_LINES 16U
#define CHUNK_SIZE     (WRITEBUF_LINES * LINE_SIZE)
// A line programs in about 25 us, and up to 7107 us for a buffer of 32 lines
#define LINE_MAX_US 223U
// Copying a chunk to the buffer, within its timeslot
#define SLOT_MARGIN_US 250U

struct prv_chunk {
  const struct pbl_flash_nrf_rramc *rramc;
  uint32_t addr;
  const uint8_t *src;
  size_t len;
  uint32_t timeout_us;
  int ret;
};

static const struct pbl_flash_nrf_rramc *prv_rramc(const struct pbl_flash *flash) {
  return PBL_CONTAINER_OF(flash, const struct pbl_flash_nrf_rramc, flash);
}

static inline uint32_t prv_getreg(const struct pbl_flash_nrf_rramc *rramc, uint32_t offset) {
  return pbl_readl(rramc->base + offset);
}

static inline void prv_putreg(const struct pbl_flash_nrf_rramc *rramc, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, rramc->base + offset);
}

static void prv_copy(uintptr_t dst, const uint8_t *src, size_t len) {
  while (len > 0U && ((dst % 4U) != 0U || len < 4U)) {
    *(volatile uint8_t *)dst = (src != nullptr) ? *src++ : ERASED_BYTE;
    dst++;
    len--;
  }

  while (len >= 4U) {
    uint32_t word = UINT32_MAX;

    if (src != nullptr) {
      memcpy(&word, src, sizeof(word));
      src += 4U;
    }
    *(volatile uint32_t *)dst = word;
    dst += 4U;
    len -= 4U;
  }

  while (len > 0U) {
    *(volatile uint8_t *)dst = (src != nullptr) ? *src++ : ERASED_BYTE;
    dst++;
    len--;
  }
}

static void prv_chunk_program(void *arg) {
  struct prv_chunk *chunk = arg;
  const struct pbl_flash_nrf_rramc *rramc = chunk->rramc;

  prv_putreg(rramc, NRF54L_RRAMC_CONFIG_OFFSET,
             RRAMC_CONFIG_WEN | PBL_FIELD_PREP(RRAMC_CONFIG_WRITEBUFSIZE_MASK, WRITEBUF_LINES));
  prv_copy(rramc->mem + chunk->addr, chunk->src, chunk->len);
  pbl_mb();

  if ((prv_getreg(rramc, NRF54L_RRAMC_BUFSTATUS_WRITEBUFEMPTY_OFFSET) &
       RRAMC_BUFSTATUS_WRITEBUFEMPTY_EMPTY) == 0U) {
    prv_putreg(rramc, NRF54L_RRAMC_TASKS_COMMITWRITEBUF_OFFSET, 1U);
  }

  chunk->ret =
      PBL_POLL_BUSY_US((prv_getreg(rramc, NRF54L_RRAMC_READY_OFFSET) & RRAMC_READY_READY) != 0U, 0,
                       chunk->timeout_us);
  prv_putreg(rramc, NRF54L_RRAMC_CONFIG_OFFSET, 0U);
}

// Programs len bytes from src, or the erased value without src
static int prv_program(const struct pbl_flash *flash, uint32_t addr, const uint8_t *src,
                       size_t len) {
  struct prv_chunk chunk = {.rramc = prv_rramc(flash), .addr = addr, .src = src};

  while (len > 0U) {
    int ret = 0;

    chunk.len = PBL_MIN(len, CHUNK_SIZE - (chunk.addr % CHUNK_SIZE));
    chunk.timeout_us =
        PBL_DIV_ROUND_UP((chunk.addr % LINE_SIZE) + chunk.len, LINE_SIZE) * LINE_MAX_US;
    chunk.ret = 0;

#ifdef CONFIG_MPSL
    // Reads of the RRAM stall while it programs, and the radio interrupts run from it
    if (!pbl_flash_in_coredump(flash)) {
      ret = mpsl_pebble_timeslot_run(prv_chunk_program, &chunk, chunk.timeout_us + SLOT_MARGIN_US);
    } else
#endif
    {
      prv_chunk_program(&chunk);
    }
    if (ret != 0) {
      return ret;
    }
    if (chunk.ret != 0) {
      return -EIO;
    }

    chunk.addr += chunk.len;
    if (chunk.src != nullptr) {
      chunk.src += chunk.len;
    }
    len -= chunk.len;
  }

  return 0;
}

static int prv_read(const struct pbl_flash *flash, uint32_t addr, void *buf, size_t len) {
  memcpy(buf, (const void *)(prv_rramc(flash)->mem + addr), len);
  return 0;
}

static int prv_write(const struct pbl_flash *flash, uint32_t addr, const void *buf, size_t len) {
  return prv_program(flash, addr, buf, len);
}

static int prv_erase(const struct pbl_flash *flash, uint32_t addr, uint32_t size) {
  const int ret = prv_program(flash, addr, nullptr, size);

  return (ret == 0) ? 1 : ret;
}

const struct pbl_flash_ops pbl_flash_nrf_rramc_ops = {
  .read = prv_read,
  .write = prv_write,
  .erase = prv_erase,
};

#include PBL_DT_INSTANCES(nordic_nrf54l15_rramc)
