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
#include <hardware/sf32lb52_hpsys_rcc.h>
#include <hardware/sf32lb52_lpsys_aon.h>
#include <hardware/sf32lb52_lpsys_rcc.h>
#include <hardware/sf32lb52_mailbox.h>
#include <hardware/sf32lb52_pmuc.h>
#include <lpsys.h>

PBL_LOG_MODULE_DEFINE(clk_sf32lb52_pmuc, CONFIG_CLK_SF32LB52_PMUC_LOG_LEVEL);

#define LRC32_HZ 32000U
// At about 32 kHz, it starts within a few periods
#define LRC32_READY_US 1000U
#define HRC48_HZ       48000000U

// HRC48 calibration: HXT48 cycles per gate, trim search and the count differences that end it
#define HRC48_CAL_LENGTH     0x3fffU
#define HRC48_CAL_TIMEOUT_US 2000U
#define HRC48_CAL_STEPS      11U
#define HRC48_TRIM_START     0x200U
#define HRC48_SETTLE_US      3U
#define HRC48_CAL_DONE_DIFF  64U
#define HRC48_CAL_MAX_DIFF   160U

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

struct pbl_clk_sf32lb52_pmuc {
  struct pbl_clk_controller ctrl;
  uintptr_t base;
  uintptr_t rcc;
  // LRC10 calibration: the counter of the Bluetooth controller, LPSYS and the lock shared with
  // the LCPU
  uintptr_t bt_mac;
  uintptr_t lpsys_aon;
  uintptr_t lpsys_rcc;
  uintptr_t lock;
  struct pbl_clk hxt48;
  struct pbl_clk lxt32;
  const struct pbl_clk_assigned *assigned;
};

static const struct pbl_clk_cal_ops s_cal_ops;

static struct pbl_clk_cal s_cal = PBL_CLK_CAL_INIT(
    &s_cal_ops, &s_params, PBL_CLK(SF32LB52_PMUC, SF32LB52_CLK_LRC10), GATE_CYCLES, PERIOD_MS);

static const struct pbl_clk_sf32lb52_pmuc *s_pmuc;
static bool s_hxt48_requested;

static inline uint32_t prv_result(void) {
  return pbl_readl(s_pmuc->bt_mac + SF32LB52_BT_MAC_RCCAL_RESULT_OFFSET);
}

static inline void prv_acr_modify(uint32_t clear, uint32_t set) {
  pbl_modifyl(clear, set, s_pmuc->lpsys_aon + SF32LB52_LPSYS_AON_ACR_OFFSET);
}

static inline void prv_unlock(void) {
  pbl_writel(MAILBOX_CxEXR_EX, s_pmuc->lock + SF32LB52_MAILBOX_C1EXR_OFFSET);
}

// Takes LPSYS, the counter and the reference clock. Returns 0, -EBUSY if the LCPU holds the
// counter, or another negative errno.
static int prv_acquire(void) {
  uintptr_t csr_addr = s_pmuc->lpsys_rcc + SF32LB52_LPSYS_RCC_CSR_OFFSET;
  int ret;

  soc_sf32lb_lpsys_wake();

  // Reading the register takes the mutex if it is free.
  if ((pbl_readl(s_pmuc->lock + SF32LB52_MAILBOX_C1EXR_OFFSET) & MAILBOX_CxEXR_EX) == 0U) {
    soc_sf32lb_lpsys_release();
    return -EBUSY;
  }

  s_hxt48_requested = (pbl_readl(s_pmuc->lpsys_aon + SF32LB52_LPSYS_AON_ACR_OFFSET) &
                       LPSYS_AON_ACR_HXT48_REQ) == 0U;
  if (s_hxt48_requested) {
    prv_acr_modify(0U, LPSYS_AON_ACR_HXT48_REQ);
  }
  ret = PBL_POLL_BUSY_US((pbl_readl(s_pmuc->lpsys_aon + SF32LB52_LPSYS_AON_ACR_OFFSET) &
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
  soc_sf32lb_lpsys_release();
  return ret;
}

static void prv_release(void) {
  if (s_hxt48_requested) {
    prv_acr_modify(LPSYS_AON_ACR_HXT48_REQ, 0U);
  }
  prv_unlock();
  soc_sf32lb_lpsys_release();
}

static int prv_start_gate(uint32_t cycles) {
  uintptr_t ctrl_addr = s_pmuc->bt_mac + SF32LB52_BT_MAC_RCCAL_CTRL_OFFSET;
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
  const uintptr_t base = prv_pmuc(ctrl)->base;

  switch (id) {
    case SF32LB52_CLK_HXT48_DLL:
      pbl_modifyl(0U, PMUC_HXT_CR1_BUF_DLL_EN, base + SF32LB52_PMUC_HXT_CR1_OFFSET);
      return 0;
    case SF32LB52_CLK_LRC32:
      pbl_modifyl(0U, PMUC_LRC32_CR_EN, base + SF32LB52_PMUC_LRC32_CR_OFFSET);
      return PBL_POLL_BUSY_US(
          (pbl_readl(base + SF32LB52_PMUC_LRC32_CR_OFFSET) & PMUC_LRC32_CR_RDY) != 0U, 10U,
          LRC32_READY_US);
    default:
      return 0;
  }
}

static int prv_disable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  const uintptr_t base = prv_pmuc(ctrl)->base;

  switch (id) {
    case SF32LB52_CLK_HXT48_DLL:
      pbl_modifyl(PMUC_HXT_CR1_BUF_DLL_EN, 0U, base + SF32LB52_PMUC_HXT_CR1_OFFSET);
      return 0;
    case SF32LB52_CLK_LRC32:
      pbl_modifyl(PMUC_LRC32_CR_EN, 0U, base + SF32LB52_PMUC_LRC32_CR_OFFSET);
      return 0;
    default:
      return 0;
  }
}

static int prv_set_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                          const struct pbl_clk *parent) {
  uint32_t sel;

  if (id != SF32LB52_CLK_WDT || parent->ctrl != ctrl) {
    return -EINVAL;
  }

  switch (parent->id) {
    case SF32LB52_CLK_LRC10:
      sel = 0U;
      break;
    case SF32LB52_CLK_LRC32:
      sel = PMUC_CR_SEL_LPCLK;
      break;
    default:
      return -EINVAL;
  }

  pbl_modifyl(PMUC_CR_SEL_LPCLK, sel, prv_pmuc(ctrl)->base + SF32LB52_PMUC_CR_OFFSET);

  return 0;
}

// Counts HXT48 and HRC48 cycles over the same gate, with the counter of the HPSYS RCC
static int prv_hrc48_count(const struct pbl_clk_sf32lb52_pmuc *pmuc, uint32_t *hxt, uint32_t *hrc) {
  const uintptr_t cal1 = pmuc->rcc + SF32LB52_HPSYS_RCC_HRCCAL1_OFFSET;
  uint32_t cal2;
  int ret;

  pbl_modifyl(0U, HPSYS_RCC_HRCCAL1_CAL_EN, cal1);
  ret = PBL_POLL_BUSY_US((pbl_readl(cal1) & HPSYS_RCC_HRCCAL1_CAL_DONE) != 0U, 1U,
                         HRC48_CAL_TIMEOUT_US);
  cal2 = pbl_readl(pmuc->rcc + SF32LB52_HPSYS_RCC_HRCCAL2_OFFSET);
  pbl_modifyl(HPSYS_RCC_HRCCAL1_CAL_EN, 0U, cal1);

  *hxt = PBL_FIELD_GET(HPSYS_RCC_HRCCAL2_HXT_CNT_MASK, cal2);
  *hrc = PBL_FIELD_GET(HPSYS_RCC_HRCCAL2_HRC_CNT_MASK, cal2);

  return ret;
}

// Trims HRC48 to HXT48 by binary search, as the boot code of the vendor does
static int prv_hrc48_calibrate(const struct pbl_clk_sf32lb52_pmuc *pmuc) {
  const uintptr_t hrc_cr = pmuc->base + SF32LB52_PMUC_HRC_CR_OFFSET;
  uint32_t trim = HRC48_TRIM_START;
  uint32_t step = HRC48_TRIM_START / 2U;
  uint32_t diff = UINT32_MAX;
  uint32_t hxt;
  uint32_t hrc;
  int ret;

  pbl_modifyl(HPSYS_RCC_HRCCAL1_CAL_LENGTH_MASK,
              PBL_FIELD_PREP(HPSYS_RCC_HRCCAL1_CAL_LENGTH_MASK, HRC48_CAL_LENGTH),
              pmuc->rcc + SF32LB52_HPSYS_RCC_HRCCAL1_OFFSET);
  pbl_modifyl(PMUC_HRC_CR_CLKHP_SEL_MASK | PMUC_HRC_CR_CLKLP_SEL_MASK,
              PMUC_HRC_CR_CLKHP_SEL_MASK | PMUC_HRC_CR_CLKLP_SEL_MASK, hrc_cr);

  for (uint32_t i = 0U; i < HRC48_CAL_STEPS && step > 0U; i++, step >>= 1) {
    pbl_modifyl(PMUC_HRC_CR_FREQ_TRIM_MASK, PBL_FIELD_PREP(PMUC_HRC_CR_FREQ_TRIM_MASK, trim),
                hrc_cr);
    pbl_busy_wait_us(HRC48_SETTLE_US);

    ret = prv_hrc48_count(pmuc, &hxt, &hrc);
    if (ret != 0) {
      return ret;
    }

    if (hxt > hrc) {
      trim += step;
      diff = hxt - hrc;
    } else {
      trim -= step;
      diff = hrc - hxt;
    }

    if (diff < HRC48_CAL_DONE_DIFF) {
      break;
    }
  }

  return (diff <= HRC48_CAL_MAX_DIFF) ? 0 : -EIO;
}

static int prv_init(const struct pbl_device *dev) {
  const struct pbl_clk_sf32lb52_pmuc *pmuc =
      container_of(dev, const struct pbl_clk_sf32lb52_pmuc, ctrl.dev);
  int ret;

  s_pmuc = pmuc;

  ret = prv_hrc48_calibrate(pmuc);
  if (ret != 0) {
    PBL_LOG_ERR("HRC48 calibration failed (%d)", ret);
    return ret;
  }

  // The unit itself runs from it
  ret = pbl_clk_enable(&(struct pbl_clk)PBL_CLK(&pmuc->ctrl, SF32LB52_CLK_WDT));
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_assign(pmuc->assigned);
  if (ret != 0) {
    return ret;
  }

  // Gates fail until the LCPU has moved LPSYS to the crystal, some seconds into boot.
  pbl_clk_cal_start(&s_cal, NOMINAL_HZ);

  return 0;
}

static const struct pbl_clk_controller_ops s_ops = {
  .enable = prv_enable,
  .disable = prv_disable,
  .get_rate = prv_get_rate,
  .get_parent = prv_get_parent,
  .set_parent = prv_set_parent,
};

#define PBL_CLK_SF32LB52_PMUC_DEFINE(sym, _name, _base, _rcc, _bt_mac, _lpsys_aon, _lpsys_rcc, \
                                     _lock, _hxt48, _lxt32, ...)                               \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, SF32LB52_PMUC_NUM_CLKS);                                \
  PBL_CLK_ASSIGNED_DEFINE(sym##_assigned, __VA_ARGS__);                                        \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_sf32lb52_pmuc, struct pbl_clk_controller, ctrl) {    \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, prv_init, nullptr, &s_ops), .base = (_base),   \
    .rcc = (_rcc), .bt_mac = (_bt_mac), .lpsys_aon = (_lpsys_aon), .lpsys_rcc = (_lpsys_rcc),  \
    .lock = (_lock), .hxt48 = _hxt48, .lxt32 = _lxt32, .assigned = sym##_assigned,             \
  }

#include PBL_DT_INSTANCES(sifli_sf32lb52_pmuc_clk)
