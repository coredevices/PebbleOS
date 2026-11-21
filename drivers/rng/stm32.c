/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/devicetree.h>
#include <pbl/drivers/rng.h>
#include <pbl/drivers/rng/stm32.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/logging/logging.h>

#include <hardware/stm32_rng.h>

PBL_LOG_MODULE_DEFINE(driver_rng, CONFIG_DRIVER_RNG_LOG_LEVEL);

// A word takes 42 RNG clock cycles once the generator runs, its first a few hundred
#define DRDY_TIMEOUT_US 1000U

// Words to read after a seed error, to empty the output pipeline
#define SEED_ERROR_FLUSH 12U

extern const struct pbl_rng_stm32 pbl_rng_stm32_inst;

static const struct pbl_rng_stm32 *const s_rng = &pbl_rng_stm32_inst;

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(s_rng->base + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, s_rng->base + offset);
}

// Restarts the generator after a seed error, the recovery the reference manual describes
static void prv_recover(void) {
  prv_putreg(STM32_RNG_SR_OFFSET, (uint32_t)~RNG_SR_SEIS);
  for (uint32_t i = 0U; i < SEED_ERROR_FLUSH; i++) {
    (void)prv_getreg(STM32_RNG_DR_OFFSET);
  }
  prv_putreg(STM32_RNG_CR_OFFSET, 0U);
  prv_putreg(STM32_RNG_CR_OFFSET, RNG_CR_RNGEN);
}

static int prv_read(uint32_t *out) {
  uint32_t sr;
  int ret;

  ret = PBL_POLL_BUSY_US(
      ((sr = prv_getreg(STM32_RNG_SR_OFFSET)) & (RNG_SR_DRDY | RNG_SR_SECS | RNG_SR_CECS)) != 0U,
      1U, DRDY_TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  if ((sr & (RNG_SR_SECS | RNG_SR_CECS)) != 0U) {
    if ((sr & RNG_SR_SECS) != 0U) {
      prv_recover();
    }
    prv_putreg(STM32_RNG_SR_OFFSET, (uint32_t)~RNG_SR_CEIS);
    return -EIO;
  }

  *out = prv_getreg(STM32_RNG_DR_OFFSET);

  // A zero word may be read while a seed error comes up
  return *out == 0U ? -EAGAIN : 0;
}

int pbl_rng_stm32_init(const struct pbl_device *dev) {
  int ret;

  ret = pbl_device_init(&s_rng->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&s_rng->clk);
  if (ret != 0) {
    return ret;
  }

  prv_putreg(STM32_RNG_CR_OFFSET, RNG_CR_RNGEN);

  return 0;
}

int rng_rand(uint32_t *rand_out) {
  int ret = -EIO;

  PBL_ASSERTN(pbl_device_is_ready(&s_rng->dev));

  // A seed error recovers on its own, after the restart
  for (uint32_t attempt = 0U; attempt < 3U && ret != 0; attempt++) {
    ret = prv_read(rand_out);
  }

  if (ret != 0) {
    PBL_LOG_ERR("Random number generation failed: %d", ret);
  }

  return ret;
}

#include PBL_DT_INSTANCES(st_stm32_rng)
