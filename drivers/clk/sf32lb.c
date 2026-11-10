/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/power_domain.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_hpsys_cfg.h>
#include <hardware/sf32lb52_hpsys_rcc.h>

#define NUM_GATES 64U

#define DLL_STEP_HZ    24000000U
#define DLL_STG_MAX    15U
#define DLL_LOCK_US    10U
#define DLL_READY_US   1000U
#define HDIV_MAX       255U
#define PDIV_MAX       7U
#define PCLK_MAX_HZ    120000000U
#define PCLK2_MAX_HZ   7500000U
#define MILLIHZ_PER_HZ 1000U

// The CPU clock, which the busy waits count
extern uint32_t SystemCoreClock;

struct pbl_clk_sf32lb {
  struct pbl_clk_controller ctrl;
  uintptr_t base;
  uintptr_t cfg;
  struct pbl_clk hrc48;
  struct pbl_clk hxt48;
  struct pbl_clk hxt48_dll;
  struct pbl_power_domain pd;
  uint32_t pd_level;
  const struct pbl_clk_assigned *assigned;
};

// The clock each gate gates. The audio blocks pick their own kernel clock, HXT48 or the audio
// PLL, so their gates are on the bus clock.
static const uint8_t s_gate_parents[NUM_GATES] = {
  [SF32LB52_HPSYS_CLK_DMAC1] = SF32LB52_HPSYS_CLK_HCLK,
  [SF32LB52_HPSYS_CLK_MAILBOX1] = SF32LB52_HPSYS_CLK_PCLK,
  [SF32LB52_HPSYS_CLK_PINMUX1] = SF32LB52_HPSYS_CLK_PCLK,
  [SF32LB52_HPSYS_CLK_USART2] = SF32LB52_HPSYS_CLK_PERI,
  [SF32LB52_HPSYS_CLK_EZIP1] = SF32LB52_HPSYS_CLK_HCLK,
  [SF32LB52_HPSYS_CLK_EPIC] = SF32LB52_HPSYS_CLK_HCLK,
  [SF32LB52_HPSYS_CLK_LCDC1] = SF32LB52_HPSYS_CLK_HCLK,
  [SF32LB52_HPSYS_CLK_I2S1] = SF32LB52_HPSYS_CLK_PCLK,
  [SF32LB52_HPSYS_CLK_SYSCFG1] = SF32LB52_HPSYS_CLK_PCLK,
  [SF32LB52_HPSYS_CLK_EFUSEC] = SF32LB52_HPSYS_CLK_PCLK,
  [SF32LB52_HPSYS_CLK_AES] = SF32LB52_HPSYS_CLK_HCLK,
  [SF32LB52_HPSYS_CLK_CRC1] = SF32LB52_HPSYS_CLK_HCLK,
  [SF32LB52_HPSYS_CLK_TRNG] = SF32LB52_HPSYS_CLK_PCLK,
  [SF32LB52_HPSYS_CLK_GPTIM1] = SF32LB52_HPSYS_CLK_PCLK,
  [SF32LB52_HPSYS_CLK_GPTIM2] = SF32LB52_HPSYS_CLK_PERI_DIV2,
  [SF32LB52_HPSYS_CLK_BTIM1] = SF32LB52_HPSYS_CLK_PCLK,
  [SF32LB52_HPSYS_CLK_BTIM2] = SF32LB52_HPSYS_CLK_PERI_DIV2,
  [SF32LB52_HPSYS_CLK_SPI1] = SF32LB52_HPSYS_CLK_PERI,
  [SF32LB52_HPSYS_CLK_SPI2] = SF32LB52_HPSYS_CLK_PERI,
  [SF32LB52_HPSYS_CLK_EXTDMA] = SF32LB52_HPSYS_CLK_HCLK,
  [SF32LB52_HPSYS_CLK_SECU1] = SF32LB52_HPSYS_CLK_HCLK,
  [SF32LB52_HPSYS_CLK_PDM1] = SF32LB52_HPSYS_CLK_PCLK,
  [SF32LB52_HPSYS_CLK_I2C1] = SF32LB52_HPSYS_CLK_PERI,
  [SF32LB52_HPSYS_CLK_I2C2] = SF32LB52_HPSYS_CLK_PERI,
  [SF32LB52_HPSYS_CLK_PTC1] = SF32LB52_HPSYS_CLK_HCLK,
  [SF32LB52_HPSYS_CLK_GPIO1] = SF32LB52_HPSYS_CLK_HCLK,
  [SF32LB52_HPSYS_CLK_MPI1] = SF32LB52_HPSYS_CLK_MPI1_K,
  [SF32LB52_HPSYS_CLK_MPI2] = SF32LB52_HPSYS_CLK_MPI2_K,
  [SF32LB52_HPSYS_CLK_SDMMC1] = SF32LB52_HPSYS_CLK_HCLK,
  [SF32LB52_HPSYS_CLK_USBC] = SF32LB52_HPSYS_CLK_USBC_K,
  [SF32LB52_HPSYS_CLK_I2C3] = SF32LB52_HPSYS_CLK_PERI,
  [SF32LB52_HPSYS_CLK_ATIM1] = SF32LB52_HPSYS_CLK_PCLK,
  [SF32LB52_HPSYS_CLK_USART3] = SF32LB52_HPSYS_CLK_PERI,
  [SF32LB52_HPSYS_CLK_AUDCODEC] = SF32LB52_HPSYS_CLK_PCLK,
  [SF32LB52_HPSYS_CLK_AUDPRC] = SF32LB52_HPSYS_CLK_PCLK,
  [SF32LB52_HPSYS_CLK_GPADC] = SF32LB52_HPSYS_CLK_PCLK,
  [SF32LB52_HPSYS_CLK_TSEN] = SF32LB52_HPSYS_CLK_PCLK,
  [SF32LB52_HPSYS_CLK_I2C4] = SF32LB52_HPSYS_CLK_PERI,
};

static const struct pbl_clk_sf32lb *prv_rcc(const struct pbl_clk_controller *ctrl) {
  return container_of(ctrl, const struct pbl_clk_sf32lb, ctrl);
}

static uint32_t prv_read(const struct pbl_clk_controller *ctrl, uint32_t offset) {
  return pbl_readl(prv_rcc(ctrl)->base + offset);
}

static inline uintptr_t prv_reg(uintptr_t base, uint32_t offset, uint32_t id) {
  return base + offset + ((id >> 5) << 2);
}

static void prv_write(const struct pbl_clk_controller *ctrl, uint32_t offset, uint32_t val) {
  pbl_writel(val, prv_rcc(ctrl)->base + offset);
}

static void prv_modify(const struct pbl_clk_controller *ctrl, uint32_t offset, uint32_t clear,
                       uint32_t set) {
  pbl_modifyl(clear, set, prv_rcc(ctrl)->base + offset);
}

static uint32_t prv_dll_offset(uint32_t id) {
  return (id == SF32LB52_HPSYS_CLK_DLL1) ? SF32LB52_HPSYS_RCC_DLL1CR_OFFSET
                                         : SF32LB52_HPSYS_RCC_DLL2CR_OFFSET;
}

static bool prv_is_dll(uint32_t id) {
  return id == SF32LB52_HPSYS_CLK_DLL1 || id == SF32LB52_HPSYS_CLK_DLL2;
}

static bool prv_sys_on_dll1(uint32_t csr) {
  return (csr & HPSYS_RCC_CSR_SEL_SYS_LP) == 0U &&
         PBL_FIELD_GET(HPSYS_RCC_CSR_SEL_SYS_MASK, csr) == HPSYS_RCC_CSR_SEL_SYS_DLL1;
}

// Whether a kernel clock of the MPI or USB controllers runs from a DLL
static bool prv_dll_feeds_peripheral(const struct pbl_clk_controller *ctrl, uint32_t id) {
  uint32_t csr = prv_read(ctrl, SF32LB52_HPSYS_RCC_CSR_OFFSET);
  uint32_t sel =
      (id == SF32LB52_HPSYS_CLK_DLL1) ? HPSYS_RCC_CSR_SEL_MPI_DLL1 : HPSYS_RCC_CSR_SEL_MPI_DLL2;

  return PBL_FIELD_GET(HPSYS_RCC_CSR_SEL_MPI1_MASK, csr) == sel ||
         PBL_FIELD_GET(HPSYS_RCC_CSR_SEL_MPI2_MASK, csr) == sel ||
         (id == SF32LB52_HPSYS_CLK_DLL2 && (csr & HPSYS_RCC_CSR_SEL_USBC) != 0U);
}

static int prv_dll_enable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  uint32_t offset = prv_dll_offset(id);
  uint32_t cr = prv_read(ctrl, offset);

  if ((cr & (HPSYS_RCC_DLLCR_EN | HPSYS_RCC_DLLCR_READY)) ==
      (HPSYS_RCC_DLLCR_EN | HPSYS_RCC_DLLCR_READY)) {
    return 0;
  }

  // The DLLs run from the HPSYS bandgap
  pbl_modifyl(0U, HPSYS_CFG_CAU2_CR_HPBG_VDDPSW_EN | HPSYS_CFG_CAU2_CR_HPBG_EN,
              prv_rcc(ctrl)->cfg + SF32LB52_HPSYS_CFG_CAU2_CR_OFFSET);

  cr &= ~(HPSYS_RCC_DLLCR_EN | HPSYS_RCC_DLLCR_OUT_DIV2_EN);
  cr |= HPSYS_RCC_DLLCR_IN_DIV2_EN;
  prv_write(ctrl, offset, cr);
  prv_write(ctrl, offset, cr | HPSYS_RCC_DLLCR_EN);

  pbl_busy_wait_us(DLL_LOCK_US);

  return PBL_POLL_BUSY_US((prv_read(ctrl, offset) & HPSYS_RCC_DLLCR_READY) != 0U, 1U, DLL_READY_US);
}

static int prv_dll_disable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  if ((id == SF32LB52_HPSYS_CLK_DLL1 &&
       prv_sys_on_dll1(prv_read(ctrl, SF32LB52_HPSYS_RCC_CSR_OFFSET))) ||
      prv_dll_feeds_peripheral(ctrl, id)) {
    return -EBUSY;
  }

  prv_modify(ctrl, prv_dll_offset(id), HPSYS_RCC_DLLCR_EN, 0U);

  return 0;
}

static int prv_clk_enable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  if (id < NUM_GATES) {
    pbl_writel(PBL_BIT(id & 31U), prv_reg(prv_rcc(ctrl)->base, SF32LB52_HPSYS_RCC_ESR1_OFFSET, id));
  } else if (prv_is_dll(id)) {
    return prv_dll_enable(ctrl, id);
  }

  return 0;
}

static int prv_clk_disable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  if (id < NUM_GATES) {
    pbl_writel(PBL_BIT(id & 31U), prv_reg(prv_rcc(ctrl)->base, SF32LB52_HPSYS_RCC_ECR1_OFFSET, id));
  } else if (prv_is_dll(id)) {
    return prv_dll_disable(ctrl, id);
  }

  return 0;
}

static int prv_dll_rate(const struct pbl_clk_controller *ctrl, uint32_t offset,
                        uint64_t *rate_mhz) {
  uint32_t cr = prv_read(ctrl, offset);

  if ((cr & HPSYS_RCC_DLLCR_EN) == 0U) {
    return -ENODEV;
  }

  *rate_mhz = (PBL_FIELD_GET(HPSYS_RCC_DLLCR_STG_MASK, cr) + 1ULL) * DLL_STEP_HZ * 1000U;

  return 0;
}

static int prv_clk_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t parent_mhz,
                            uint64_t *rate_mhz) {
  uint32_t cfgr = prv_read(ctrl, SF32LB52_HPSYS_RCC_CFGR_OFFSET);
  uint32_t div;

  switch (id) {
    case SF32LB52_HPSYS_CLK_DLL1:
      return prv_dll_rate(ctrl, SF32LB52_HPSYS_RCC_DLL1CR_OFFSET, rate_mhz);
    case SF32LB52_HPSYS_CLK_DLL2:
      return prv_dll_rate(ctrl, SF32LB52_HPSYS_RCC_DLL2CR_OFFSET, rate_mhz);
    case SF32LB52_HPSYS_CLK_HCLK:
      div = MAX(PBL_FIELD_GET(HPSYS_RCC_CFGR_HDIV_MASK, cfgr), 1U);
      *rate_mhz = parent_mhz / div;
      break;
    case SF32LB52_HPSYS_CLK_PCLK:
      *rate_mhz = parent_mhz >> PBL_FIELD_GET(HPSYS_RCC_CFGR_PDIV1_MASK, cfgr);
      break;
    case SF32LB52_HPSYS_CLK_PCLK2:
      *rate_mhz = parent_mhz >> PBL_FIELD_GET(HPSYS_RCC_CFGR_PDIV2_MASK, cfgr);
      break;
    case SF32LB52_HPSYS_CLK_PERI_DIV2:
      *rate_mhz = parent_mhz / 2U;
      break;
    case SF32LB52_HPSYS_CLK_USBC_K:
      div =
          PBL_FIELD_GET(HPSYS_RCC_USBCR_DIV_MASK, prv_read(ctrl, SF32LB52_HPSYS_RCC_USBCR_OFFSET));
      *rate_mhz = parent_mhz / MAX(div, 1U);
      break;
    default:
      *rate_mhz = parent_mhz;
      break;
  }

  return *rate_mhz != 0U ? 0 : -ENODEV;
}

static int prv_mpi_parent(const struct pbl_clk_controller *ctrl, uint32_t mask,
                          struct pbl_clk *parent) {
  switch (PBL_FIELD_GET(mask, prv_read(ctrl, SF32LB52_HPSYS_RCC_CSR_OFFSET))) {
    case HPSYS_RCC_CSR_SEL_MPI_PERI:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, SF32LB52_HPSYS_CLK_PERI);
      return 0;
    case HPSYS_RCC_CSR_SEL_MPI_DLL1:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, SF32LB52_HPSYS_CLK_DLL1);
      return 0;
    case HPSYS_RCC_CSR_SEL_MPI_DLL2:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, SF32LB52_HPSYS_CLK_DLL2);
      return 0;
    default:
      return -ENOENT;
  }
}

static int prv_sys_parent(const struct pbl_clk_controller *ctrl, uint32_t csr,
                          struct pbl_clk *parent) {
  const struct pbl_clk_sf32lb *rcc = prv_rcc(ctrl);

  if ((csr & HPSYS_RCC_CSR_SEL_SYS_LP) != 0U) {
    return -ENOENT;
  }

  switch (PBL_FIELD_GET(HPSYS_RCC_CSR_SEL_SYS_MASK, csr)) {
    case HPSYS_RCC_CSR_SEL_SYS_HRC48:
      *parent = rcc->hrc48;
      return 0;
    case HPSYS_RCC_CSR_SEL_SYS_HXT48:
      *parent = rcc->hxt48;
      return 0;
    case HPSYS_RCC_CSR_SEL_SYS_DLL1:
      *parent = (struct pbl_clk)PBL_CLK(ctrl, SF32LB52_HPSYS_CLK_DLL1);
      return 0;
    default:
      return -ENOENT;
  }
}

static int prv_clk_get_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                              struct pbl_clk *parent) {
  const struct pbl_clk_sf32lb *rcc = prv_rcc(ctrl);
  uint32_t csr = prv_read(ctrl, SF32LB52_HPSYS_RCC_CSR_OFFSET);
  uint32_t parent_id;

  switch (id) {
    case SF32LB52_HPSYS_CLK_SYS:
      return prv_sys_parent(ctrl, csr, parent);
    case SF32LB52_HPSYS_CLK_DLL1:
    case SF32LB52_HPSYS_CLK_DLL2:
      *parent = rcc->hxt48_dll;
      return 0;
    case SF32LB52_HPSYS_CLK_PERI:
      *parent = (csr & HPSYS_RCC_CSR_SEL_PERI) != 0U ? rcc->hxt48 : rcc->hrc48;
      return 0;
    case SF32LB52_HPSYS_CLK_MPI1_K:
      return prv_mpi_parent(ctrl, HPSYS_RCC_CSR_SEL_MPI1_MASK, parent);
    case SF32LB52_HPSYS_CLK_MPI2_K:
      return prv_mpi_parent(ctrl, HPSYS_RCC_CSR_SEL_MPI2_MASK, parent);
    case SF32LB52_HPSYS_CLK_HCLK:
      parent_id = SF32LB52_HPSYS_CLK_SYS;
      break;
    case SF32LB52_HPSYS_CLK_PCLK:
    case SF32LB52_HPSYS_CLK_PCLK2:
      parent_id = SF32LB52_HPSYS_CLK_HCLK;
      break;
    case SF32LB52_HPSYS_CLK_PERI_DIV2:
      parent_id = SF32LB52_HPSYS_CLK_PERI;
      break;
    case SF32LB52_HPSYS_CLK_USBC_K:
      parent_id =
          (csr & HPSYS_RCC_CSR_SEL_USBC) != 0U ? SF32LB52_HPSYS_CLK_DLL2 : SF32LB52_HPSYS_CLK_SYS;
      break;
    default:
      if (id >= NUM_GATES || s_gate_parents[id] == 0U) {
        return -ENOENT;
      }
      parent_id = s_gate_parents[id];
      break;
  }

  *parent = (struct pbl_clk)PBL_CLK(ctrl, parent_id);

  return 0;
}

static bool prv_same(const struct pbl_clk *a, const struct pbl_clk *b) {
  return a->ctrl == b->ctrl && a->id == b->id;
}

static uint64_t prv_rate(const struct pbl_clk_controller *ctrl, uint32_t id) {
  uint64_t rate_mhz = 0U;

  (void)pbl_clk_get_rate(&(struct pbl_clk)PBL_CLK(ctrl, id), &rate_mhz);

  return rate_mhz;
}

// Raises the APB dividers so that the APB clocks stay within their limits at an HCLK rate
static void prv_limit_pclks(const struct pbl_clk_controller *ctrl, uint64_t hclk_mhz) {
  uint32_t cfgr = prv_read(ctrl, SF32LB52_HPSYS_RCC_CFGR_OFFSET);
  uint32_t pdiv1 = PBL_FIELD_GET(HPSYS_RCC_CFGR_PDIV1_MASK, cfgr);
  uint32_t pdiv2 = MAX(PBL_FIELD_GET(HPSYS_RCC_CFGR_PDIV2_MASK, cfgr), 1U);

  while ((hclk_mhz >> pdiv1) > (uint64_t)PCLK_MAX_HZ * MILLIHZ_PER_HZ && pdiv1 < PDIV_MAX) {
    pdiv1++;
  }
  while ((hclk_mhz >> pdiv2) > (uint64_t)PCLK2_MAX_HZ * MILLIHZ_PER_HZ && pdiv2 < PDIV_MAX) {
    pdiv2++;
  }

  prv_modify(ctrl, SF32LB52_HPSYS_RCC_CFGR_OFFSET,
             HPSYS_RCC_CFGR_PDIV1_MASK | HPSYS_RCC_CFGR_PDIV2_MASK,
             PBL_FIELD_PREP(HPSYS_RCC_CFGR_PDIV1_MASK, pdiv1) |
                 PBL_FIELD_PREP(HPSYS_RCC_CFGR_PDIV2_MASK, pdiv2));
}

static void prv_update_core_clock(const struct pbl_clk_controller *ctrl) {
  uint64_t hclk_mhz = prv_rate(ctrl, SF32LB52_HPSYS_CLK_HCLK);

  if (hclk_mhz != 0U) {
    SystemCoreClock = (uint32_t)(hclk_mhz / MILLIHZ_PER_HZ);
  }
}

// Switches clk_hpsys, keeping the APB clocks within their limits whatever the new rate
static void prv_sys_select(const struct pbl_clk_controller *ctrl, uint32_t sel, uint64_t sys_mhz) {
  uint32_t hdiv = MAX(
      PBL_FIELD_GET(HPSYS_RCC_CFGR_HDIV_MASK, prv_read(ctrl, SF32LB52_HPSYS_RCC_CFGR_OFFSET)), 1U);

  prv_limit_pclks(ctrl, sys_mhz / hdiv);
  prv_modify(ctrl, SF32LB52_HPSYS_RCC_CSR_OFFSET,
             HPSYS_RCC_CSR_SEL_SYS_MASK | HPSYS_RCC_CSR_SEL_SYS_LP,
             PBL_FIELD_PREP(HPSYS_RCC_CSR_SEL_SYS_MASK, sel));
  prv_update_core_clock(ctrl);
}

static int prv_set_sys_parent(const struct pbl_clk_controller *ctrl, const struct pbl_clk *parent) {
  const struct pbl_clk_sf32lb *rcc = prv_rcc(ctrl);
  uint64_t rate_mhz = 0U;
  uint32_t sel;
  int ret;

  if (prv_same(parent, &rcc->hrc48)) {
    sel = HPSYS_RCC_CSR_SEL_SYS_HRC48;
  } else if (prv_same(parent, &rcc->hxt48)) {
    sel = HPSYS_RCC_CSR_SEL_SYS_HXT48;
  } else if (prv_same(parent, &(struct pbl_clk)PBL_CLK(ctrl, SF32LB52_HPSYS_CLK_DLL1))) {
    sel = HPSYS_RCC_CSR_SEL_SYS_DLL1;
  } else {
    return -EINVAL;
  }

  ret = pbl_clk_get_rate(parent, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  prv_sys_select(ctrl, sel, rate_mhz);

  return 0;
}

static int prv_set_mpi_parent(const struct pbl_clk_controller *ctrl, uint32_t mask,
                              const struct pbl_clk *parent) {
  uint32_t sel;

  if (parent->ctrl != ctrl) {
    return -EINVAL;
  }

  switch (parent->id) {
    case SF32LB52_HPSYS_CLK_PERI:
      sel = HPSYS_RCC_CSR_SEL_MPI_PERI;
      break;
    case SF32LB52_HPSYS_CLK_DLL1:
      sel = HPSYS_RCC_CSR_SEL_MPI_DLL1;
      break;
    case SF32LB52_HPSYS_CLK_DLL2:
      sel = HPSYS_RCC_CSR_SEL_MPI_DLL2;
      break;
    default:
      return -EINVAL;
  }

  prv_modify(ctrl, SF32LB52_HPSYS_RCC_CSR_OFFSET, mask, PBL_FIELD_PREP(mask, sel));

  return 0;
}

static int prv_clk_set_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                              const struct pbl_clk *parent) {
  const struct pbl_clk_sf32lb *rcc = prv_rcc(ctrl);
  uint32_t sel;

  switch (id) {
    case SF32LB52_HPSYS_CLK_SYS:
      return prv_set_sys_parent(ctrl, parent);
    case SF32LB52_HPSYS_CLK_MPI1_K:
      return prv_set_mpi_parent(ctrl, HPSYS_RCC_CSR_SEL_MPI1_MASK, parent);
    case SF32LB52_HPSYS_CLK_MPI2_K:
      return prv_set_mpi_parent(ctrl, HPSYS_RCC_CSR_SEL_MPI2_MASK, parent);
    case SF32LB52_HPSYS_CLK_PERI:
      break;
    default:
      return -EINVAL;
  }

  if (prv_same(parent, &rcc->hxt48)) {
    sel = HPSYS_RCC_CSR_SEL_PERI;
  } else if (prv_same(parent, &rcc->hrc48)) {
    sel = 0U;
  } else {
    return -EINVAL;
  }

  prv_modify(ctrl, SF32LB52_HPSYS_RCC_CSR_OFFSET, HPSYS_RCC_CSR_SEL_PERI, sel);

  return 0;
}

// Reprograms a running DLL, moving clk_hpsys to HRC48 for the time if it runs from it
static int prv_dll_set_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t rate_mhz) {
  const uint64_t step_mhz = (uint64_t)DLL_STEP_HZ * MILLIHZ_PER_HZ;
  uint32_t offset = prv_dll_offset(id);
  uint32_t stg;
  bool sys_on;
  int ret;

  if (rate_mhz < step_mhz || rate_mhz % step_mhz != 0U || rate_mhz / step_mhz - 1U > DLL_STG_MAX) {
    return -EINVAL;
  }
  stg = (uint32_t)(rate_mhz / step_mhz - 1U);

  if ((prv_read(ctrl, offset) & HPSYS_RCC_DLLCR_EN) == 0U) {
    prv_modify(ctrl, offset, HPSYS_RCC_DLLCR_STG_MASK,
               PBL_FIELD_PREP(HPSYS_RCC_DLLCR_STG_MASK, stg));
    return 0;
  }

  if (prv_dll_feeds_peripheral(ctrl, id)) {
    return -EBUSY;
  }

  sys_on = id == SF32LB52_HPSYS_CLK_DLL1 &&
           prv_sys_on_dll1(prv_read(ctrl, SF32LB52_HPSYS_RCC_CSR_OFFSET));
  if (sys_on) {
    uint64_t hrc48_mhz = 0U;

    ret = pbl_clk_get_rate(&prv_rcc(ctrl)->hrc48, &hrc48_mhz);
    if (ret != 0) {
      return ret;
    }
    prv_sys_select(ctrl, HPSYS_RCC_CSR_SEL_SYS_HRC48, hrc48_mhz);
  }

  prv_modify(ctrl, offset, HPSYS_RCC_DLLCR_EN | HPSYS_RCC_DLLCR_STG_MASK,
             PBL_FIELD_PREP(HPSYS_RCC_DLLCR_STG_MASK, stg));
  ret = prv_dll_enable(ctrl, id);

  if (sys_on && ret == 0) {
    prv_sys_select(ctrl, HPSYS_RCC_CSR_SEL_SYS_DLL1, rate_mhz);
  }

  return ret;
}

static int prv_clk_set_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t rate_mhz,
                            uint64_t parent_mhz) {
  uint32_t div;

  if (prv_is_dll(id)) {
    return prv_dll_set_rate(ctrl, id, rate_mhz);
  }

  if (rate_mhz == 0U || parent_mhz % rate_mhz != 0U) {
    return -EINVAL;
  }
  div = (uint32_t)(parent_mhz / rate_mhz);

  switch (id) {
    case SF32LB52_HPSYS_CLK_HCLK:
      if (div > HDIV_MAX) {
        return -EINVAL;
      }
      prv_limit_pclks(ctrl, rate_mhz);
      prv_modify(ctrl, SF32LB52_HPSYS_RCC_CFGR_OFFSET, HPSYS_RCC_CFGR_HDIV_MASK,
                 PBL_FIELD_PREP(HPSYS_RCC_CFGR_HDIV_MASK, div));
      prv_update_core_clock(ctrl);
      return 0;
    case SF32LB52_HPSYS_CLK_PCLK:
    case SF32LB52_HPSYS_CLK_PCLK2: {
      bool pclk2 = id == SF32LB52_HPSYS_CLK_PCLK2;
      uint32_t mask = pclk2 ? HPSYS_RCC_CFGR_PDIV2_MASK : HPSYS_RCC_CFGR_PDIV1_MASK;
      uint32_t max_hz = pclk2 ? PCLK2_MAX_HZ : PCLK_MAX_HZ;
      uint32_t pdiv = (uint32_t)__builtin_ctz(div);

      // PDIV2 cannot be 0
      if ((div & (div - 1U)) != 0U || pdiv > PDIV_MAX || (pclk2 && pdiv == 0U) ||
          rate_mhz > (uint64_t)max_hz * MILLIHZ_PER_HZ) {
        return -EINVAL;
      }
      prv_modify(ctrl, SF32LB52_HPSYS_RCC_CFGR_OFFSET, mask, PBL_FIELD_PREP(mask, pdiv));
      return 0;
    }
    default:
      return -EINVAL;
  }
}

static int prv_init(const struct pbl_device *dev) {
  const struct pbl_clk_sf32lb *rcc = container_of(dev, const struct pbl_clk_sf32lb, ctrl.dev);
  int ret;

  // The voltage the assigned rates need, before they are set
  ret = pbl_power_domain_set_performance_state(&rcc->pd, rcc->pd_level);
  if (ret != 0) {
    return ret;
  }

  // The CPU runs from it
  ret = pbl_clk_enable(&(struct pbl_clk)PBL_CLK(&rcc->ctrl, SF32LB52_HPSYS_CLK_SYS));
  if (ret != 0) {
    return ret;
  }

  return pbl_clk_assign(rcc->assigned);
}

static const struct pbl_clk_controller_ops s_clk_ops = {
  .enable = prv_clk_enable,
  .disable = prv_clk_disable,
  .get_rate = prv_clk_get_rate,
  .get_parent = prv_clk_get_parent,
  .set_parent = prv_clk_set_parent,
  .set_rate = prv_clk_set_rate,
};

#define PBL_CLK_SF32LB_DEFINE(sym, _name, _base, _cfg, _hrc48, _hxt48, _hxt48_dll, _pd, _pd_level, \
                              ...)                                                                 \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, SF32LB52_HPSYS_NUM_CLKS);                                   \
  PBL_CLK_ASSIGNED_DEFINE(sym##_assigned, __VA_ARGS__);                                            \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_sf32lb, struct pbl_clk_controller, ctrl) {               \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, prv_init, nullptr, &s_clk_ops), .base = (_base),   \
    .cfg = (_cfg), .hrc48 = _hrc48, .hxt48 = _hxt48, .hxt48_dll = _hxt48_dll, .pd = _pd,           \
    .pd_level = (_pd_level), .assigned = sym##_assigned,                                           \
  }

#include PBL_DT_INSTANCES(sifli_sf32lb52_hpsys_rcc_clk)
