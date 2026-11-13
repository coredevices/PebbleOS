/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/hyperbus/sf32lb52_mpi.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_mpi.h>

#define OCTAL MPI_CCR_MODE_OCTAL_DTR

// Command-address byte: read or write, register space, linear burst
#define CA_REG_READ  0xe0U
#define CA_REG_WRITE 0x60U

#define ALT_BYTE     0xffU
#define CMD_INTERVAL 0x50005000U
#define TIMEOUT      0xffU
#define ROW_BOUNDARY 7U
#define CS_LOW_MIN   6U
#define WDT_TIMEOUT  0xffffU

// Calibration runs at half the rate, then the bus runs at half the controller clock, in DDR
#define CAL_DIV 2U
#define BUS_DIV 1U

#define CMD_TIMEOUT_US 1000U
#define CAL_TIME_US    20U
#define CAL_TIMEOUT_US 1000U
// Taken off the measured delay
#define SCK_DELAY_BACKOFF 1U
#define DQS_DELAY_BACKOFF 4U
// Kept off the longest CS low time
#define CS_LOW_MARGIN 10U

// Data goes through the system bus alias of the window, not its code bus address
#define SBUS_OFFSET 0x50000000U

static const struct pbl_hyperbus_sf32lb52_mpi *prv_hb(const struct pbl_hyperbus *bus) {
  return container_of(bus, const struct pbl_hyperbus_sf32lb52_mpi, bus);
}

static uint32_t prv_ccr(uint32_t dcyc, uint32_t abmode) {
  return PBL_FIELD_PREP(MPI_CCR_IMODE_MASK, OCTAL) | PBL_FIELD_PREP(MPI_CCR_ADMODE_MASK, OCTAL) |
         PBL_FIELD_PREP(MPI_CCR_ADSIZE_MASK, MPI_CCR_ADSIZE_4) |
         PBL_FIELD_PREP(MPI_CCR_ABMODE_MASK, abmode) | PBL_FIELD_PREP(MPI_CCR_DCYC_MASK, dcyc) |
         PBL_FIELD_PREP(MPI_CCR_DMODE_MASK, OCTAL);
}

static int prv_cmd(uintptr_t base, uint32_t ccr, uint8_t opcode, uint32_t addr) {
  int ret;

  pbl_writel(ccr, base + SF32LB52_MPI_CCR1_OFFSET);
  pbl_writel(addr, base + SF32LB52_MPI_AR1_OFFSET);
  pbl_writel(opcode, base + SF32LB52_MPI_CMDR1_OFFSET);
  ret = PBL_POLL_BUSY_US((pbl_readl(base + SF32LB52_MPI_SR_OFFSET) & MPI_SR_TCF) != 0U, 0U,
                         CMD_TIMEOUT_US);
  pbl_writel(MPI_SCR_TCFC, base + SF32LB52_MPI_SCR_OFFSET);

  return ret;
}

// The controller takes the word address in two parts, and exchanges register values with their
// bytes swapped
static void prv_reg_addr(uintptr_t base, uint32_t addr, uint32_t *ar) {
  pbl_writel(addr & 0x7U, base + SF32LB52_MPI_ABR1_OFFSET);
  pbl_writel(PBL_FIELD_PREP(MPI_DLR_DLEN_MASK, 1U), base + SF32LB52_MPI_DLR1_OFFSET);
  *ar = (addr >> 3) << 8;
}

static uint16_t prv_swap(uint16_t val) {
  return (uint16_t)((val << 8) | (val >> 8));
}

static int prv_clk_rate(const struct pbl_sf32lb52_mpi *mpi, uint32_t *rate_hz) {
  uint64_t rate_mhz;
  int ret;

  ret = pbl_clk_get_rate(&mpi->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }
  *rate_hz = (uint32_t)(rate_mhz / 1000U) / BUS_DIV;

  return 0;
}

static int prv_enable(const struct pbl_hyperbus *bus) {
  const struct pbl_sf32lb52_mpi *mpi = prv_hb(bus)->mpi;
  const uintptr_t base = mpi->base;
  int ret;

  ret = pbl_sf32lb52_mpi_prepare(mpi);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&mpi->clk);
  if (ret != 0) {
    return ret;
  }

  pbl_writel(ALT_BYTE, base + SF32LB52_MPI_ABR1_OFFSET);
  pbl_writel(ALT_BYTE, base + SF32LB52_MPI_HRABR_OFFSET);
  pbl_writel(CMD_INTERVAL, base + SF32LB52_MPI_CIR_OFFSET);
  pbl_writel(TIMEOUT, base + SF32LB52_MPI_TIMR_OFFSET);
  pbl_modifyl(MPI_CR_DFM, MPI_CR_EN | MPI_CR_OPIE, base + SF32LB52_MPI_CR_OFFSET);

  return 0;
}

static void prv_disable(const struct pbl_hyperbus *bus) {
  (void)pbl_clk_disable(&prv_hb(bus)->mpi->clk);
}

static int prv_get_rate(const struct pbl_hyperbus *bus, uint32_t *rate_hz) {
  int ret;

  ret = prv_clk_rate(prv_hb(bus)->mpi, rate_hz);
  if (ret != 0) {
    return ret;
  }
  *rate_hz /= 2U;

  return 0;
}

// Measures the SCK and DQS delay lines against the controller's own reference
static int prv_calibrate(uintptr_t base, uint8_t *sck, uint8_t *dqs) {
  uint32_t delay;
  int ret;

  pbl_writel(CAL_DIV, base + SF32LB52_MPI_PSCLR_OFFSET);
  pbl_modifyl(MPI_MISCR_SCKINV, 0U, base + SF32LB52_MPI_MISCR_OFFSET);
  pbl_modifyl(0U, MPI_CALCR_EN, base + SF32LB52_MPI_CALCR_OFFSET);
  pbl_busy_wait_us(CAL_TIME_US);
  ret = PBL_POLL_BUSY_US((pbl_readl(base + SF32LB52_MPI_CALCR_OFFSET) & MPI_CALCR_DONE) != 0U, 0U,
                         CAL_TIMEOUT_US);
  delay = PBL_FIELD_GET(MPI_CALCR_DELAY_MASK, pbl_readl(base + SF32LB52_MPI_CALCR_OFFSET));
  pbl_modifyl(MPI_CALCR_EN, 0U, base + SF32LB52_MPI_CALCR_OFFSET);
  pbl_writel(BUS_DIV, base + SF32LB52_MPI_PSCLR_OFFSET);
  if (ret != 0) {
    return ret;
  }
  if (delay < DQS_DELAY_BACKOFF) {
    return -EIO;
  }

  *sck = (uint8_t)(delay - SCK_DELAY_BACKOFF);
  *dqs = (uint8_t)(delay - DQS_DELAY_BACKOFF);

  return 0;
}

// Clock cycles covering a time, less the one the controller adds
static uint32_t prv_cycles(uint32_t ns, uint32_t rate_hz) {
  return (uint32_t)DIVIDE_CEIL((uint64_t)ns * rate_hz, 1000000000ULL) - 1U;
}

static int prv_configure(const struct pbl_hyperbus *bus, const struct pbl_hyperbus_config *cfg,
                         uintptr_t *mem_base) {
  const struct pbl_hyperbus_sf32lb52_mpi *hb = prv_hb(bus);
  const struct pbl_sf32lb52_mpi *mpi = hb->mpi;
  const uintptr_t base = mpi->base;
  uint32_t rate_hz;
  uint32_t cs_low_max;
  uint32_t ccr;
  uint8_t sck;
  uint8_t dqs;
  int ret;

  ret = prv_clk_rate(mpi, &rate_hz);
  if (ret != 0) {
    return ret;
  }

  ret = prv_calibrate(base, &sck, &dqs);
  if (ret != 0) {
    return ret;
  }

  cs_low_max = (uint32_t)(((uint64_t)cfg->cs_low_max_ns * rate_hz) / 1000000000ULL) - CS_LOW_MARGIN;
  pbl_modifyl(MPI_DCR_CSLMIN_MASK | MPI_DCR_CSLMAX_MASK | MPI_DCR_CSHMIN_MASK |
                  MPI_DCR_TRCMIN_MASK | MPI_DCR_RBSIZE_MASK,
              PBL_FIELD_PREP(MPI_DCR_CSLMIN_MASK, CS_LOW_MIN) |
                  PBL_FIELD_PREP(MPI_DCR_CSLMAX_MASK, cs_low_max) |
                  PBL_FIELD_PREP(MPI_DCR_CSHMIN_MASK, prv_cycles(cfg->cs_high_min_ns, rate_hz)) |
                  PBL_FIELD_PREP(MPI_DCR_TRCMIN_MASK, prv_cycles(cfg->read_recovery_ns, rate_hz)) |
                  PBL_FIELD_PREP(MPI_DCR_RBSIZE_MASK, ROW_BOUNDARY) | MPI_DCR_DQSE | MPI_DCR_HYPER |
                  MPI_DCR_FIXLAT,
              base + SF32LB52_MPI_DCR_OFFSET);
  pbl_modifyl(
      MPI_MISCR_DQSDLY_MASK | MPI_MISCR_SCKDLY_MASK | MPI_MISCR_SCKINV,
      PBL_FIELD_PREP(MPI_MISCR_DQSDLY_MASK, dqs) | PBL_FIELD_PREP(MPI_MISCR_SCKDLY_MASK, sck),
      base + SF32LB52_MPI_MISCR_OFFSET);

  // The memory-mapped read and write commands
  hb->data->latency = cfg->latency;
  ccr = prv_ccr(cfg->latency - 1U, OCTAL);
  pbl_writel(ccr, base + SF32LB52_MPI_HRCCR_OFFSET);
  pbl_writel(ccr, base + SF32LB52_MPI_HWCCR_OFFSET);

  pbl_writel(PBL_FIELD_PREP(MPI_WDTR_TIMEOUT_MASK, WDT_TIMEOUT) | MPI_WDTR_EN,
             base + SF32LB52_MPI_WDTR_OFFSET);

  *mem_base = mpi->mem_base + SBUS_OFFSET;

  return 0;
}

static int prv_read_reg(const struct pbl_hyperbus *bus, uint32_t addr, uint16_t *val) {
  const struct pbl_hyperbus_sf32lb52_mpi *hb = prv_hb(bus);
  const uintptr_t base = hb->mpi->base;
  uint32_t ar;
  int ret;

  prv_reg_addr(base, addr, &ar);
  ret = prv_cmd(base, prv_ccr(hb->data->latency - 1U, OCTAL), CA_REG_READ, ar);
  *val = prv_swap((uint16_t)pbl_readl(base + SF32LB52_MPI_DR_OFFSET));

  return ret;
}

static int prv_write_reg(const struct pbl_hyperbus *bus, uint32_t addr, uint16_t val) {
  const uintptr_t base = prv_hb(bus)->mpi->base;
  uint32_t ar;

  prv_reg_addr(base, addr, &ar);
  pbl_writel(prv_swap(val), base + SF32LB52_MPI_DR_OFFSET);

  return prv_cmd(base, prv_ccr(0U, OCTAL) | MPI_CCR_FMODE_WRITE, CA_REG_WRITE, ar);
}

// A dummy write, which the memory ignores, gives the pulse
static int prv_wake(const struct pbl_hyperbus *bus) {
  const uintptr_t base = prv_hb(bus)->mpi->base;

  pbl_modifyl(MPI_CR_DFM, MPI_CR_EN | MPI_CR_OPIE, base + SF32LB52_MPI_CR_OFFSET);
  pbl_writel(PBL_FIELD_PREP(MPI_DLR_DLEN_MASK, 3U), base + SF32LB52_MPI_DLR1_OFFSET);
  pbl_writel(0U, base + SF32LB52_MPI_DR_OFFSET);

  return prv_cmd(base, prv_ccr(7U, MPI_CCR_MODE_NONE) | MPI_CCR_FMODE_WRITE, 0U, 0U);
}

const struct pbl_hyperbus_ops pbl_hyperbus_sf32lb52_mpi_ops = {
  .enable = prv_enable,
  .disable = prv_disable,
  .get_rate = prv_get_rate,
  .configure = prv_configure,
  .read_reg = prv_read_reg,
  .write_reg = prv_write_reg,
  .wake = prv_wake,
};

#include PBL_DT_INSTANCES(sifli_sf32lb52_mpi_hyperbus)
