/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/mfd/nrf_cracen.h>
#include <pbl/drivers/rng.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>

#include <hardware/nrf54l_cracen.h>

PBL_LOG_MODULE_DEFINE(driver_rng_nrf_cracen, CONFIG_DRIVER_RNG_LOG_LEVEL);

#define INIT_WAIT_CYCLES 512U
#define NB128BITBLOCKS   4U

// The first words of each run key the conditioning function
#define KEY_WORDS   4U
#define CACHE_WORDS 8U

// Covers the start-up health tests, which take 33 ms on the nRF54LM20A
#define TIMEOUT_US 100000U
#define ATTEMPTS   3U

static const struct pbl_nrf_cracen *const s_cracen = &pbl_nrf_cracen_inst;

static uint32_t s_cache[CACHE_WORDS];
static uint32_t s_count;

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(s_cracen->core + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, s_cracen->core + offset);
}

static int prv_wait_words(uint32_t words) {
  uint32_t state;
  int ret;

  ret = PBL_POLL_BUSY_US(((state = PBL_FIELD_GET(CRACEN_RNG_STATUS_STATE_MASK,
                                                 prv_getreg(NRF54L_CRACEN_RNG_STATUS_OFFSET))) ==
                          CRACEN_RNG_STATUS_STATE_ERROR) ||
                             ((state != CRACEN_RNG_STATUS_STATE_RESET) &&
                              (prv_getreg(NRF54L_CRACEN_RNG_FIFOLEVEL_OFFSET) >= words)),
                         1U, TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  return state == CRACEN_RNG_STATUS_STATE_ERROR ? -EIO : 0;
}

static int prv_fill(void) {
  int ret;

  prv_putreg(NRF54L_CRACEN_RNG_CONTROL_OFFSET, CRACEN_RNG_CONTROL_SOFTRST);
#ifdef CONFIG_SOC_NRF54L15
  // The generator of the nRF54LM20A has none of these, and warms up for as long out of reset
  prv_putreg(NRF54L_CRACEN_RNG_SWOFFTMRVAL_OFFSET, 0U);
  prv_putreg(NRF54L_CRACEN_RNG_CLKDIV_OFFSET, 0U);
  prv_putreg(NRF54L_CRACEN_RNG_INITWAITVAL_OFFSET, INIT_WAIT_CYCLES);
#endif
  prv_putreg(NRF54L_CRACEN_RNG_CONTROL_OFFSET,
             CRACEN_RNG_CONTROL_ENABLE |
                 PBL_FIELD_PREP(CRACEN_RNG_CONTROL_NB128BITBLOCKS_MASK, NB128BITBLOCKS));

  ret = prv_wait_words(KEY_WORDS);
  if (ret != 0) {
    return ret;
  }

  for (uint32_t i = 0U; i < KEY_WORDS; i++) {
    prv_putreg(NRF54L_CRACEN_RNG_KEY_OFFSET(i), prv_getreg(NRF54L_CRACEN_RNG_FIFO_OFFSET));
  }

  ret = prv_wait_words(CACHE_WORDS);
  if (ret != 0) {
    return ret;
  }

  for (uint32_t i = 0U; i < CACHE_WORDS; i++) {
    s_cache[i] = prv_getreg(NRF54L_CRACEN_RNG_FIFO_OFFSET);
  }
  s_count = CACHE_WORDS;

  return 0;
}

int rng_rand(uint32_t *rand_out) {
  int ret = 0;

  PBL_ASSERTN(pbl_device_is_ready(&s_cracen->dev));

  pbl_irq_lock();

  if (s_count == 0U) {
    ret = -EIO;
    pbl_nrf_cracen_enable(s_cracen, CRACEN_ENABLE_RNG);
    for (uint32_t attempt = 0U; attempt < ATTEMPTS && ret != 0; attempt++) {
      ret = prv_fill();
    }
    pbl_nrf_cracen_disable(s_cracen, CRACEN_ENABLE_RNG);
  }

  if (ret == 0) {
    s_count--;
    *rand_out = s_cache[s_count];
    s_cache[s_count] = 0U;
  }

  pbl_irq_unlock();

  if (ret != 0) {
    PBL_LOG_ERR("Random number generation failed: %d", ret);
  }

  return ret;
}
