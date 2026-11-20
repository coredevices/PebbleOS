/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/hyperbus/sf32lb_mpi.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_mpi.h>

#ifdef CONFIG_SOC_SF32LB57
#include <hardware/sf32lb57_mpi.h>

#define CIR_OFFSET   SF32LB57_MPI_CIR_OFFSET
#define TIMR_OFFSET  SF32LB57_MPI_TIMR_OFFSET
#define WDTR_OFFSET  SF32LB57_MPI_WDTR_OFFSET
#define DCR_PROTOCOL SF32LB57_MPI_DCR_PROT_MASK
#define DCR_HYPERBUS PBL_FIELD_PREP(SF32LB57_MPI_DCR_PROT_MASK, SF32LB57_MPI_DCR_PROT_HYPERBUS)
#define MISCR_DQSDLY SF32LB57_MPI_MISCR_DQSDLY_MASK
#define MISCR_SCKDLY SF32LB57_MPI_MISCR_SCKDLY_MASK
#define MISCR_SCKINV SF32LB57_MPI_MISCR_SCKINV
#else
#define CIR_OFFSET   SF32LB52_MPI_CIR_OFFSET
#define TIMR_OFFSET  SF32LB52_MPI_TIMR_OFFSET
#define WDTR_OFFSET  SF32LB52_MPI_WDTR_OFFSET
#define DCR_PROTOCOL (MPI_DCR_DQSE | MPI_DCR_HYPER | MPI_DCR_FIXLAT)
#define DCR_HYPERBUS (MPI_DCR_DQSE | MPI_DCR_HYPER | MPI_DCR_FIXLAT)
#define MISCR_DQSDLY MPI_MISCR_DQSDLY_MASK
#define MISCR_SCKDLY MPI_MISCR_SCKDLY_MASK
#define MISCR_SCKINV MPI_MISCR_SCKINV
#endif

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

// The bus runs at half the controller clock, in DDR
#define BUS_DIV 1U

#define CMD_TIMEOUT_US 1000U
#ifdef CONFIG_SOC_SF32LB57
#define CAL_DELAY_MAX 127U
#define CAL_SETTLE_US 10U
#define CAL_SAMPLES   5U
// Taken off half the measured clock period
#define SCK_DELAY_BACKOFF 5U
#else
// Calibration runs at half the rate
#define CAL_DIV           2U
#define CAL_TIME_US       20U
#define CAL_TIMEOUT_US    1000U
// Taken off the measured delay
#define SCK_DELAY_BACKOFF 1U
#define DQS_DELAY_BACKOFF 4U
#endif
// Kept off the longest CS low time
#define CS_LOW_MARGIN 10U

static const struct pbl_hyperbus_sf32lb_mpi *prv_hb(const struct pbl_hyperbus *bus) {
  return PBL_CONTAINER_OF(bus, const struct pbl_hyperbus_sf32lb_mpi, bus);
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

static int prv_clk_rate(const struct pbl_sf32lb_mpi *mpi, uint32_t *rate_hz) {
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
  const struct pbl_sf32lb_mpi *mpi = prv_hb(bus)->mpi;
  const uintptr_t base = mpi->base;
  int ret;

  ret = pbl_sf32lb_mpi_prepare(mpi);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&mpi->clk);
  if (ret != 0) {
    return ret;
  }

  pbl_writel(ALT_BYTE, base + SF32LB52_MPI_ABR1_OFFSET);
  pbl_writel(ALT_BYTE, base + SF32LB52_MPI_HRABR_OFFSET);
  pbl_writel(CMD_INTERVAL, base + CIR_OFFSET);
  pbl_writel(TIMEOUT, base + TIMR_OFFSET);
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

#ifdef CONFIG_SOC_SF32LB57
// Whether the controller samples its clock at a level, at the SCK delay set
static bool prv_cal_samples(uintptr_t base, uint32_t delay, bool level) {
  pbl_writel(PBL_FIELD_PREP(MISCR_SCKDLY, delay), base + SF32LB52_MPI_MISCR_OFFSET);
  pbl_busy_wait_us(CAL_SETTLE_US);

  for (uint32_t i = 0U; i < CAL_SAMPLES; i++) {
    if (((pbl_readl(base + SF32LB57_MPI_CALRR_OFFSET) & SF32LB57_MPI_CALRR_CSMP) != 0U) != level) {
      return false;
    }
  }

  return true;
}

// Sweeps the SCK delay line from a low to a high sample of the clock: the distance is half a
// clock period, in delay steps. It only runs with the controller disabled.
static int prv_calibrate(uintptr_t base, uint8_t *sck, uint8_t *dqs) {
  const uint32_t cr = pbl_readl(base + SF32LB52_MPI_CR_OFFSET);
  uint32_t low;
  uint32_t high;
  uint32_t half;

  pbl_writel(BUS_DIV, base + SF32LB52_MPI_PSCLR_OFFSET);
  pbl_writel(cr & ~MPI_CR_EN, base + SF32LB52_MPI_CR_OFFSET);
  pbl_writel(SF32LB57_MPI_CALCR_START, base + SF32LB57_MPI_CALCR_OFFSET);
  for (low = 0U; low <= CAL_DELAY_MAX && !prv_cal_samples(base, low, false); low++) {
  }
  for (high = low + 1U; high <= CAL_DELAY_MAX && !prv_cal_samples(base, high, true); high++) {
  }
  pbl_writel(SF32LB57_MPI_CALCR_STOP, base + SF32LB57_MPI_CALCR_OFFSET);
  pbl_writel(cr, base + SF32LB52_MPI_CR_OFFSET);

  if (high > CAL_DELAY_MAX) {
    return -EIO;
  }
  half = (high - low) / 2U;
  if (half < SCK_DELAY_BACKOFF) {
    return -EIO;
  }

  *sck = (uint8_t)(half - SCK_DELAY_BACKOFF);
  *dqs = (uint8_t)half;

  return 0;
}
#else
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
#endif

// Clock cycles covering a time, less the one the controller adds
static uint32_t prv_cycles(uint32_t ns, uint32_t rate_hz) {
  return (uint32_t)PBL_DIV_ROUND_UP((uint64_t)ns * rate_hz, 1000000000ULL) - 1U;
}

static int prv_configure(const struct pbl_hyperbus *bus, const struct pbl_hyperbus_config *cfg,
                         uintptr_t *mem_base) {
  const struct pbl_hyperbus_sf32lb_mpi *hb = prv_hb(bus);
  const struct pbl_sf32lb_mpi *mpi = hb->mpi;
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

#ifdef CONFIG_SOC_SF32LB57
  // A sweep after deep sleep leaves the bus stalling, so the delays of the first one are kept
  if (!hb->data->calibrated) {
    ret = prv_calibrate(base, &hb->data->sck, &hb->data->dqs);
    if (ret != 0) {
      return ret;
    }
    hb->data->calibrated = true;
  }
  sck = hb->data->sck;
  dqs = hb->data->dqs;
#else
  ret = prv_calibrate(base, &sck, &dqs);
  if (ret != 0) {
    return ret;
  }
#endif

  cs_low_max = (uint32_t)(((uint64_t)cfg->cs_low_max_ns * rate_hz) / 1000000000ULL) - CS_LOW_MARGIN;
  pbl_modifyl(MPI_DCR_CSLMIN_MASK | MPI_DCR_CSLMAX_MASK | MPI_DCR_CSHMIN_MASK |
                  MPI_DCR_TRCMIN_MASK | MPI_DCR_RBSIZE_MASK | DCR_PROTOCOL,
              PBL_FIELD_PREP(MPI_DCR_CSLMIN_MASK, CS_LOW_MIN) |
                  PBL_FIELD_PREP(MPI_DCR_CSLMAX_MASK, cs_low_max) |
                  PBL_FIELD_PREP(MPI_DCR_CSHMIN_MASK, prv_cycles(cfg->cs_high_min_ns, rate_hz)) |
                  PBL_FIELD_PREP(MPI_DCR_TRCMIN_MASK, prv_cycles(cfg->read_recovery_ns, rate_hz)) |
                  PBL_FIELD_PREP(MPI_DCR_RBSIZE_MASK, ROW_BOUNDARY) | DCR_HYPERBUS,
              base + SF32LB52_MPI_DCR_OFFSET);
  pbl_modifyl(MISCR_DQSDLY | MISCR_SCKDLY | MISCR_SCKINV,
              PBL_FIELD_PREP(MISCR_DQSDLY, dqs) | PBL_FIELD_PREP(MISCR_SCKDLY, sck),
              base + SF32LB52_MPI_MISCR_OFFSET);

  // The memory-mapped read and write commands
  hb->data->latency = cfg->latency;
  ccr = prv_ccr(cfg->latency - 1U, OCTAL);
  pbl_writel(ccr, base + SF32LB52_MPI_HRCCR_OFFSET);
  pbl_writel(ccr, base + SF32LB52_MPI_HWCCR_OFFSET);

  pbl_writel(PBL_FIELD_PREP(MPI_WDTR_TIMEOUT_MASK, WDT_TIMEOUT) | MPI_WDTR_EN, base + WDTR_OFFSET);

  // Data goes through the system bus
  *mem_base = mpi->mem_sbus_base;

  return 0;
}

static int prv_read_reg(const struct pbl_hyperbus *bus, uint32_t addr, uint16_t *val) {
  const struct pbl_hyperbus_sf32lb_mpi *hb = prv_hb(bus);
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

const struct pbl_hyperbus_ops pbl_hyperbus_sf32lb_mpi_ops = {
  .enable = prv_enable,
  .disable = prv_disable,
  .get_rate = prv_get_rate,
  .configure = prv_configure,
  .read_reg = prv_read_reg,
  .write_reg = prv_write_reg,
  .wake = prv_wake,
};

#include PBL_DT_INSTANCES(sifli_sf32lb52_mpi_hyperbus)
#include PBL_DT_INSTANCES(sifli_sf32lb57_mpi_hyperbus)
