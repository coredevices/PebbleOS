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

#include <hardware/sf32lb52_hpsys_rcc.h>

#define NUM_GATES 64U

#define DLL_STEP_HZ 24000000U

struct pbl_clk_sf32lb {
  struct pbl_clk_controller ctrl;
  uintptr_t base;
  struct pbl_clk hrc48;
  struct pbl_clk hxt48;
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

static int prv_clk_enable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  if (id < NUM_GATES) {
    pbl_writel(PBL_BIT(id & 31U), prv_reg(prv_rcc(ctrl)->base, SF32LB52_HPSYS_RCC_ESR1_OFFSET, id));
  }

  return 0;
}

static int prv_clk_disable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  if (id < NUM_GATES) {
    pbl_writel(PBL_BIT(id & 31U), prv_reg(prv_rcc(ctrl)->base, SF32LB52_HPSYS_RCC_ECR1_OFFSET, id));
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
      *parent = rcc->hxt48;
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

static int prv_clk_set_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                              const struct pbl_clk *parent) {
  const struct pbl_clk_sf32lb *rcc = prv_rcc(ctrl);
  uint32_t sel;

  if (id != SF32LB52_HPSYS_CLK_PERI) {
    return -EINVAL;
  }

  if (prv_same(parent, &rcc->hxt48)) {
    sel = HPSYS_RCC_CSR_SEL_PERI;
  } else if (prv_same(parent, &rcc->hrc48)) {
    sel = 0U;
  } else {
    return -EINVAL;
  }

  pbl_modifyl(HPSYS_RCC_CSR_SEL_PERI, sel, rcc->base + SF32LB52_HPSYS_RCC_CSR_OFFSET);

  return 0;
}

static int prv_init(const struct pbl_device *dev) {
  const struct pbl_clk_sf32lb *rcc = container_of(dev, const struct pbl_clk_sf32lb, ctrl.dev);

  return pbl_clk_assign(rcc->assigned);
}

static const struct pbl_clk_controller_ops s_clk_ops = {
  .enable = prv_clk_enable,
  .disable = prv_clk_disable,
  .get_rate = prv_clk_get_rate,
  .get_parent = prv_clk_get_parent,
  .set_parent = prv_clk_set_parent,
};

#define PBL_CLK_SF32LB_DEFINE(sym, _name, _base, _hrc48, _hxt48, ...)                            \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, SF32LB52_HPSYS_NUM_CLKS);                                 \
  PBL_CLK_ASSIGNED_DEFINE(sym##_assigned, __VA_ARGS__);                                          \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_sf32lb, struct pbl_clk_controller, ctrl) {             \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, prv_init, nullptr, &s_clk_ops), .base = (_base), \
    .hrc48 = _hrc48, .hxt48 = _hxt48, .assigned = sym##_assigned,                                \
  }

#include PBL_DT_INSTANCES(sifli_sf32lb52_hpsys_rcc_clk)
