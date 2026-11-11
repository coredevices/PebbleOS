/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb57_pmuc.h>

#define LRC10_HZ 10000U
#define LRC32_HZ 32000U
#define HRC48_HZ 48000000U
// At about 32 kHz, it starts within a few periods
#define LRC32_READY_US 1000U

struct pbl_clk_sf32lb57_pmuc {
  struct pbl_clk_controller ctrl;
  uintptr_t base;
  struct pbl_clk hxt48;
  struct pbl_clk lxt32;
  const struct pbl_clk_assigned *assigned;
};

static const struct pbl_clk_sf32lb57_pmuc *prv_pmuc(const struct pbl_clk_controller *ctrl) {
  return container_of(ctrl, const struct pbl_clk_sf32lb57_pmuc, ctrl);
}

static uint32_t prv_read(const struct pbl_clk_controller *ctrl, uint32_t offset) {
  return pbl_readl(prv_pmuc(ctrl)->base + offset);
}

static int prv_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t parent_mhz,
                        uint64_t *rate_mhz) {
  switch (id) {
    case SF32LB57_CLK_LRC10:
      *rate_mhz = LRC10_HZ * 1000ULL;
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

  return pbl_clk_assign(pmuc->assigned);
}

static const struct pbl_clk_controller_ops s_ops = {
  .enable = prv_enable,
  .disable = prv_disable,
  .get_rate = prv_get_rate,
  .get_parent = prv_get_parent,
  .set_parent = prv_set_parent,
};

#define PBL_CLK_SF32LB57_PMUC_DEFINE(sym, _name, _base, _hxt48, _lxt32, ...)                 \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, SF32LB57_PMUC_NUM_CLKS);                              \
  PBL_CLK_ASSIGNED_DEFINE(sym##_assigned, __VA_ARGS__);                                      \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_sf32lb57_pmuc, struct pbl_clk_controller, ctrl) {  \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, prv_init, nullptr, &s_ops), .base = (_base), \
    .hxt48 = _hxt48, .lxt32 = _lxt32, .assigned = sym##_assigned,                            \
  }

#include PBL_DT_INSTANCES(sifli_sf32lb57_pmuc)
