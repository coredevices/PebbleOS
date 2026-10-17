/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/rng.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/logging/logging.h>

#include <hardware/sf32lb52_trng.h>

PBL_LOG_MODULE_DEFINE(driver_rng_sf32lb, CONFIG_DRIVER_RNG_LOG_LEVEL);

#define TRNG_TIMEOUT_US 3000U

static const struct pbl_clk s_clk = PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_TRNG);
static bool s_inited;

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(SF32LB52_TRNG_BASE + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, SF32LB52_TRNG_BASE + offset);
}

static int prv_generate(uint32_t cmd, uint32_t valid, uint32_t offset, uint32_t *out) {
  prv_putreg(SF32LB52_TRNG_CTRL_OFFSET, cmd);

  int ret =
      PBL_POLL_BUSY_US((prv_getreg(SF32LB52_TRNG_STAT_OFFSET) & valid) != 0U, 1U, TRNG_TIMEOUT_US);
  if (ret < 0) {
    return ret;
  }

  *out = prv_getreg(offset);

  return 0;
}

bool rng_rand(uint32_t *rand_out) {
  int ret;

  if (!s_inited) {
    PBL_ASSERTN(pbl_device_init(&s_clk.ctrl->dev) == 0);
  }

  ret = pbl_clk_enable(&s_clk);
  if (ret != 0) {
    return false;
  }

  if (!s_inited) {
    uint32_t val = prv_getreg(SF32LB52_TRNG_CTRL_OFFSET);
    prv_putreg(SF32LB52_TRNG_CTRL_OFFSET,
               val & ~(TRNG_CTRL_GEN_SEED_STOP | TRNG_CTRL_GEN_RAND_NUM_STOP));

    ret = prv_generate(TRNG_CTRL_GEN_SEED_START, TRNG_STAT_SEED_VALID,
                       SF32LB52_TRNG_RAND_SEED_OFFSET(0), rand_out);
    PBL_ASSERTN(ret == 0);

    s_inited = true;
  }

  ret = prv_generate(TRNG_CTRL_GEN_RAND_NUM_START, TRNG_STAT_RAND_NUM_VALID,
                     SF32LB52_TRNG_RAND_NUM_OFFSET(0), rand_out);
  if (ret != 0) {
    PBL_LOG_ERR("Random number generation failed: %d", ret);
  }

  pbl_clk_disable(&s_clk);

  return ret == 0;
}
