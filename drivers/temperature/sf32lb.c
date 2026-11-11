/* SPDX-FileCopyrightText: 2025 SiFli Technologies(Nanjing) Co., Ltd */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/devicetree.h>
#include <pbl/drivers/temperature.h>
#include <pbl/drivers/temperature/sf32lb.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_hpsys_cfg.h>
#include <hardware/sf32lb52_tsen.h>

#define CONVERSION_TIMEOUT_US 20000U

#define SLOPE_NUM (2971) // approximate slope molecule
#define SLOPE_DEN (40)   // approximate slope denominator
#define OFFSET    (277539)
#define ROUND_ADD (SLOPE_DEN / 2)

static const struct pbl_temp_sf32lb *prv_tsen(const struct pbl_temp *temp) {
  return container_of(temp, const struct pbl_temp_sf32lb, temp);
}

static inline uint32_t prv_getreg(const struct pbl_temp_sf32lb *tsen, uint32_t offset) {
  return pbl_readl(tsen->base + offset);
}

static inline void prv_putreg(const struct pbl_temp_sf32lb *tsen, uint32_t offset, uint32_t value) {
  pbl_writel(value, tsen->base + offset);
}

static int prv_init(const struct pbl_temp *temp) {
  pbl_modifyl(0U, HPSYS_CFG_ANAU_CR_EN_BG, prv_tsen(temp)->cfg + SF32LB52_HPSYS_CFG_ANAU_CR_OFFSET);

  return 0;
}

static void prv_tsen_enable(const struct pbl_temp_sf32lb *tsen) {
  uint32_t ctrl = prv_getreg(tsen, SF32LB52_TSEN_CTRL_OFFSET);

  ctrl &= ~TSEN_CTRL_RSTB;
  prv_putreg(tsen, SF32LB52_TSEN_CTRL_OFFSET, ctrl);
  ctrl |= TSEN_CTRL_EN | TSEN_CTRL_PU;
  prv_putreg(tsen, SF32LB52_TSEN_CTRL_OFFSET, ctrl);
  ctrl |= TSEN_CTRL_RSTB;
  prv_putreg(tsen, SF32LB52_TSEN_CTRL_OFFSET, ctrl);
  pbl_busy_wait_us(20);
  ctrl |= TSEN_CTRL_RUN;
  prv_putreg(tsen, SF32LB52_TSEN_CTRL_OFFSET, ctrl);
}

static void prv_tsen_disable(const struct pbl_temp_sf32lb *tsen) {
  uint32_t ctrl = prv_getreg(tsen, SF32LB52_TSEN_CTRL_OFFSET);

  prv_putreg(tsen, SF32LB52_TSEN_CTRL_OFFSET, ctrl & ~(TSEN_CTRL_EN | TSEN_CTRL_PU));
}

static int prv_read(const struct pbl_temp *temp, int32_t *millicelsius) {
  const struct pbl_temp_sf32lb *tsen = prv_tsen(temp);
  int ret;

  ret = pbl_clk_enable(&tsen->clk);
  if (ret != 0) {
    return ret;
  }

  prv_tsen_enable(tsen);

  ret = PBL_POLL_BUSY_US((prv_getreg(tsen, SF32LB52_TSEN_IRQ_OFFSET) & TSEN_IRQ_IRSR) != 0U, 1,
                         CONVERSION_TIMEOUT_US);
  if (ret == 0) {
    // The Celsius conversion formula is: (DATA + 3000)/10100 * 749.2916 − 277.5391
    // In order to calculate milli-Celsius degrees, we can perform the following conversions:
    // COEF_NUM/COEF_DEN ≃ 749.2916/10100 × 1000
    // (DATA+3000)*COEF_NUM/COEF_DEN - OFFSET
    uint32_t raw = prv_getreg(tsen, SF32LB52_TSEN_RDATA_OFFSET) & TSEN_RDATA_MASK;
    uint32_t num = (raw + 3000U) * SLOPE_NUM + ROUND_ADD;
    *millicelsius = (int32_t)(num / SLOPE_DEN) - OFFSET;
  }

  prv_putreg(tsen, SF32LB52_TSEN_IRQ_OFFSET,
             prv_getreg(tsen, SF32LB52_TSEN_IRQ_OFFSET) | TSEN_IRQ_ICR);
  prv_tsen_disable(tsen);
  pbl_clk_disable(&tsen->clk);

  return ret;
}

const struct pbl_temp_ops pbl_temp_sf32lb_ops = {
  .init = prv_init,
  .read = prv_read,
};

#include PBL_DT_INSTANCES(sifli_sf32lb52_tsen)
