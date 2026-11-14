/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/cal.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/sf32lb/sleep.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb57_hpsys_rcc.h>
#include <hardware/sf32lb57_pmuc.h>

// Nominal, until the first gate
#define LRC10_HZ 10000U
#define LRC32_HZ 32000U
#define HRC48_HZ 48000000U
// At about 32 kHz, it starts within a few periods
#define LRC32_READY_US 1000U

// The RC10CAL counter of the HPSYS RCC counts 48 MHz crystal cycles over a number of LRC10 cycles
#define REF_HZ          48000000.0
#define GATE_CYCLES     CONFIG_CLK_SF32LB57_PMUC_LRC10_CAL_GATE_CYCLES
#define PERIOD_MS       (CONFIG_CLK_SF32LB57_PMUC_LRC10_CAL_PERIOD_S * 1000U)
#define DONE_TIMEOUT_US 2000U

// The SF32LB52 LRC10 model; see the RC10K page of the clock driver documentation.
static const struct pbl_freq_track_params s_params = {
  .wfm = 7.2e-6,
  .wander = 8e-6,
  .rw = 1.1e-6,
  .reject_sigma = 5.0,
  .max_rejects = 3U,
};

struct pbl_clk_sf32lb57_pmuc {
  struct pbl_clk_controller ctrl;
  uintptr_t base;
  // HPSYS RCC, whose counter measures LRC10
  uintptr_t rcc;
  struct pbl_clk hxt48;
  struct pbl_clk lxt32;
  const struct pbl_clk_assigned *assigned;
};

static const struct pbl_clk_cal_ops s_cal_ops;

static struct pbl_clk_cal s_cal = PBL_CLK_CAL_INIT(
    &s_cal_ops, &s_params, PBL_CLK(SF32LB57_PMUC, SF32LB57_CLK_LRC10), GATE_CYCLES, PERIOD_MS);

static uintptr_t s_rcc;

static const struct pbl_clk_sf32lb57_pmuc *prv_pmuc(const struct pbl_clk_controller *ctrl) {
  return container_of(ctrl, const struct pbl_clk_sf32lb57_pmuc, ctrl);
}

static inline uint32_t prv_cal_result(void) {
  return pbl_readl(s_rcc + SF32LB57_HPSYS_RCC_RC10CAL2_OFFSET);
}

static int prv_cal_start(struct pbl_clk_cal *cal, uint32_t cycles) {
  uint32_t ctrl;
  int ret;

  if (cycles == 0U || cycles > PBL_FIELD_GET(HPSYS_RCC_RC10CAL1_CAL_LENGTH_MASK, UINT32_MAX)) {
    return -EINVAL;
  }

  // The crystal must keep counting through the gate
  soc_sf32lb_sleep_block(SOC_SF32LB_DEEPWFI);

  // One gate, on LRC10 (CAL_SEL clear)
  ctrl = PBL_FIELD_PREP(HPSYS_RCC_RC10CAL1_CAL_LENGTH_MASK, cycles) |
         PBL_FIELD_PREP(HPSYS_RCC_RC10CAL1_CON_NUM_MASK, 1U) | HPSYS_RCC_RC10CAL1_CON_MODE;
  pbl_writel(ctrl, s_rcc + SF32LB57_HPSYS_RCC_RC10CAL1_OFFSET);
  pbl_writel(ctrl | HPSYS_RCC_RC10CAL1_CAL_START, s_rcc + SF32LB57_HPSYS_RCC_RC10CAL1_OFFSET);

  // DONE stays set from the previous gate until this one starts
  ret =
      PBL_POLL_BUSY_US((prv_cal_result() & HPSYS_RCC_RC10CAL2_CAL_DONE) == 0U, 0U, DONE_TIMEOUT_US);
  if (ret != 0) {
    soc_sf32lb_sleep_release(SOC_SF32LB_DEEPWFI);
  }

  return ret;
}

static int prv_cal_finish(struct pbl_clk_cal *cal, uint32_t cycles, double *hz) {
  uint32_t count;
  int ret;

  ret = PBL_POLL_BUSY_US((prv_cal_result() & HPSYS_RCC_RC10CAL2_CAL_DONE) != 0U, 10U,
                         DONE_TIMEOUT_US);
  count = PBL_FIELD_GET(HPSYS_RCC_RC10CAL2_CAL_RSLT_MASK, prv_cal_result());
  soc_sf32lb_sleep_release(SOC_SF32LB_DEEPWFI);
  if (ret != 0) {
    return ret;
  }

  if (count == 0U) {
    return -EIO;
  }

  *hz = REF_HZ * cycles / count;

  return 0;
}

static const struct pbl_clk_cal_ops s_cal_ops = {
  .start = prv_cal_start,
  .finish = prv_cal_finish,
};

static uint32_t prv_read(const struct pbl_clk_controller *ctrl, uint32_t offset) {
  return pbl_readl(prv_pmuc(ctrl)->base + offset);
}

static int prv_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t parent_mhz,
                        uint64_t *rate_mhz) {
  switch (id) {
    case SF32LB57_CLK_LRC10:
      *rate_mhz = pbl_clk_cal_get_rate(&s_cal);
      return 0;
    case SF32LB57_CLK_LRC32:
      *rate_mhz = LRC32_HZ * 1000ULL;
      return 0;
    case SF32LB57_CLK_HRC48:
      *rate_mhz = HRC48_HZ * 1000ULL;
      return 0;
    case SF32LB57_CLK_LXT32:
    case SF32LB57_CLK_HXT48:
    case SF32LB57_CLK_HXT48_DLL:
    case SF32LB57_CLK_WDT:
    case SF32LB57_CLK_RTC:
      if (parent_mhz == 0U) {
        return -ENODEV;
      }
      *rate_mhz = parent_mhz;
      return 0;
    default:
      return -EINVAL;
  }
}

static int prv_get_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                          struct pbl_clk *parent) {
  const struct pbl_clk_sf32lb57_pmuc *pmuc = prv_pmuc(ctrl);
  uint32_t cr;

  switch (id) {
    case SF32LB57_CLK_LXT32:
      if (pmuc->lxt32.ctrl == nullptr) {
        return -ENOENT;
      }
      *parent = pmuc->lxt32;
      return 0;
    case SF32LB57_CLK_HXT48:
      *parent = pmuc->hxt48;
      return 0;
    case SF32LB57_CLK_HXT48_DLL:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, SF32LB57_CLK_HXT48);
      return 0;
    case SF32LB57_CLK_WDT:
      cr = prv_read(ctrl, SF32LB57_PMUC_CR_OFFSET);
      *parent = (struct pbl_clk)PBL_CLK(
          ctrl, (cr & PMUC_CR_SEL_WDT) != 0U ? SF32LB57_CLK_LRC32 : SF32LB57_CLK_LRC10);
      return 0;
    case SF32LB57_CLK_RTC:
      cr = prv_read(ctrl, SF32LB57_PMUC_CR_OFFSET);
      *parent = (struct pbl_clk)PBL_CLK(
          ctrl, (cr & PMUC_CR_SEL_RTC) != 0U ? SF32LB57_CLK_LXT32 : SF32LB57_CLK_LRC10);
      return 0;
    default:
      return -ENOENT;
  }
}

static int prv_enable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  const uintptr_t base = prv_pmuc(ctrl)->base;

  switch (id) {
    case SF32LB57_CLK_HXT48_DLL:
      pbl_modifyl(0U, PMUC_HXT_CR1_BUF_DLL_EN, base + SF32LB57_PMUC_HXT_CR1_OFFSET);
      return 0;
    case SF32LB57_CLK_LRC32:
      pbl_modifyl(0U, PMUC_LRC32_CR_EN, base + SF32LB57_PMUC_LRC32_CR_OFFSET);
      return PBL_POLL_BUSY_US(
          (pbl_readl(base + SF32LB57_PMUC_LRC32_CR_OFFSET) & PMUC_LRC32_CR_RDY) != 0U, 10U,
          LRC32_READY_US);
    case SF32LB57_CLK_LXT32:
      // Only once the boot ROM or the LCPU started it
      return (pbl_readl(base + SF32LB57_PMUC_LXT_CR_OFFSET) & PMUC_LXT_CR_RDY) != 0U ? 0 : -ENOTSUP;
    default:
      return 0;
  }
}

static int prv_disable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  const uintptr_t base = prv_pmuc(ctrl)->base;

  switch (id) {
    case SF32LB57_CLK_HXT48_DLL:
      pbl_modifyl(PMUC_HXT_CR1_BUF_DLL_EN, 0U, base + SF32LB57_PMUC_HXT_CR1_OFFSET);
      return 0;
    case SF32LB57_CLK_LRC32:
      pbl_modifyl(PMUC_LRC32_CR_EN, 0U, base + SF32LB57_PMUC_LRC32_CR_OFFSET);
      return 0;
    default:
      return 0;
  }
}

static int prv_set_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                          const struct pbl_clk *parent) {
  uint32_t mask;
  uint32_t sel;

  if (parent->ctrl != ctrl) {
    return -EINVAL;
  }

  switch (id) {
    case SF32LB57_CLK_WDT:
      mask = PMUC_CR_SEL_WDT;
      if (parent->id == SF32LB57_CLK_LRC10) {
        sel = 0U;
      } else if (parent->id == SF32LB57_CLK_LRC32) {
        sel = mask;
      } else {
        return -EINVAL;
      }
      break;
    case SF32LB57_CLK_RTC:
      mask = PMUC_CR_SEL_RTC;
      if (parent->id == SF32LB57_CLK_LRC10) {
        sel = 0U;
      } else if (parent->id == SF32LB57_CLK_LXT32) {
        sel = mask;
      } else {
        return -EINVAL;
      }
      break;
    default:
      return -EINVAL;
  }

  pbl_modifyl(mask, sel, prv_pmuc(ctrl)->base + SF32LB57_PMUC_CR_OFFSET);

  return 0;
}

static int prv_init(const struct pbl_device *dev) {
  const struct pbl_clk_sf32lb57_pmuc *pmuc =
      container_of(dev, const struct pbl_clk_sf32lb57_pmuc, ctrl.dev);
  int ret;

  // The unit itself runs from it
  ret = pbl_clk_enable(&(struct pbl_clk)PBL_CLK(&pmuc->ctrl, SF32LB57_CLK_WDT));
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_assign(pmuc->assigned);
  if (ret != 0) {
    return ret;
  }

  s_rcc = pmuc->rcc;
  pbl_clk_cal_start(&s_cal, LRC10_HZ);

  return 0;
}

static const struct pbl_clk_controller_ops s_ops = {
  .enable = prv_enable,
  .disable = prv_disable,
  .get_rate = prv_get_rate,
  .get_parent = prv_get_parent,
  .set_parent = prv_set_parent,
};

#define PBL_CLK_SF32LB57_PMUC_DEFINE(sym, _name, _base, _rcc, _hxt48, _lxt32, ...)           \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, SF32LB57_PMUC_NUM_CLKS);                              \
  PBL_CLK_ASSIGNED_DEFINE(sym##_assigned, __VA_ARGS__);                                      \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_sf32lb57_pmuc, struct pbl_clk_controller, ctrl) {  \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, prv_init, nullptr, &s_ops), .base = (_base), \
    .rcc = (_rcc), .hxt48 = _hxt48, .lxt32 = _lxt32, .assigned = sym##_assigned,             \
  }

#include PBL_DT_INSTANCES(sifli_sf32lb57_pmuc_clk)
