/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/drivers/spi_mem/sf32lb52_mpi.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/section.h>
#include <pbl/kernel/time.h>
#include <pbl/mcu/cache.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_mpi.h>

#define FIFO_SIZE      64U
#define WORD_SIZE      sizeof(uint32_t)
#define MAX_DCYC       31U
#define MAX_ADDR_BYTES 4U
#define ALT_BYTE       0xFFU
// Interval between the status reads of the second command slot
#define CMD_INTERVAL 0x50005000U
// Controller timeout, as previous flash drivers always set it
#define TIMEOUT 0xFFU
// Window words compared before and after changing the read command
#define CHECK_WORDS 4U
// Data cache invalidate by address, and the cycle counter
#define SCB_DCIMVAC        0xE000EF5CUL
#define DEMCR              0xE000EDFCUL
#define DEMCR_TRCENA       PBL_BIT(24)
#define DWT_CTRL           0xE0001000UL
#define DWT_CTRL_CYCCNTENA PBL_BIT(0)
#define DWT_CYCCNT         0xE0001004UL
// Interrupt controller
#define NVIC_ISER(i) (0xE000E100UL + ((i) * 4U))
#define NVIC_ISPR(i) (0xE000E200UL + ((i) * 4U))
#define NVIC_IPR(n)  (0xE000E400UL + (n))
#define NVIC_WORDS   ((CONFIG_NUM_IRQS + 31U) / 32U)
#define KERNEL_PRIO  (PBL_IRQ_PRIO_KERNEL & ~((1U << (8U - CONFIG_NUM_IRQ_PRIO_BITS)) - 1U))

// Data written through the DMA, or pushed to the FIFO, is staged here: the source may be the
// memory being programmed
static uint8_t PBL_ALIGNED(32) s_bounce[256];

struct ccr_op {
  uint32_t ccr;
  uint32_t addr;
  uint8_t opcode;
};

static const struct pbl_spi_mem_sf32lb52_mpi *prv_mpi(const struct pbl_spi_mem *mem) {
  return container_of(mem, const struct pbl_spi_mem_sf32lb52_mpi, mem);
}

static uintptr_t prv_base(const struct pbl_spi_mem_sf32lb52_mpi *mpi) {
  return (mpi->idx == 1U) ? SF32LB52_MPI1_BASE : SF32LB52_MPI2_BASE;
}

static uintptr_t prv_mem_base(const struct pbl_spi_mem_sf32lb52_mpi *mpi) {
  return (mpi->idx == 1U) ? SF32LB52_MPI1_MEM_BASE : SF32LB52_MPI2_MEM_BASE;
}

static PBL_ALWAYS_INLINE uint32_t prv_mode(uint8_t buswidth) {
  switch (buswidth) {
    case 0U:
      return MPI_CCR_MODE_NONE;
    case 1U:
      return MPI_CCR_MODE_SINGLE;
    case 2U:
      return MPI_CCR_MODE_DUAL;
    default:
      return MPI_CCR_MODE_QUAD;
  }
}

static bool prv_width_ok(uint8_t buswidth) {
  return buswidth == 0U || buswidth == 1U || buswidth == 2U || buswidth == 4U;
}

static PBL_ALWAYS_INLINE uint32_t prv_dummy_cycles(const struct pbl_spi_mem_op *op) {
  return (op->dummy.buswidth != 0U) ? (op->dummy.nbytes * 8U) / op->dummy.buswidth : 0U;
}

// The first dummy byte after an address goes out as an 0xFF alternate byte, so that the memory
// never sees a continuous read mode pattern
static PBL_ALWAYS_INLINE uint32_t prv_alt_cycles(const struct pbl_spi_mem_op *op) {
  uint32_t cycles;

  if (op->addr.nbytes == 0U) {
    return 0U;
  }

  cycles = 8U / op->addr.buswidth;

  return (prv_dummy_cycles(op) >= cycles) ? cycles : 0U;
}

static PBL_ALWAYS_INLINE struct ccr_op prv_encode(const struct pbl_spi_mem_op *op) {
  uint32_t alt = prv_alt_cycles(op);
  uint32_t ccr = PBL_FIELD_PREP(MPI_CCR_IMODE_MASK, prv_mode(op->cmd.buswidth));

  if (op->addr.nbytes != 0U) {
    ccr |= PBL_FIELD_PREP(MPI_CCR_ADMODE_MASK, prv_mode(op->addr.buswidth)) |
           PBL_FIELD_PREP(MPI_CCR_ADSIZE_MASK, op->addr.nbytes - 1U);
  }

  if (alt != 0U) {
    ccr |= PBL_FIELD_PREP(MPI_CCR_ABMODE_MASK, prv_mode(op->addr.buswidth)) |
           PBL_FIELD_PREP(MPI_CCR_ABSIZE_MASK, 0U);
  }

  ccr |= PBL_FIELD_PREP(MPI_CCR_DCYC_MASK, prv_dummy_cycles(op) - alt);

  if (op->data.dir != PBL_SPI_MEM_NO_DATA) {
    ccr |= PBL_FIELD_PREP(MPI_CCR_DMODE_MASK, prv_mode(op->data.buswidth));
  }

  if (op->data.dir == PBL_SPI_MEM_DATA_OUT) {
    ccr |= MPI_CCR_FMODE_WRITE;
  }

  return (struct ccr_op){.ccr = ccr, .addr = op->addr.val, .opcode = op->cmd.opcode};
}

static bool prv_supports_op(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  if (op->cmd.buswidth != 1U || !prv_width_ok(op->addr.buswidth) ||
      !prv_width_ok(op->dummy.buswidth) || !prv_width_ok(op->data.buswidth)) {
    return false;
  }

  if (op->addr.nbytes > MAX_ADDR_BYTES || (op->addr.nbytes != 0U && op->addr.buswidth == 0U)) {
    return false;
  }

  if (prv_dummy_cycles(op) - prv_alt_cycles(op) > MAX_DCYC) {
    return false;
  }

  // Reads are split into FIFO-sized pieces by advancing the address
  return op->data.dir != PBL_SPI_MEM_DATA_IN || op->addr.nbytes != 0U ||
         op->data.nbytes <= FIFO_SIZE;
}

static int prv_adjust_op_size(const struct pbl_spi_mem *mem, struct pbl_spi_mem_op *op) {
  if (op->data.dir == PBL_SPI_MEM_DATA_OUT) {
    op->data.nbytes = MIN(op->data.nbytes, sizeof(s_bounce));
  }

  return 0;
}

// Everything from here to the end of the section runs with interrupts disabled, from RAM, and
// calls nothing: with the CPU executing from the memory, it may be unreadable meanwhile.

// Masks every interrupt, including those the kernel never masks
static PBL_ALWAYS_INLINE uint32_t prv_irq_save(void) {
  uint32_t primask;

  __asm volatile("mrs %0, primask\n\tcpsid i" : "=r"(primask)::"memory");

  return primask;
}

static PBL_ALWAYS_INLINE void prv_irq_restore(uint32_t primask) {
  __asm volatile("msr primask, %0" ::"r"(primask) : "memory");
}

static PBL_ALWAYS_INLINE void prv_sync(void) {
  __asm volatile("dsb 0xf\n\tisb 0xf" ::: "memory");
}

static PBL_ALWAYS_INLINE void prv_wait_flag(uintptr_t base, uint32_t flag) {
  while ((pbl_readl(base + SF32LB52_MPI_SR_OFFSET) & flag) == 0U) {
  }
}

static PBL_ALWAYS_INLINE void prv_issue(uintptr_t base, const struct ccr_op *cmd) {
  pbl_writel(cmd->ccr, base + SF32LB52_MPI_CCR1_OFFSET);
  pbl_writel(cmd->addr, base + SF32LB52_MPI_AR1_OFFSET);
  pbl_writel(cmd->opcode, base + SF32LB52_MPI_CMDR1_OFFSET);
  prv_wait_flag(base, MPI_SR_TCF);
  pbl_writel(MPI_SCR_TCFC, base + SF32LB52_MPI_SCR_OFFSET);
}

static PBL_ALWAYS_INLINE void prv_pop(uintptr_t base, uint8_t *buf, size_t len) {
  for (size_t i = 0U; i < len; i += WORD_SIZE) {
    uint32_t word = pbl_readl(base + SF32LB52_MPI_DR_OFFSET);

    for (size_t j = 0U; j < WORD_SIZE && (i + j) < len; j++) {
      buf[i + j] = (uint8_t)(word >> (8U * j));
    }
  }
}

// Low power modes may stop the cycle counter
static PBL_ALWAYS_INLINE uint32_t prv_cycles(void) {
  pbl_writel(pbl_readl(DEMCR) | DEMCR_TRCENA, DEMCR);
  pbl_writel(pbl_readl(DWT_CTRL) | DWT_CTRL_CYCCNTENA, DWT_CTRL);

  return pbl_readl(DWT_CYCCNT);
}

static PBL_ALWAYS_INLINE void prv_delay_cycles(uint32_t cycles) {
  const uint32_t start = prv_cycles();

  while ((pbl_readl(DWT_CYCCNT) - start) < cycles) {
  }
}

static PBL_ALWAYS_INLINE uint8_t prv_read_status(uintptr_t base, const struct ccr_op *status) {
  uint8_t val;

  pbl_writel(0U, base + SF32LB52_MPI_DLR1_OFFSET);
  prv_issue(base, status);
  prv_pop(base, &val, 1U);

  return val;
}

// An enabled interrupt that would run with BASEPRI at @p level is pending
static PBL_ALWAYS_INLINE bool prv_irq_pending(uint32_t level) {
  for (uint32_t i = 0U; i < NVIC_WORDS; i++) {
    uint32_t pending = pbl_readl(NVIC_ISPR(i)) & pbl_readl(NVIC_ISER(i));

    while (pending != 0U) {
      uint32_t n = (i * 32U) + (uint32_t)__builtin_ctz(pending);

      if (pbl_readb(NVIC_IPR(n)) < level) {
        return true;
      }

      pending &= pending - 1U;
    }
  }

  return false;
}

static PBL_SECTION_RAM void prv_ram_read(uintptr_t base, const struct ccr_op *cmd, uint8_t *buf,
                                         size_t len) {
  uint32_t primask = prv_irq_save();

  pbl_writel(PBL_FIELD_PREP(MPI_DLR_DLEN_MASK, len - 1U), base + SF32LB52_MPI_DLR1_OFFSET);
  prv_issue(base, cmd);
  prv_pop(base, buf, len);

  prv_irq_restore(primask);
}

static PBL_SECTION_RAM void prv_ram_cmd(uintptr_t base, const struct ccr_op *cmd) {
  uint32_t primask = prv_irq_save();

  prv_issue(base, cmd);

  prv_irq_restore(primask);
}

// Issues the command, set up beforehand, then polls the status until it matches
static PBL_SECTION_RAM void prv_ram_poll(uintptr_t base, const struct ccr_op *cmd,
                                         const struct ccr_op *status, uint8_t mask, uint8_t match) {
  uint32_t primask = prv_irq_save();

  prv_issue(base, cmd);
  while ((prv_read_status(base, status) & mask) != match) {
  }

  prv_irq_restore(primask);
}

static PBL_ALWAYS_INLINE void prv_read_window(uintptr_t mem_base, uint32_t *words,
                                              uint32_t line_size) {
  for (uintptr_t addr = mem_base; addr < mem_base + (CHECK_WORDS * WORD_SIZE); addr += line_size) {
    pbl_writel(addr, SCB_DCIMVAC);
  }
  prv_sync();

  for (size_t i = 0U; i < CHECK_WORDS; i++) {
    words[i] = *(const volatile uint32_t *)(mem_base + (i * WORD_SIZE));
  }
}

// Polls the status in software, and whenever an interrupt is pending suspends the operation,
// lets the interrupts more urgent than the kernel's own run, and resumes it; never sooner than
// resume_cyc after the last resume, so that the operation progresses
static PBL_SECTION_RAM void prv_ram_poll_suspend(uintptr_t base, const struct ccr_op *cmd,
                                                 const struct ccr_op *status, uint8_t mask,
                                                 uint8_t match, const struct ccr_op *suspend,
                                                 const struct ccr_op *resume, uint32_t suspend_cyc,
                                                 uint32_t resume_cyc) {
  uint32_t primask = prv_irq_save();
  uint32_t basepri;
  uint32_t level;
  uint32_t resumed;

  // The window never lets in more than the caller does, and never the kernel's own interrupts
  __asm volatile("mrs %0, basepri" : "=r"(basepri));
  level = (basepri != 0U && basepri < KERNEL_PRIO) ? basepri : KERNEL_PRIO;

  prv_issue(base, cmd);
  resumed = prv_cycles();

  while ((prv_read_status(base, status) & mask) != match) {
    if (primask != 0U || (prv_cycles() - resumed) < resume_cyc || !prv_irq_pending(level)) {
      continue;
    }

    prv_issue(base, suspend);
    prv_delay_cycles(suspend_cyc);
    while ((prv_read_status(base, status) & mask) != match) {
    }

    __asm volatile(
        "msr basepri, %0\n\tcpsie i\n\tisb 0xf\n\tcpsid i\n\tmsr basepri, %1" ::"r"(level),
        "r"(basepri)
        : "memory");

    prv_issue(base, resume);
    resumed = prv_cycles();
  }

  prv_irq_restore(primask);
}

// Changes the window's read command, keeping the previous one if the window reads differently
static PBL_SECTION_RAM int prv_ram_set_read(uintptr_t base, uintptr_t mem_base, uint8_t opcode,
                                            uint32_t ccr, uint32_t line_size) {
  uint32_t primask = prv_irq_save();
  uint32_t before[CHECK_WORDS];
  uint32_t after[CHECK_WORDS];
  uint32_t hcmdr = pbl_readl(base + SF32LB52_MPI_HCMDR_OFFSET);
  uint32_t hrccr = pbl_readl(base + SF32LB52_MPI_HRCCR_OFFSET);
  uint32_t hrabr = pbl_readl(base + SF32LB52_MPI_HRABR_OFFSET);
  int ret = 0;

  prv_read_window(mem_base, before, line_size);

  pbl_writel(ALT_BYTE, base + SF32LB52_MPI_HRABR_OFFSET);
  pbl_writel(ccr, base + SF32LB52_MPI_HRCCR_OFFSET);
  pbl_writel((hcmdr & ~MPI_HCMDR_RCMD_MASK) | PBL_FIELD_PREP(MPI_HCMDR_RCMD_MASK, opcode),
             base + SF32LB52_MPI_HCMDR_OFFSET);

  prv_read_window(mem_base, after, line_size);

  for (size_t i = 0U; i < CHECK_WORDS; i++) {
    if (before[i] != after[i]) {
      ret = -EIO;
    }
  }

  if (ret != 0) {
    pbl_writel(hrabr, base + SF32LB52_MPI_HRABR_OFFSET);
    pbl_writel(hrccr, base + SF32LB52_MPI_HRCCR_OFFSET);
    pbl_writel(hcmdr, base + SF32LB52_MPI_HCMDR_OFFSET);
    prv_read_window(mem_base, after, line_size);
  }

  prv_irq_restore(primask);

  return ret;
}

// Every value is computed before the memory goes down: only fn may run from it meanwhile
static PBL_SECTION_RAM void prv_power_down(const struct pbl_spi_mem *mem,
                                           const struct pbl_spi_mem_pm *pm, pbl_spi_mem_pm_fn_t fn,
                                           void *arg) {
  const uintptr_t base = prv_base(prv_mpi(mem));
  const uint32_t cycles_per_us = (pbl_cycles_per_sec() + 999999U) / 1000000U;
  struct ccr_op enter = {};
  struct ccr_op exit = {};
  uint32_t enter_cycles = 0U;
  uint32_t exit_cycles = 0U;
  bool cmds = pm != nullptr;

  if (cmds) {
    enter = prv_encode(&pm->enter);
    exit = prv_encode(&pm->exit);
    enter_cycles = pm->enter_us * cycles_per_us;
    exit_cycles = pm->exit_us * cycles_per_us;
  }

  __asm volatile("" ::: "memory");

  if (cmds) {
    prv_issue(base, &enter);
    prv_delay_cycles(enter_cycles);
  }

  fn(arg);

  if (cmds) {
    prv_issue(base, &exit);
    prv_delay_cycles(exit_cycles);
  }
}

static int prv_dma_wait(const struct pbl_dma_chan *dma) {
  size_t residue;
  int ret;

  do {
    ret = pbl_dma_tx_status(dma, &residue);
  } while (ret == 0 && residue > 0U);

  (void)pbl_dma_terminate(dma);

  return ret;
}

// Data goes out through the DMA, which feeds the FIFO exactly as many bytes as the command takes;
// then, with status, the status is polled until it matches
static int prv_write(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op,
                     const struct pbl_spi_mem_op *status, uint8_t mask, uint8_t match) {
  const struct pbl_spi_mem_sf32lb52_mpi *mpi = prv_mpi(mem);
  const uintptr_t base = prv_base(mpi);
  const struct ccr_op cmd = prv_encode(op);
  const size_t len = op->data.nbytes;
  int ret;

  if (len == 0U || len > sizeof(s_bounce)) {
    return -EINVAL;
  }

  memcpy(s_bounce, op->data.buf.out, len);
  dcache_flush(s_bounce, sizeof(s_bounce));

  pbl_writel(cmd.ccr, base + SF32LB52_MPI_CCR1_OFFSET);
  pbl_writel(PBL_FIELD_PREP(MPI_DLR_DLEN_MASK, len - 1U), base + SF32LB52_MPI_DLR1_OFFSET);
  pbl_modifyl(0U, MPI_CR_DMAE, base + SF32LB52_MPI_CR_OFFSET);
  ret = pbl_dma_prep_slave_single(mpi->dma, s_bounce, len, PBL_DMA_MEM_TO_DEV, nullptr, nullptr);
  if (ret != 0) {
    pbl_modifyl(MPI_CR_DMAE, 0U, base + SF32LB52_MPI_CR_OFFSET);
    return ret;
  }

  if (status != nullptr) {
    const struct ccr_op st = prv_encode(status);

    prv_ram_poll(base, &cmd, &st, mask, match);
  } else {
    prv_ram_cmd(base, &cmd);
  }

  ret = prv_dma_wait(mpi->dma);
  pbl_modifyl(MPI_CR_DMAE, 0U, base + SF32LB52_MPI_CR_OFFSET);

  return ret;
}

static int prv_exec_op(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  const uintptr_t base = prv_base(prv_mpi(mem));
  struct ccr_op cmd = prv_encode(op);

  if (op->data.dir == PBL_SPI_MEM_DATA_IN) {
    uint8_t *buf = op->data.buf.in;
    size_t len = op->data.nbytes;

    while (len > 0U) {
      size_t chunk = MIN(len, FIFO_SIZE);

      prv_ram_read(base, &cmd, buf, chunk);
      cmd.addr += chunk;
      buf += chunk;
      len -= chunk;
    }

    return 0;
  }

  if (op->data.dir == PBL_SPI_MEM_DATA_OUT) {
    return prv_write(mem, op, nullptr, 0U, 0U);
  }

  prv_ram_cmd(base, &cmd);

  return 0;
}

static int prv_poll_status(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op,
                           const struct pbl_spi_mem_op *status, uint8_t mask, uint8_t match,
                           const struct pbl_spi_mem_suspend *susp) {
  const uintptr_t base = prv_base(prv_mpi(mem));
  const struct ccr_op cmd = prv_encode(op);
  const struct ccr_op st = prv_encode(status);

  if (op->data.dir == PBL_SPI_MEM_DATA_IN) {
    return -ENOTSUP;
  }

  if (op->data.dir == PBL_SPI_MEM_DATA_OUT) {
    return prv_write(mem, op, status, mask, match);
  }

  if (susp != nullptr && susp->suspend.data.dir == PBL_SPI_MEM_NO_DATA &&
      susp->resume.data.dir == PBL_SPI_MEM_NO_DATA) {
    const uint32_t cycles_per_us = (pbl_cycles_per_sec() + 999999U) / 1000000U;
    const struct ccr_op suspend = prv_encode(&susp->suspend);
    const struct ccr_op resume = prv_encode(&susp->resume);

    prv_ram_poll_suspend(base, &cmd, &st, mask, match, &suspend, &resume,
                         susp->suspend_us * cycles_per_us, susp->resume_us * cycles_per_us);
    return 0;
  }

  prv_ram_poll(base, &cmd, &st, mask, match);

  return 0;
}

static int prv_dirmap_create(const struct pbl_spi_mem *mem, struct pbl_spi_mem_dirmap *desc) {
  const struct pbl_spi_mem_sf32lb52_mpi *mpi = prv_mpi(mem);
  const uintptr_t base = prv_base(mpi);
  struct ccr_op cmd = prv_encode(&desc->op);

  if (desc->offset >= mpi->mem_size) {
    return -ENOTSUP;
  }

  desc->length = MIN(desc->length, mpi->mem_size - desc->offset);

  if (PBL_FIELD_GET(MPI_HCMDR_RCMD_MASK, pbl_readl(base + SF32LB52_MPI_HCMDR_OFFSET)) ==
          cmd.opcode &&
      pbl_readl(base + SF32LB52_MPI_HRCCR_OFFSET) == cmd.ccr &&
      pbl_readl(base + SF32LB52_MPI_HRABR_OFFSET) == ALT_BYTE) {
    return 0;
  }

  return prv_ram_set_read(base, prv_mem_base(mpi), cmd.opcode, cmd.ccr, dcache_line_size());
}

static int prv_dirmap_read(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_dirmap *desc,
                           uint32_t offs, void *buf, size_t len) {
  memcpy(buf, (const void *)(prv_mem_base(prv_mpi(mem)) + offs), len);

  return 0;
}

static void prv_dirmap_invalidate(const struct pbl_spi_mem *mem,
                                  const struct pbl_spi_mem_dirmap *desc, uint32_t offs,
                                  size_t len) {
  uintptr_t addr;
  size_t size;

  if (offs >= desc->offset + desc->length) {
    return;
  }

  len = MIN(len, desc->offset + desc->length - offs);
  addr = prv_mem_base(prv_mpi(mem)) + offs;
  size = len;
  dcache_align(&addr, &size);
  dcache_invalidate((void *)addr, size);

  addr = prv_mem_base(prv_mpi(mem)) + offs;
  size = len;
  icache_align(&addr, &size);
  icache_invalidate((void *)addr, size);
}

static int prv_init(const struct pbl_spi_mem *mem) {
  const struct pbl_spi_mem_sf32lb52_mpi *mpi = prv_mpi(mem);
  const uintptr_t base = prv_base(mpi);
  int ret;

  // Never released: with XIP the clock must stay on
  ret = pbl_clk_enable(&mpi->clk);
  if (ret != 0) {
    return ret;
  }

  if (!pbl_spi_mem_is_xip(mem)) {
    pbl_modifyl(MPI_CR_DFM, MPI_CR_EN, base + SF32LB52_MPI_CR_OFFSET);
  }

  pbl_writel(ALT_BYTE, base + SF32LB52_MPI_ABR1_OFFSET);
  pbl_writel(CMD_INTERVAL, base + SF32LB52_MPI_CIR_OFFSET);
  pbl_writel(TIMEOUT, base + SF32LB52_MPI_TIMR_OFFSET);

  pbl_modifyl(MPI_FIFOCR_TXSLOTS_MASK, PBL_FIELD_PREP(MPI_FIFOCR_TXSLOTS_MASK, 1U),
              base + SF32LB52_MPI_FIFOCR_OFFSET);

  return pbl_dma_slave_config(mpi->dma, &(struct pbl_dma_slave_config){
                                          .dst_addr = base + SF32LB52_MPI_DR_OFFSET,
                                          .dst_addr_width = PBL_DMA_SLAVE_BUSWIDTH_1_BYTE,
                                        });
}

const struct pbl_spi_mem_ops pbl_spi_mem_sf32lb52_mpi_ops = {
  .init = prv_init,
  .supports_op = prv_supports_op,
  .adjust_op_size = prv_adjust_op_size,
  .exec_op = prv_exec_op,
  .poll_status = prv_poll_status,
  .dirmap_create = prv_dirmap_create,
  .dirmap_read = prv_dirmap_read,
  .dirmap_invalidate = prv_dirmap_invalidate,
  .power_down = prv_power_down,
};
