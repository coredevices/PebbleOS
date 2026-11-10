/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/cal.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_bt_mac.h>
#include <hardware/sf32lb52_lpsys_aon.h>
#include <hardware/sf32lb52_lpsys_rcc.h>
#include <hardware/sf32lb52_mailbox.h>
#include <hardware/sf32lb52_pmuc.h>
#include <lpsys.h>

PBL_LOG_MODULE_DEFINE(clk_sf32lb52_pmuc, CONFIG_CLK_SF32LB52_PMUC_LOG_LEVEL);

#define LRC32_HZ 32000U
#define HRC48_HZ 48000000U

// The RCCAL counter of the Bluetooth controller counts LPSYS clock cycles, from the 48 MHz
// crystal, over a number of LRC10 cycles. The LCPU calibrates on its own with the same counter,
// under a mailbox mutex, and never uses this estimate.
#define REF_HZ 48000000.0

#define GATE_CYCLES CONFIG_CLK_SF32LB52_PMUC_LRC10_CAL_GATE_CYCLES
#define PERIOD_MS   (CONFIG_CLK_SF32LB52_PMUC_LRC10_CAL_PERIOD_S * 1000U)

// Only until the first gate, whatever the real rate is.
#define NOMINAL_HZ 10000.0

#define HXT48_TIMEOUT_US 5000U
#define DONE_TIMEOUT_US  2000U

// Measured on a watch; see the RC10K page of the clock driver documentation.
static const struct pbl_freq_track_params s_params = {
  .wfm = 7.2e-6,
  .wander = 8e-6,
  .rw = 1.1e-6,
  .reject_sigma = 5.0,
  .max_rejects = 3U,
};

static const struct pbl_clk_cal_ops s_cal_ops;

static struct pbl_clk_cal s_cal = PBL_CLK_CAL_INIT(
    &s_cal_ops, &s_params, PBL_CLK(SF32LB52_PMUC, SF32LB52_CLK_LRC10), GATE_CYCLES, PERIOD_MS);

static bool s_hxt48_requested;

static inline uint32_t prv_result(void) {
  return pbl_readl(SF32LB52_BT_MAC_BASE + SF32LB52_BT_MAC_RCCAL_RESULT_OFFSET);
}

static inline void prv_acr_modify(uint32_t clear, uint32_t set) {
  pbl_modifyl(clear, set, SF32LB52_LPSYS_AON_BASE + SF32LB52_LPSYS_AON_ACR_OFFSET);
}

static inline void prv_unlock(void) {
  pbl_writel(MAILBOX_CxEXR_EX, SF32LB52_MAILBOX2_BASE + SF32LB52_MAILBOX_C1EXR_OFFSET);
}

// Takes LPSYS, the counter and the reference clock. Returns 0, -EBUSY if the LCPU holds the
// counter, or another negative errno.
static int prv_acquire(void) {
  uintptr_t csr_addr = SF32LB52_LPSYS_RCC_BASE + SF32LB52_LPSYS_RCC_CSR_OFFSET;
  int ret;

  soc_sf32lb52_lpsys_wake();

  // Reading the register takes the mutex if it is free.
  if ((pbl_readl(SF32LB52_MAILBOX2_BASE + SF32LB52_MAILBOX_C1EXR_OFFSET) & MAILBOX_CxEXR_EX) ==
      0U) {
    soc_sf32lb52_lpsys_release();
    return -EBUSY;
  }

  s_hxt48_requested = (pbl_readl(SF32LB52_LPSYS_AON_BASE + SF32LB52_LPSYS_AON_ACR_OFFSET) &
                       LPSYS_AON_ACR_HXT48_REQ) == 0U;
  if (s_hxt48_requested) {
    prv_acr_modify(0U, LPSYS_AON_ACR_HXT48_REQ);
  }
  ret = PBL_POLL_BUSY_US((pbl_readl(SF32LB52_LPSYS_AON_BASE + SF32LB52_LPSYS_AON_ACR_OFFSET) &
                          LPSYS_AON_ACR_HXT48_RDY) != 0U,
                         10U, HXT48_TIMEOUT_US);
  if (ret == 0) {
    // The LCPU may not be running. Not restored: it may start during the gate and needs this too.
    if (PBL_FIELD_GET(LPSYS_RCC_CSR_SEL_SYS_MASK, pbl_readl(csr_addr)) !=
        LPSYS_RCC_CSR_SEL_SYS_HXT48) {
      pbl_modifyl(LPSYS_RCC_CSR_SEL_SYS_MASK,
                  PBL_FIELD_PREP(LPSYS_RCC_CSR_SEL_SYS_MASK, LPSYS_RCC_CSR_SEL_SYS_HXT48),
                  csr_addr);
    }
    return 0;
  }

  if (s_hxt48_requested) {
    prv_acr_modify(LPSYS_AON_ACR_HXT48_REQ, 0U);
  }
  prv_unlock();
  soc_sf32lb52_lpsys_release();
  return ret;
}

static void prv_release(void) {
  if (s_hxt48_requested) {
    prv_acr_modify(LPSYS_AON_ACR_HXT48_REQ, 0U);
  }
  prv_unlock();
  soc_sf32lb52_lpsys_release();
}

static int prv_start_gate(uint32_t cycles) {
  uintptr_t ctrl_addr = SF32LB52_BT_MAC_BASE + SF32LB52_BT_MAC_RCCAL_CTRL_OFFSET;
  uint32_t ctrl = pbl_readl(ctrl_addr);

  ctrl &= ~(BT_MAC_RCCAL_CTRL_LENGTH_MASK | BT_MAC_RCCAL_CTRL_AUTO | BT_MAC_RCCAL_CTRL_STOP |
            BT_MAC_RCCAL_CTRL_CON_NUM_MASK | BT_MAC_RCCAL_CTRL_CON_MODE);
  ctrl |= PBL_FIELD_PREP(BT_MAC_RCCAL_CTRL_LENGTH_MASK, cycles) |
          PBL_FIELD_PREP(BT_MAC_RCCAL_CTRL_CON_NUM_MASK, 1U) | BT_MAC_RCCAL_CTRL_CON_MODE;
  pbl_writel(ctrl, ctrl_addr);
  pbl_writel(ctrl | BT_MAC_RCCAL_CTRL_START, ctrl_addr);

  // DONE stays set from the previous gate, possibly the LCPU's, until this one starts.
  return PBL_POLL_BUSY_US((prv_result() & BT_MAC_RCCAL_RESULT_DONE) == 0U, 0U, DONE_TIMEOUT_US);
}

static int prv_cal_finish(struct pbl_clk_cal *cal, uint32_t cycles, double *hz) {
  uint32_t count;
  int ret;

  ret = PBL_POLL_BUSY_US((prv_result() & BT_MAC_RCCAL_RESULT_DONE) != 0U, 10U, DONE_TIMEOUT_US);
  count = PBL_FIELD_GET(BT_MAC_RCCAL_RESULT_RESULT_MASK, prv_result());
  prv_release();
  if (ret != 0) {
    return ret;
  }

  if (count == 0U) {
    return -EIO;
  }

  *hz = REF_HZ * cycles / count;

  return 0;
}

static int prv_cal_start(struct pbl_clk_cal *cal, uint32_t cycles) {
  int ret;

  if (cycles == 0U || cycles > PBL_FIELD_GET(BT_MAC_RCCAL_CTRL_LENGTH_MASK, UINT32_MAX)) {
    return -EINVAL;
  }

  ret = prv_acquire();
  if (ret != 0) {
    return ret;
  }

  ret = prv_start_gate(cycles);
  if (ret != 0) {
    prv_release();
  }

  return ret;
}

static const struct pbl_clk_cal_ops s_cal_ops = {
  .start = prv_cal_start,
  .finish = prv_cal_finish,
};

struct pbl_clk_sf32lb52_pmuc {
  struct pbl_clk_controller ctrl;
  uintptr_t base;
  struct pbl_clk hxt48;
  struct pbl_clk lxt32;
};

static const struct pbl_clk_sf32lb52_pmuc *prv_pmuc(const struct pbl_clk_controller *ctrl) {
  return container_of(ctrl, const struct pbl_clk_sf32lb52_pmuc, ctrl);
}

static int prv_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t parent_mhz,
                        uint64_t *rate_mhz) {
  switch (id) {
    case SF32LB52_CLK_LRC10:
      *rate_mhz = pbl_clk_cal_get_rate(&s_cal);
      return 0;
    case SF32LB52_CLK_LRC32:
      *rate_mhz = LRC32_HZ * 1000ULL;
      return 0;
    case SF32LB52_CLK_HRC48:
      *rate_mhz = HRC48_HZ * 1000ULL;
      return 0;
    case SF32LB52_CLK_LXT32:
    case SF32LB52_CLK_HXT48:
    case SF32LB52_CLK_HXT48_DLL:
    case SF32LB52_CLK_WDT:
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
  const struct pbl_clk_sf32lb52_pmuc *pmuc = prv_pmuc(ctrl);

  switch (id) {
    case SF32LB52_CLK_LXT32:
      if (pmuc->lxt32.ctrl == nullptr) {
        return -ENOENT;
      }
      *parent = pmuc->lxt32;
      return 0;
    case SF32LB52_CLK_HXT48:
      *parent = pmuc->hxt48;
      return 0;
    case SF32LB52_CLK_HXT48_DLL:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, SF32LB52_CLK_HXT48);
      return 0;
    case SF32LB52_CLK_WDT:
      *parent = (struct pbl_clk)PBL_CLK(
          ctrl, (pbl_readl(pmuc->base + SF32LB52_PMUC_CR_OFFSET) & PMUC_CR_SEL_LPCLK) != 0U
                    ? SF32LB52_CLK_LRC32
                    : SF32LB52_CLK_LRC10);
      return 0;
    default:
      return -ENOENT;
  }
}

static int prv_enable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  if (id == SF32LB52_CLK_HXT48_DLL) {
    pbl_modifyl(0U, PMUC_HXT_CR1_BUF_DLL_EN, prv_pmuc(ctrl)->base + SF32LB52_PMUC_HXT_CR1_OFFSET);
  }

  return 0;
}

static int prv_disable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  if (id == SF32LB52_CLK_HXT48_DLL) {
    pbl_modifyl(PMUC_HXT_CR1_BUF_DLL_EN, 0U, prv_pmuc(ctrl)->base + SF32LB52_PMUC_HXT_CR1_OFFSET);
  }

  return 0;
}

static int prv_init(const struct pbl_device *dev) {
  // Gates fail until the LCPU has moved LPSYS to the crystal, some seconds into boot.
  pbl_clk_cal_start(&s_cal, NOMINAL_HZ);

  return 0;
}

static const struct pbl_clk_controller_ops s_ops = {
  .enable = prv_enable,
  .disable = prv_disable,
  .get_rate = prv_get_rate,
  .get_parent = prv_get_parent,
};

#define PBL_CLK_SF32LB52_PMUC_DEFINE(sym, _name, _base, _hxt48, _lxt32)                      \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, SF32LB52_PMUC_NUM_CLKS);                              \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_sf32lb52_pmuc, struct pbl_clk_controller, ctrl) {  \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, prv_init, nullptr, &s_ops), .base = (_base), \
    .hxt48 = _hxt48, .lxt32 = _lxt32,                                                        \
  }

#include PBL_DT_INSTANCES(sifli_sf32lb52_pmuc_clk)
