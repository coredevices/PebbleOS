/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/device.h>
#include <pbl/drivers/crypto.h>
#include <pbl/drivers/mfd/nrf_cracen.h>
#include <pbl/kernel/barrier.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/byteorder.h>

#include <hardware/nrf54l_cracen.h>

#define SHA256_BLOCK_SIZE 64U
#define SHA256_STATE_SIZE 32U
#define AES_BLOCK_SIZE    16U
#define AES_KEY_MAX       32U

// Bytes processed with interrupts locked: four SHA-256 blocks, or eight AES blocks in and out
#define BURST_LEN 256U

#define SHA256_BURST_BLOCKS (BURST_LEN / SHA256_BLOCK_SIZE)
#define AES_BURST_BLOCKS    (BURST_LEN / 2U / AES_BLOCK_SIZE)
#define AES_OUT_OFFSET      (BURST_LEN / 2U)

#define TIMEOUT_US 1000U

#define DMA_BUSY \
  (CRACEN_DMA_STATUS_FETCHBUSY | CRACEN_DMA_STATUS_PUSHBUSY | CRACEN_DMA_STATUS_PUSHWAITINGFIFO)

#define AES_TAG(x)  (CRACEN_DMA_TAG_ENGINE_AES | (x))
#define HASH_TAG(x) (CRACEN_DMA_TAG_ENGINE_HASH | (x))

static const struct pbl_nrf_cracen *const s_cracen = &pbl_nrf_cracen_inst;

// The engine masters the bus: what it reads and writes goes through RAM, used with interrupts
// locked.
static uint8_t s_buf[BURST_LEN] PBL_ALIGNED(4);
static uint8_t s_key[AES_KEY_MAX] PBL_ALIGNED(4);
static uint8_t s_state[SHA256_STATE_SIZE] PBL_ALIGNED(4);
static uint32_t s_config;
static struct nrf54l_cracen_dma_desc s_in[3];
static struct nrf54l_cracen_dma_desc s_out;

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(s_cracen->core + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, s_cracen->core + offset);
}

static void prv_desc(struct nrf54l_cracen_dma_desc *desc, const void *addr, size_t len,
                     uint32_t tag, const struct nrf54l_cracen_dma_desc *next) {
  desc->addr = addr;
  desc->next = next;
  desc->len = (uint32_t)len | CRACEN_DMA_LEN_REALIGN;
  desc->tag = tag;
}

static int prv_run(void) {
  int ret;

  pbl_nrf_cracen_enable(s_cracen, CRACEN_ENABLE_CRYPTOMASTER);

  prv_putreg(NRF54L_CRACEN_DMA_INTSTATCLR_OFFSET, UINT32_MAX);
  prv_putreg(NRF54L_CRACEN_DMA_FETCHADDRLSB_OFFSET, (uint32_t)(uintptr_t)s_in);
  prv_putreg(NRF54L_CRACEN_DMA_PUSHADDRLSB_OFFSET, (uint32_t)(uintptr_t)&s_out);
  prv_putreg(NRF54L_CRACEN_DMA_CONFIG_OFFSET,
             CRACEN_DMA_CONFIG_FETCHCTRLINDIRECT | CRACEN_DMA_CONFIG_PUSHCTRLINDIRECT);
  pbl_wmb();
  prv_putreg(NRF54L_CRACEN_DMA_START_OFFSET, CRACEN_DMA_START_FETCH | CRACEN_DMA_START_PUSH);

  ret = PBL_POLL_BUSY_US((prv_getreg(NRF54L_CRACEN_DMA_STATUS_OFFSET) & DMA_BUSY) == 0U, 0,
                         TIMEOUT_US);
  if ((ret == 0) && ((prv_getreg(NRF54L_CRACEN_DMA_INTSTATRAW_OFFSET) &
                      (CRACEN_DMA_INT_FETCH_ERROR | CRACEN_DMA_INT_PUSH_ERROR)) != 0U)) {
    ret = -EIO;
  }
  pbl_rmb();

  prv_putreg(NRF54L_CRACEN_DMA_CONFIG_OFFSET, CRACEN_DMA_CONFIG_SOFTRST);
  prv_putreg(NRF54L_CRACEN_DMA_CONFIG_OFFSET, 0U);
  (void)PBL_POLL_BUSY_US(
      (prv_getreg(NRF54L_CRACEN_DMA_STATUS_OFFSET) & CRACEN_DMA_STATUS_SOFTRSTBUSY) == 0U, 0,
      TIMEOUT_US);

  pbl_nrf_cracen_disable(s_cracen, CRACEN_ENABLE_CRYPTOMASTER);

  return ret;
}

static int prv_sha256_burst(uint32_t state[8], const uint8_t *data, size_t nblocks) {
  const size_t len = nblocks * SHA256_BLOCK_SIZE;
  int ret;

  s_config = CRACEN_HASH_CONFIG_SHA256;
  for (unsigned int i = 0; i < 8U; i++) {
    pbl_put_be32(state[i], &s_state[4U * i]);
  }
  memcpy(s_buf, data, len);

  prv_desc(&s_in[0], &s_config, sizeof(s_config), HASH_TAG(CRACEN_DMA_TAG_CONFIG), &s_in[1]);
  prv_desc(&s_in[1], s_state, sizeof(s_state),
           HASH_TAG(CRACEN_DMA_TAG_DATATYPE(CRACEN_HASH_DATATYPE_STATE) | CRACEN_DMA_TAG_LAST),
           &s_in[2]);
  prv_desc(&s_in[2], s_buf, len,
           HASH_TAG(CRACEN_DMA_TAG_DATATYPE(CRACEN_HASH_DATATYPE_MESSAGE) | CRACEN_DMA_TAG_LAST),
           CRACEN_DMA_DESC_STOP);
  prv_desc(&s_out, s_state, sizeof(s_state), CRACEN_DMA_TAG_LAST, CRACEN_DMA_DESC_STOP);

  ret = prv_run();
  if (ret != 0) {
    return ret;
  }

  for (unsigned int i = 0; i < 8U; i++) {
    state[i] = pbl_get_be32(&s_state[4U * i]);
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
    ret = prv_sha256_burst(state, &data[done * SHA256_BLOCK_SIZE], burst);
    pbl_irq_unlock();

    if (ret != 0) {
      break;
    }

    done += burst;
  }

  return done;
}

static int prv_aes_burst(uint32_t config, const uint8_t *key, size_t key_len, uint8_t *out,
                         const uint8_t *in, size_t nblocks) {
  const size_t len = nblocks * AES_BLOCK_SIZE;
  int ret;

  s_config = config;
  memcpy(s_key, key, key_len);
  memcpy(s_buf, in, len);

  prv_desc(&s_in[0], &s_config, sizeof(s_config),
           AES_TAG(CRACEN_DMA_TAG_CONFIG | CRACEN_DMA_TAG_OFFSET(CRACEN_AES_REG_CONFIG)), &s_in[1]);
  prv_desc(&s_in[1], s_key, key_len,
           AES_TAG(CRACEN_DMA_TAG_CONFIG | CRACEN_DMA_TAG_OFFSET(CRACEN_AES_REG_KEY)), &s_in[2]);
  prv_desc(&s_in[2], s_buf, len, AES_TAG(CRACEN_DMA_TAG_LAST), CRACEN_DMA_DESC_STOP);
  prv_desc(&s_out, &s_buf[AES_OUT_OFFSET], len, CRACEN_DMA_TAG_LAST, CRACEN_DMA_DESC_STOP);

  ret = prv_run();
  if (ret == 0) {
    memcpy(out, &s_buf[AES_OUT_OFFSET], len);
  }

  memset(s_key, 0, sizeof(s_key));

  return ret;
}

static size_t prv_aes(bool encrypt, const uint8_t *key, size_t key_len, uint8_t *out,
                      const uint8_t *in, size_t nblocks) {
  const uint32_t config = CRACEN_AES_CONFIG_MODE_ECB | (encrypt ? 0U : CRACEN_AES_CONFIG_DECRYPT);
  size_t done = 0;

  if ((key_len != 16U) && (key_len != 24U) && (key_len != 32U)) {
    return 0;
  }

  while (done < nblocks) {
    const size_t burst = (nblocks - done < AES_BURST_BLOCKS) ? (nblocks - done) : AES_BURST_BLOCKS;
    const size_t offset = done * AES_BLOCK_SIZE;
    int ret;

    pbl_irq_lock();
    ret = prv_aes_burst(config, key, key_len, &out[offset], &in[offset], burst);
    pbl_irq_unlock();

    if (ret != 0) {
      break;
    }

    done += burst;
  }

  return done;
}

static bool prv_ready(void) {
  return pbl_device_is_ready(&s_cracen->dev);
}

size_t pbl_crypto_hw_sha256_blocks(uint32_t state[8], const void *data, size_t nblocks) {
  return prv_ready() ? prv_sha256(state, data, nblocks) : 0;
}

size_t pbl_crypto_hw_aes_ecb_encrypt(const uint8_t *key, size_t key_len, uint8_t *out,
                                     const uint8_t *in, size_t nblocks) {
  return prv_ready() ? prv_aes(true, key, key_len, out, in, nblocks) : 0;
}

size_t pbl_crypto_hw_aes_ecb_decrypt(const uint8_t *key, size_t key_len, uint8_t *out,
                                     const uint8_t *in, size_t nblocks) {
  return prv_ready() ? prv_aes(false, key, key_len, out, in, nblocks) : 0;
}
