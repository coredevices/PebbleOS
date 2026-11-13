/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/device.h>
#include <pbl/devicetree.h>
#include <pbl/drivers/crypto.h>
#include <pbl/drivers/crypto/sf32lb.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>

#include <hardware/sf32lb52_aes.h>

#define SHA256_BLOCK_SIZE 64U
#define AES_BLOCK_SIZE    16U

// Bytes processed with interrupts locked: four SHA-256 blocks, or eight AES blocks in and out
#define BURST_LEN 256U

#define SHA256_BURST_BLOCKS (BURST_LEN / SHA256_BLOCK_SIZE)
#define AES_BURST_BLOCKS    (BURST_LEN / 2U / AES_BLOCK_SIZE)
#define AES_OUT_OFFSET      (BURST_LEN / 2U)

#define TIMEOUT_US 1000U

#define HASH_SETTING PBL_FIELD_PREP(AES_HASH_SETTING_MODE_MASK, AES_HASH_SETTING_MODE_SHA256)

extern const struct pbl_crypto_sf32lb pbl_crypto_sf32lb_inst;

static const struct pbl_crypto_sf32lb *const s_crypto = &pbl_crypto_sf32lb_inst;

// The engine masters the bus: data goes through a buffer in SRAM, which needs no address
// translation and owns its cache lines.
static uint8_t s_buf[BURST_LEN] PBL_ALIGNED(PBL_DCACHE_LINE_SIZE);

static_assert((AES_OUT_OFFSET % PBL_DCACHE_LINE_SIZE) == 0U);

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(s_crypto->base + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, s_crypto->base + offset);
}

static int prv_wait(uint32_t busy, uint32_t errors, uint32_t reset) {
  int ret = PBL_POLL_BUSY_US((prv_getreg(SF32LB52_AES_STATUS_OFFSET) & busy) == 0U, 0, TIMEOUT_US);
  const uint32_t irq = prv_getreg(SF32LB52_AES_IRQ_OFFSET);

  prv_putreg(SF32LB52_AES_IRQ_OFFSET, irq);
  if ((ret == 0) && ((irq & errors) != 0U)) {
    ret = -EIO;
  }
  if (ret != 0) {
    prv_putreg(SF32LB52_AES_COMMAND_OFFSET, reset);
    prv_putreg(SF32LB52_AES_COMMAND_OFFSET, 0);
  }

  return ret;
}

static int prv_sha256_burst(uint32_t state[8], const uint8_t *data, size_t nblocks) {
  const size_t len = nblocks * SHA256_BLOCK_SIZE;
  int ret;

  memcpy(s_buf, data, len);
  pbl_dcache_flush_range(s_buf, len);

  prv_putreg(SF32LB52_AES_COMMAND_OFFSET, AES_COMMAND_HASH_RESET);
  prv_putreg(SF32LB52_AES_COMMAND_OFFSET, 0);

  // The engine holds the chaining value as big-endian words.
  for (unsigned int i = 0; i < 8U; i++) {
    prv_putreg(SF32LB52_AES_HASH_IV_OFFSET(i), PBL_BSWAP32(state[i]));
  }
  // The load takes the mode and IV selection from before the write that triggers it.
  prv_putreg(SF32LB52_AES_HASH_SETTING_OFFSET, HASH_SETTING | AES_HASH_SETTING_IV_SEL);
  prv_putreg(SF32LB52_AES_HASH_SETTING_OFFSET,
             HASH_SETTING | AES_HASH_SETTING_IV_SEL | AES_HASH_SETTING_IV_LOAD);
  prv_putreg(SF32LB52_AES_HASH_SETTING_OFFSET, HASH_SETTING);

  prv_putreg(SF32LB52_AES_SETTING_OFFSET, 0);
  prv_putreg(SF32LB52_AES_HASH_DMA_IN_OFFSET, (uint32_t)(uintptr_t)s_buf);
  prv_putreg(SF32LB52_AES_HASH_DMA_DATA_OFFSET, len);
  prv_putreg(SF32LB52_AES_COMMAND_OFFSET, AES_COMMAND_HASH_START);

  ret = prv_wait(AES_STATUS_HASH_BUSY, AES_IRQ_HASH_BUS_ERR | AES_IRQ_HASH_PAD_ERR,
                 AES_COMMAND_HASH_RESET);
  if (ret != 0) {
    return ret;
  }

  for (unsigned int i = 0; i < 8U; i++) {
    state[i] = PBL_BSWAP32(prv_getreg(SF32LB52_AES_HASH_RESULT_OFFSET(i)));
  }

  return 0;
}

static size_t prv_sha256(uint32_t state[8], const uint8_t *data, size_t nblocks) {
  size_t done = 0;

  while (done < nblocks) {
    const size_t burst =
        (nblocks - done < SHA256_BURST_BLOCKS) ? (nblocks - done) : SHA256_BURST_BLOCKS;
    int ret;

    pbl_irq_lock();
    ret = pbl_clk_enable(&s_crypto->clk);
    if (ret == 0) {
      ret = prv_sha256_burst(state, &data[done * SHA256_BLOCK_SIZE], burst);
      pbl_clk_disable(&s_crypto->clk);
    }
    pbl_irq_unlock();

    if (ret != 0) {
      break;
    }

    done += burst;
  }

  return done;
}

static int prv_aes_burst(uint32_t setting, const uint8_t *key, size_t key_len, uint8_t *out,
                         const uint8_t *in, size_t nblocks) {
  const size_t len = nblocks * AES_BLOCK_SIZE;
  int ret;

  memcpy(s_buf, in, len);
  pbl_dcache_flush_range(s_buf, len);
  pbl_dcache_flush_invalidate_range(&s_buf[AES_OUT_OFFSET], len);

  for (unsigned int i = 0; i < key_len / 4U; i++) {
    uint32_t word;
    memcpy(&word, &key[4U * i], sizeof(word));
    prv_putreg(SF32LB52_AES_EXT_KEY_OFFSET(i), word);
  }
  prv_putreg(SF32LB52_AES_AES_SETTING_OFFSET, setting);

  prv_putreg(SF32LB52_AES_SETTING_OFFSET, 0);
  prv_putreg(SF32LB52_AES_DMA_IN_OFFSET, (uint32_t)(uintptr_t)s_buf);
  prv_putreg(SF32LB52_AES_DMA_OUT_OFFSET, (uint32_t)(uintptr_t)&s_buf[AES_OUT_OFFSET]);
  prv_putreg(SF32LB52_AES_DMA_DATA_OFFSET, nblocks);
  prv_putreg(SF32LB52_AES_COMMAND_OFFSET, AES_COMMAND_START);

  ret = prv_wait(AES_STATUS_BUSY, AES_IRQ_BUS_ERR | AES_IRQ_SETUP_ERR, AES_COMMAND_AES_RESET);
  if (ret != 0) {
    return ret;
  }

  pbl_dcache_invalidate_range(&s_buf[AES_OUT_OFFSET], len);
  memcpy(out, &s_buf[AES_OUT_OFFSET], len);

  return 0;
}

static size_t prv_aes(bool encrypt, const uint8_t *key, size_t key_len, uint8_t *out,
                      const uint8_t *in, size_t nblocks) {
  uint32_t length;
  size_t done = 0;

  switch (key_len) {
    case 16:
      length = AES_AES_SETTING_LENGTH_128;
      break;
    case 24:
      length = AES_AES_SETTING_LENGTH_192;
      break;
    case 32:
      length = AES_AES_SETTING_LENGTH_256;
      break;
    default:
      return 0;
  }

  const uint32_t setting = PBL_FIELD_PREP(AES_AES_SETTING_MODE_MASK, AES_AES_SETTING_MODE_ECB) |
                           PBL_FIELD_PREP(AES_AES_SETTING_LENGTH_MASK, length) |
                           (encrypt ? AES_AES_SETTING_ENCRYPT : 0U);

  while (done < nblocks) {
    const size_t burst = (nblocks - done < AES_BURST_BLOCKS) ? (nblocks - done) : AES_BURST_BLOCKS;
    const size_t offset = done * AES_BLOCK_SIZE;
    int ret;

    pbl_irq_lock();
    ret = pbl_clk_enable(&s_crypto->clk);
    if (ret == 0) {
      ret = prv_aes_burst(setting, key, key_len, &out[offset], &in[offset], burst);
      pbl_clk_disable(&s_crypto->clk);
    }
    pbl_irq_unlock();

    if (ret != 0) {
      break;
    }

    done += burst;
  }

  return done;
}

size_t pbl_crypto_hw_sha256_blocks(uint32_t state[8], const void *data, size_t nblocks) {
  return pbl_device_is_ready(&s_crypto->dev) ? prv_sha256(state, data, nblocks) : 0;
}

size_t pbl_crypto_hw_aes_ecb_encrypt(const uint8_t *key, size_t key_len, uint8_t *out,
                                     const uint8_t *in, size_t nblocks) {
  return pbl_device_is_ready(&s_crypto->dev) ? prv_aes(true, key, key_len, out, in, nblocks) : 0;
}

size_t pbl_crypto_hw_aes_ecb_decrypt(const uint8_t *key, size_t key_len, uint8_t *out,
                                     const uint8_t *in, size_t nblocks) {
  return pbl_device_is_ready(&s_crypto->dev) ? prv_aes(false, key, key_len, out, in, nblocks) : 0;
}

#include PBL_DT_INSTANCES(sifli_sf32lb52_aes)
