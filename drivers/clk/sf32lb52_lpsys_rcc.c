/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_lpsys_rcc.h>
#include <lpsys.h>

#define HDIV_MAX       63U
#define PDIV_MAX       7U
#define PCLK_MAX_HZ    24000000U
#define MILLIHZ_PER_HZ 1000U

struct pbl_clk_sf32lb52_lpsys_rcc {
  struct pbl_clk_controller ctrl;
  uintptr_t base;
  struct pbl_clk hrc48;
  struct pbl_clk hxt48;
  const struct pbl_clk_assigned *assigned;
};

static const struct pbl_clk_sf32lb52_lpsys_rcc *prv_rcc(const struct pbl_clk_controller *ctrl) {
  return container_of(ctrl, const struct pbl_clk_sf32lb52_lpsys_rcc, ctrl);
}

// LPSYS registers only answer while it is awake
static uint32_t prv_read(const struct pbl_clk_controller *ctrl, uint32_t offset) {
  uint32_t val;

  soc_sf32lb_lpsys_wake();
  val = pbl_readl(prv_rcc(ctrl)->base + offset);
  soc_sf32lb_lpsys_release();

  return val;
}

static void prv_modify(const struct pbl_clk_controller *ctrl, uint32_t offset, uint32_t clear,
                       uint32_t set) {
  soc_sf32lb_lpsys_wake();
  pbl_modifyl(clear, set, prv_rcc(ctrl)->base + offset);
  soc_sf32lb_lpsys_release();
}

static int prv_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t parent_mhz,
                        uint64_t *rate_mhz) {
  uint32_t cfgr = prv_read(ctrl, SF32LB52_LPSYS_RCC_CFGR_OFFSET);

  switch (id) {
    case SF32LB52_LPSYS_CLK_HCLK:
      *rate_mhz = parent_mhz / MAX(PBL_FIELD_GET(LPSYS_RCC_CFGR_HDIV1_MASK, cfgr), 1U);
      break;
    case SF32LB52_LPSYS_CLK_PCLK:
      *rate_mhz = parent_mhz >> PBL_FIELD_GET(LPSYS_RCC_CFGR_PDIV1_MASK, cfgr);
      break;
    case SF32LB52_LPSYS_CLK_PCLK2:
      *rate_mhz = parent_mhz >> PBL_FIELD_GET(LPSYS_RCC_CFGR_PDIV2_MASK, cfgr);
      break;
    default:
      *rate_mhz = parent_mhz;
      break;
  }

  return *rate_mhz != 0U ? 0 : -ENODEV;
}

static int prv_get_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                          struct pbl_clk *parent) {
  const struct pbl_clk_sf32lb52_lpsys_rcc *rcc = prv_rcc(ctrl);
  uint32_t csr = prv_read(ctrl, SF32LB52_LPSYS_RCC_CSR_OFFSET);

  switch (id) {
    case SF32LB52_LPSYS_CLK_SYS:
      if ((csr & LPSYS_RCC_CSR_SEL_SYS_LP) != 0U) {
        return -ENOENT;
      }
      *parent = PBL_FIELD_GET(LPSYS_RCC_CSR_SEL_SYS_MASK, csr) == LPSYS_RCC_CSR_SEL_SYS_HXT48
                    ? rcc->hxt48
                    : rcc->hrc48;
      return 0;
    case SF32LB52_LPSYS_CLK_HCLK:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, SF32LB52_LPSYS_CLK_SYS);
      return 0;
    case SF32LB52_LPSYS_CLK_PCLK:
    case SF32LB52_LPSYS_CLK_PCLK2:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, SF32LB52_LPSYS_CLK_HCLK);
      return 0;
    case SF32LB52_LPSYS_CLK_PERI:
      *parent = (csr & LPSYS_RCC_CSR_SEL_PERI) != 0U ? rcc->hxt48 : rcc->hrc48;
      return 0;
    default:
      return -ENOENT;
  }
}

static bool prv_same(const struct pbl_clk *a, const struct pbl_clk *b) {
  return a->ctrl == b->ctrl && a->id == b->id;
}

static int prv_set_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                          const struct pbl_clk *parent) {
  const struct pbl_clk_sf32lb52_lpsys_rcc *rcc = prv_rcc(ctrl);
  bool hxt48;

  if (prv_same(parent, &rcc->hxt48)) {
    hxt48 = true;
  } else if (prv_same(parent, &rcc->hrc48)) {
    hxt48 = false;
  } else {
    return -EINVAL;
  }

  switch (id) {
    case SF32LB52_LPSYS_CLK_SYS:
      prv_modify(ctrl, SF32LB52_LPSYS_RCC_CSR_OFFSET,
                 LPSYS_RCC_CSR_SEL_SYS_MASK | LPSYS_RCC_CSR_SEL_SYS_LP,
                 PBL_FIELD_PREP(LPSYS_RCC_CSR_SEL_SYS_MASK,
                                hxt48 ? LPSYS_RCC_CSR_SEL_SYS_HXT48 : LPSYS_RCC_CSR_SEL_SYS_HRC48));
      return 0;
    case SF32LB52_LPSYS_CLK_PERI:
      prv_modify(ctrl, SF32LB52_LPSYS_RCC_CSR_OFFSET, LPSYS_RCC_CSR_SEL_PERI,
                 hxt48 ? LPSYS_RCC_CSR_SEL_PERI : 0U);
      return 0;
    default:
      return -EINVAL;
  }
}

static int prv_set_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t rate_mhz,
                        uint64_t parent_mhz) {
  uint32_t div;
  uint32_t pdiv;
  uint32_t mask;

  if (rate_mhz == 0U || parent_mhz % rate_mhz != 0U) {
    return -EINVAL;
  }
  div = (uint32_t)(parent_mhz / rate_mhz);

  if (id == SF32LB52_LPSYS_CLK_HCLK) {
    if (div > HDIV_MAX) {
      return -EINVAL;
    }

    // PCLK stays within its limit at the new rate
    pdiv = PBL_FIELD_GET(LPSYS_RCC_CFGR_PDIV1_MASK, prv_read(ctrl, SF32LB52_LPSYS_RCC_CFGR_OFFSET));
    while ((rate_mhz >> pdiv) > (uint64_t)PCLK_MAX_HZ * MILLIHZ_PER_HZ && pdiv < PDIV_MAX) {
      pdiv++;
    }

    prv_modify(ctrl, SF32LB52_LPSYS_RCC_CFGR_OFFSET,
               LPSYS_RCC_CFGR_HDIV1_MASK | LPSYS_RCC_CFGR_PDIV1_MASK,
               PBL_FIELD_PREP(LPSYS_RCC_CFGR_HDIV1_MASK, div) |
                   PBL_FIELD_PREP(LPSYS_RCC_CFGR_PDIV1_MASK, pdiv));
    return 0;
  }

  if (id == SF32LB52_LPSYS_CLK_PCLK) {
    mask = LPSYS_RCC_CFGR_PDIV1_MASK;
    if (rate_mhz > (uint64_t)PCLK_MAX_HZ * MILLIHZ_PER_HZ) {
      return -EINVAL;
    }
  } else if (id == SF32LB52_LPSYS_CLK_PCLK2) {
    mask = LPSYS_RCC_CFGR_PDIV2_MASK;
  } else {
    return -EINVAL;
  }

  pdiv = (uint32_t)__builtin_ctz(div);
  if ((div & (div - 1U)) != 0U || pdiv > PDIV_MAX) {
    return -EINVAL;
  }

  prv_modify(ctrl, SF32LB52_LPSYS_RCC_CFGR_OFFSET, mask, PBL_FIELD_PREP(mask, pdiv));

  return 0;
}

static int prv_init(const struct pbl_device *dev) {
  const struct pbl_clk_sf32lb52_lpsys_rcc *rcc =
      container_of(dev, const struct pbl_clk_sf32lb52_lpsys_rcc, ctrl.dev);

  return pbl_clk_assign(rcc->assigned);
}

static const struct pbl_clk_controller_ops s_ops = {
  .get_rate = prv_get_rate,
  .get_parent = prv_get_parent,
  .set_parent = prv_set_parent,
  .set_rate = prv_set_rate,
};

#define PBL_CLK_SF32LB52_LPSYS_RCC_DEFINE(sym, _name, _base, _hrc48, _hxt48, ...)                \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, SF32LB52_LPSYS_NUM_CLKS);                                 \
  PBL_CLK_ASSIGNED_DEFINE(sym##_assigned, __VA_ARGS__);                                          \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_sf32lb52_lpsys_rcc, struct pbl_clk_controller, ctrl) { \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, prv_init, nullptr, &s_ops), .base = (_base),     \
    .hrc48 = _hrc48, .hxt48 = _hxt48, .assigned = sym##_assigned,                                \
  }

#include PBL_DT_INSTANCES(sifli_sf32lb52_lpsys_rcc)
