/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/spi_mem/stm32_qspi.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/stm32h7_quadspi.h>

// Long enough for the longest operation the FIFO waits for, at the lowest clock
#define TIMEOUT_US 100000U

#define MODE_NONE 0U

// 2 ^ (FSIZE + 1) bytes: the whole address space, so that no address is out of range
#define FSIZE_MAX 31U

// Chip select high for 2 cycles between commands
#define CSHT 1U

static const struct pbl_spi_mem_stm32_qspi *prv_qspi(const struct pbl_spi_mem *mem) {
  return PBL_CONTAINER_OF(mem, const struct pbl_spi_mem_stm32_qspi, mem);
}

static inline uint32_t prv_getreg(const struct pbl_spi_mem_stm32_qspi *qspi, uint32_t offset) {
  return pbl_readl(qspi->base + offset);
}

static inline void prv_putreg(const struct pbl_spi_mem_stm32_qspi *qspi, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, qspi->base + offset);
}

// The mode field of a bus width: 1, 2 and 4 lines are modes 1 to 3
static uint32_t prv_mode(uint8_t buswidth) {
  return buswidth == 4U ? 3U : buswidth;
}

static bool prv_buswidth_ok(uint8_t buswidth) {
  return buswidth == 1U || buswidth == 2U || buswidth == 4U;
}

static bool prv_supports_op(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  if (!prv_buswidth_ok(op->cmd.buswidth)) {
    return false;
  }

  if (op->addr.nbytes > 4U || (op->addr.nbytes > 0U && !prv_buswidth_ok(op->addr.buswidth))) {
    return false;
  }

  if (op->dummy.nbytes > 0U && (!prv_buswidth_ok(op->dummy.buswidth) ||
                                op->dummy.nbytes * 8U / op->dummy.buswidth > QUADSPI_DCYC_MAX)) {
    return false;
  }

  return op->data.dir == PBL_SPI_MEM_NO_DATA || prv_buswidth_ok(op->data.buswidth);
}

static int prv_wait(const struct pbl_spi_mem_stm32_qspi *qspi, uint32_t mask) {
  return PBL_POLL_BUSY_US((prv_getreg(qspi, STM32H7_QUADSPI_SR_OFFSET) & mask) != 0U, 0U,
                          TIMEOUT_US);
}

static int prv_exec_op(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  const struct pbl_spi_mem_stm32_qspi *qspi = prv_qspi(mem);
  bool read = op->data.dir == PBL_SPI_MEM_DATA_IN;
  size_t nbytes = op->data.dir != PBL_SPI_MEM_NO_DATA ? op->data.nbytes : 0U;
  uintptr_t dr = qspi->base + STM32H7_QUADSPI_DR_OFFSET;
  uint32_t ccr;
  int ret;

  ret = PBL_POLL_BUSY_US((prv_getreg(qspi, STM32H7_QUADSPI_SR_OFFSET) & QUADSPI_SR_BUSY) == 0U, 0U,
                         TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  prv_putreg(qspi, STM32H7_QUADSPI_FCR_OFFSET, QUADSPI_FCR_ALL);

  ccr = PBL_FIELD_PREP(QUADSPI_CCR_INSTRUCTION_MASK, op->cmd.opcode) |
        PBL_FIELD_PREP(QUADSPI_CCR_IMODE_MASK, prv_mode(op->cmd.buswidth)) |
        PBL_FIELD_PREP(QUADSPI_CCR_FMODE_MASK,
                       read ? QUADSPI_CCR_FMODE_READ : QUADSPI_CCR_FMODE_WRITE);
  if (op->addr.nbytes > 0U) {
    ccr |= PBL_FIELD_PREP(QUADSPI_CCR_ADMODE_MASK, prv_mode(op->addr.buswidth)) |
           PBL_FIELD_PREP(QUADSPI_CCR_ADSIZE_MASK, op->addr.nbytes - 1U);
  }
  if (op->dummy.nbytes > 0U) {
    ccr |= PBL_FIELD_PREP(QUADSPI_CCR_DCYC_MASK, op->dummy.nbytes * 8U / op->dummy.buswidth);
  }
  if (nbytes > 0U) {
    ccr |= PBL_FIELD_PREP(QUADSPI_CCR_DMODE_MASK, prv_mode(op->data.buswidth));
    prv_putreg(qspi, STM32H7_QUADSPI_DLR_OFFSET, (uint32_t)nbytes - 1U);
  }

  // The operation starts on the last of these writes it needs
  prv_putreg(qspi, STM32H7_QUADSPI_CCR_OFFSET, ccr);
  if (op->addr.nbytes > 0U) {
    prv_putreg(qspi, STM32H7_QUADSPI_AR_OFFSET, op->addr.val);
  }

  // The FIFO threshold is one byte: FTF means a byte to read, or room for one to write
  for (size_t i = 0U; i < nbytes; i++) {
    ret = prv_wait(qspi, QUADSPI_SR_FTF);
    if (ret != 0) {
      break;
    }
    if (read) {
      ((uint8_t *)op->data.buf.in)[i] = pbl_readb(dr);
    } else {
      pbl_writeb(((const uint8_t *)op->data.buf.out)[i], dr);
    }
  }

  if (ret == 0) {
    ret = prv_wait(qspi, QUADSPI_SR_TCF);
  }

  if (ret != 0) {
    pbl_modifyl(0U, QUADSPI_CR_ABORT, qspi->base + STM32H7_QUADSPI_CR_OFFSET);
    (void)PBL_POLL_BUSY_US((prv_getreg(qspi, STM32H7_QUADSPI_CR_OFFSET) & QUADSPI_CR_ABORT) == 0U,
                           0U, TIMEOUT_US);
  } else if ((prv_getreg(qspi, STM32H7_QUADSPI_SR_OFFSET) & QUADSPI_SR_TEF) != 0U) {
    ret = -EIO;
  }

  prv_putreg(qspi, STM32H7_QUADSPI_FCR_OFFSET, QUADSPI_FCR_ALL);

  return ret;
}

static int prv_init(const struct pbl_spi_mem *mem) {
  const struct pbl_spi_mem_stm32_qspi *qspi = prv_qspi(mem);
  uint64_t rate_mhz;
  uint32_t clk_hz;
  uint32_t prescaler;
  int ret;

  ret = pbl_device_init(&qspi->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&qspi->clk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_get_rate(&qspi->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_pinctrl_select_state(&qspi->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0) {
    return ret;
  }

  clk_hz = (uint32_t)(rate_mhz / 1000U);
  prescaler = (clk_hz + qspi->max_freq_hz - 1U) / qspi->max_freq_hz;
  if (prescaler == 0U || prescaler > 256U) {
    return -EINVAL;
  }

  prv_putreg(qspi, STM32H7_QUADSPI_CR_OFFSET, 0U);
  prv_putreg(qspi, STM32H7_QUADSPI_DCR_OFFSET,
             PBL_FIELD_PREP(QUADSPI_DCR_FSIZE_MASK, FSIZE_MAX) |
                 PBL_FIELD_PREP(QUADSPI_DCR_CSHT_MASK, CSHT));
  prv_putreg(qspi, STM32H7_QUADSPI_CR_OFFSET,
             PBL_FIELD_PREP(QUADSPI_CR_PRESCALER_MASK, prescaler - 1U) | QUADSPI_CR_SSHIFT |
                 QUADSPI_CR_EN);

  return 0;
}

const struct pbl_spi_mem_ops pbl_spi_mem_stm32_qspi_ops = {
  .init = prv_init,
  .supports_op = prv_supports_op,
  .exec_op = prv_exec_op,
};

#include PBL_DT_INSTANCES(st_stm32f469_qspi)
