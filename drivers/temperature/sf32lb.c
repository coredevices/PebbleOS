/* SPDX-FileCopyrightText: 2025 SiFli Technologies(Nanjing) Co., Ltd */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/temperature.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>

#include <hardware/sf32lb52_hpsys_cfg.h>
#include <hardware/sf32lb52_tsen.h>

#define CONVERSION_TIMEOUT_US 20000U

#define SLOPE_NUM (2971) // approximate slope molecule
#define SLOPE_DEN (40)   // approximate slope denominator
#define OFFSET    (277539)
#define ROUND_ADD (SLOPE_DEN / 2)

static const struct pbl_clk s_clk = PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_TSEN);

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(SF32LB52_TSEN_BASE + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, SF32LB52_TSEN_BASE + offset);
}

void temperature_init(void) {
  pbl_modifyl(0U, HPSYS_CFG_ANAU_CR_EN_BG,
              SF32LB52_HPSYS_CFG_BASE + SF32LB52_HPSYS_CFG_ANAU_CR_OFFSET);
}

static void prv_tsen_enable(void) {
  uint32_t ctrl = prv_getreg(SF32LB52_TSEN_CTRL_OFFSET);

  ctrl &= ~TSEN_CTRL_RSTB;
  prv_putreg(SF32LB52_TSEN_CTRL_OFFSET, ctrl);
  ctrl |= TSEN_CTRL_EN | TSEN_CTRL_PU;
  prv_putreg(SF32LB52_TSEN_CTRL_OFFSET, ctrl);
  ctrl |= TSEN_CTRL_RSTB;
  prv_putreg(SF32LB52_TSEN_CTRL_OFFSET, ctrl);
  pbl_busy_wait_us(20);
  ctrl |= TSEN_CTRL_RUN;
  prv_putreg(SF32LB52_TSEN_CTRL_OFFSET, ctrl);
}

static void prv_tsen_disable(void) {
  uint32_t ctrl = prv_getreg(SF32LB52_TSEN_CTRL_OFFSET);

  prv_putreg(SF32LB52_TSEN_CTRL_OFFSET, ctrl & ~(TSEN_CTRL_EN | TSEN_CTRL_PU));
}

int32_t temperature_read(void) {
  int32_t temp = INT32_MIN;

  if (pbl_clk_enable(&s_clk) != 0) {
    return INT32_MIN;
  }

  prv_tsen_enable();

  if (PBL_POLL_BUSY_US((prv_getreg(SF32LB52_TSEN_IRQ_OFFSET) & TSEN_IRQ_IRSR) != 0U, 1,
                       CONVERSION_TIMEOUT_US) == 0) {
    // The Celsius conversion formula is: (DATA + 3000)/10100 * 749.2916 − 277.5391
    // In order to calculate milli-Celsius degrees, we can perform the following conversions:
    // COEF_NUM/COEF_DEN ≃ 749.2916/10100 × 1000
    // (DATA+3000)*COEF_NUM/COEF_DEN - OFFSET
    uint32_t raw = prv_getreg(SF32LB52_TSEN_RDATA_OFFSET) & TSEN_RDATA_MASK;
    uint32_t num = (raw + 3000U) * SLOPE_NUM + ROUND_ADD;
    temp = (int32_t)(num / SLOPE_DEN) - OFFSET;
  }

  prv_putreg(SF32LB52_TSEN_IRQ_OFFSET, prv_getreg(SF32LB52_TSEN_IRQ_OFFSET) | TSEN_IRQ_ICR);
  prv_tsen_disable();
  pbl_clk_disable(&s_clk);

  return temp;
}
