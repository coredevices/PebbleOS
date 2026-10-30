/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <pbl/drivers/flash/spi_nor.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

PBL_LOG_MODULE_DECLARE(driver_flash, CONFIG_DRIVER_FLASH_LOG_LEVEL);

#define US_PER_MS            1000U
#define ID_LEN               3U
#define ADDR_3BYTE           3U
#define ADDR_4BYTE           4U
#define ADDR_4BYTE_THRESHOLD (16U * 1024U * 1024U)
#define FAST_READ_DUMMY      1U
#define OTP_READ_DUMMY       1U
#define OTP_PAGE_SIZE        256U

// SFDP (JESD216)
#define SFDP_SIGNATURE       0x50444653U
#define SFDP_HEADER_LEN      8U
#define SFDP_PARAM_HEADER    8U
#define SFDP_MAX_HEADERS     8U
#define SFDP_ADDR_NBYTES     3U
#define SFDP_DUMMY           1U
#define SFDP_BFPT_ID         0xFF00U
#define SFDP_4BAIT_ID        0xFF84U
#define SFDP_BFPT_MAX_DWORDS 16U
#define SFDP_4BAIT_DWORDS    2U
#define SFDP_NUM_ERASE_TYPES 4U

#define BFPT_DW1  0U
#define BFPT_DW2  1U
#define BFPT_DW3  2U
#define BFPT_DW4  3U
#define BFPT_DW8  7U
#define BFPT_DW10 9U
#define BFPT_DW11 10U
#define BFPT_DW16 15U

#define BFPT_DW1_READ_1_1_2      PBL_BIT(16)
#define BFPT_DW1_READ_1_2_2      PBL_BIT(20)
#define BFPT_DW1_READ_1_4_4      PBL_BIT(21)
#define BFPT_DW1_READ_1_1_4      PBL_BIT(22)
#define BFPT_DW2_DENSITY_EXP     PBL_BIT(31)
#define BFPT_DW11_PAGE_SHIFT(dw) (((dw) >> 4) & 0xFU)
#define BFPT_DW16_EN4B_WREN      PBL_BIT(15)
#define BFPT_DW16_EX4B_WREN      PBL_BIT(25)

// 4-byte address instruction table, DWORD 1
#define FBAIT_FAST_1_1_1  PBL_BIT(1)
#define FBAIT_READ_1_1_2  PBL_BIT(2)
#define FBAIT_READ_1_2_2  PBL_BIT(3)
#define FBAIT_READ_1_1_4  PBL_BIT(4)
#define FBAIT_READ_1_4_4  PBL_BIT(5)
#define FBAIT_PP_1_1_1    PBL_BIT(6)
#define FBAIT_PP_1_1_4    PBL_BIT(7)
#define FBAIT_ERASE(type) PBL_BIT(9U + (type))

// JESD216 quad enable requirements
#define QER_NONE        0U
#define QER_SR2_BIT1_V1 1U
#define QER_SR1_BIT6    2U
#define QER_SR2_BIT1_V6 6U

// Erase time units of BFPT DWORD 10, in milliseconds
static const uint16_t s_erase_time_units_ms[] = {1U, 16U, 128U, 1000U};

static const struct pbl_spi_mem_op s_rdsr =
    PBL_SPI_MEM_OP(PBL_SPI_MEM_OP_CMD(PBL_FLASH_NOR_CMD_RDSR, 1), PBL_SPI_MEM_OP_NO_ADDR,
                   PBL_SPI_MEM_OP_NO_DUMMY, PBL_SPI_MEM_OP_DATA_IN(1, nullptr, 1));

// 3-byte address opcodes and their 4-byte address counterparts
static const struct {
  uint8_t op3;
  uint8_t op4;
  uint32_t fbait;
} s_4b_opcodes[] = {
  {0x0BU, 0x0CU, FBAIT_FAST_1_1_1}, {0x3BU, 0x3CU, FBAIT_READ_1_1_2},
  {0xBBU, 0xBCU, FBAIT_READ_1_2_2}, {0x6BU, 0x6CU, FBAIT_READ_1_1_4},
  {0xEBU, 0xECU, FBAIT_READ_1_4_4}, {0x02U, 0x12U, FBAIT_PP_1_1_1},
  {0x32U, 0x34U, FBAIT_PP_1_1_4},
};

struct read_mode {
  uint8_t opcode;
  uint8_t dummy_cycles;
  uint8_t addr_width;
  uint8_t data_width;
};

struct erase_type {
  uint32_t size;
  uint8_t opcode;
  uint8_t idx;
};

struct sfdp {
  uint32_t bfpt[SFDP_BFPT_MAX_DWORDS];
  uint32_t fbait[SFDP_4BAIT_DWORDS];
  uint8_t bfpt_ndwords;
  bool have_bfpt;
  bool have_fbait;
};

struct modes {
  struct read_mode read;
  uint8_t pp_opcode;
  uint8_t pp_data_width;
  uint8_t sector_opcode;
  uint8_t subsector_opcode;
};

static const struct pbl_flash_spi_nor *prv_nor(const struct pbl_flash *flash) {
  return container_of(flash, const struct pbl_flash_spi_nor, flash);
}

static int prv_cmd(const struct pbl_flash_spi_nor *nor, uint8_t opcode) {
  struct pbl_spi_mem_op op = PBL_SPI_MEM_OP(PBL_SPI_MEM_OP_CMD(opcode, 1), PBL_SPI_MEM_OP_NO_ADDR,
                                            PBL_SPI_MEM_OP_NO_DUMMY, PBL_SPI_MEM_OP_NO_DATA);

  return pbl_spi_mem_exec_op(nor->bus, &op);
}

static int prv_read_reg(const struct pbl_flash_spi_nor *nor, uint8_t opcode, void *buf,
                        size_t len) {
  struct pbl_spi_mem_op op =
      PBL_SPI_MEM_OP(PBL_SPI_MEM_OP_CMD(opcode, 1), PBL_SPI_MEM_OP_NO_ADDR, PBL_SPI_MEM_OP_NO_DUMMY,
                     PBL_SPI_MEM_OP_DATA_IN(len, buf, 1));

  return pbl_spi_mem_exec_op(nor->bus, &op);
}

static int prv_busy(const struct pbl_flash_spi_nor *nor, bool *busy) {
  uint8_t sr;
  int ret;

  ret = prv_read_reg(nor, PBL_FLASH_NOR_CMD_RDSR, &sr, sizeof(sr));
  if (ret != 0) {
    return ret;
  }

  *busy = (sr & PBL_FLASH_NOR_SR_WIP) != 0U;

  return 0;
}

static int prv_wait_idle(const struct pbl_flash_spi_nor *nor) {
  bool busy;
  int ret;

  while (true) {
    ret = prv_busy(nor, &busy);
    if (ret != 0 || !busy) {
      return ret;
    }

    if (!pbl_flash_in_coredump(&nor->flash)) {
      pbl_sleep_ms(0U);
    }
  }
}

// Runs an operation that leaves the part busy, returning once it is idle again
static int prv_exec_wait_susp(const struct pbl_flash_spi_nor *nor, const struct pbl_spi_mem_op *op,
                              const struct pbl_spi_mem_suspend *susp) {
  int ret;

  ret = pbl_spi_mem_poll_status(nor->bus, op, &s_rdsr, PBL_FLASH_NOR_SR_WIP, 0U, susp);
  if (ret != -ENOTSUP) {
    return ret;
  }

  ret = pbl_spi_mem_exec_op(nor->bus, op);
  if (ret != 0) {
    return ret;
  }

  return prv_wait_idle(nor);
}

static int prv_exec_wait(const struct pbl_flash_spi_nor *nor, const struct pbl_spi_mem_op *op) {
  return prv_exec_wait_susp(nor, op, nullptr);
}

static int prv_write_reg(const struct pbl_flash_spi_nor *nor, uint8_t opcode, const void *buf,
                         size_t len) {
  struct pbl_spi_mem_op op =
      PBL_SPI_MEM_OP(PBL_SPI_MEM_OP_CMD(opcode, 1), PBL_SPI_MEM_OP_NO_ADDR, PBL_SPI_MEM_OP_NO_DUMMY,
                     PBL_SPI_MEM_OP_DATA_OUT(len, buf, 1));
  int ret;

  ret = prv_cmd(nor, PBL_FLASH_NOR_CMD_WREN);
  if (ret != 0) {
    return ret;
  }

  return prv_exec_wait(nor, &op);
}

// Writes status register 2 the way the part takes it, keeping status register 1
static int prv_write_sr2(const struct pbl_flash_spi_nor *nor, uint8_t sr2) {
  uint8_t sr[2];
  int ret;

  if (nor->data->qer == QER_SR2_BIT1_V6) {
    return prv_write_reg(nor, PBL_FLASH_NOR_CMD_WRSR2, &sr2, 1U);
  }

  ret = prv_read_reg(nor, PBL_FLASH_NOR_CMD_RDSR, &sr[0], 1U);
  if (ret != 0) {
    return ret;
  }

  sr[1] = sr2;

  return prv_write_reg(nor, PBL_FLASH_NOR_CMD_WRSR, sr, sizeof(sr));
}

// Writes status register 1 the way the part takes it, keeping status register 2
static int prv_write_sr1(const struct pbl_flash_spi_nor *nor, uint8_t sr1) {
  uint8_t sr[2] = {sr1};
  int ret;

  if (nor->data->qer == QER_SR2_BIT1_V6 || nor->data->qer == QER_SR1_BIT6) {
    return prv_write_reg(nor, PBL_FLASH_NOR_CMD_WRSR, sr, 1U);
  }

  ret = prv_read_reg(nor, PBL_FLASH_NOR_CMD_RDSR2, &sr[1], 1U);
  if (ret != 0) {
    return ret;
  }

  return prv_write_reg(nor, PBL_FLASH_NOR_CMD_WRSR, sr, sizeof(sr));
}

// Runs an operation in as many pieces as the controller needs
static int prv_exec_chunked(const struct pbl_flash_spi_nor *nor, struct pbl_spi_mem_op *op) {
  size_t remaining = op->data.nbytes;
  int ret;

  while (remaining > 0U) {
    op->data.nbytes = remaining;
    ret = pbl_spi_mem_adjust_op_size(nor->bus, op);
    if (ret != 0) {
      return ret;
    }

    if (op->data.nbytes == 0U) {
      return -EINVAL;
    }

    ret = pbl_spi_mem_exec_op(nor->bus, op);
    if (ret != 0) {
      return ret;
    }

    remaining -= op->data.nbytes;
    op->addr.val += op->data.nbytes;
    op->data.buf.in = (uint8_t *)op->data.buf.in + op->data.nbytes;
  }

  return 0;
}

static uint32_t prv_le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int prv_read_sfdp(const struct pbl_flash_spi_nor *nor, uint32_t addr, void *buf,
                         size_t len) {
  struct pbl_spi_mem_op op =
      PBL_SPI_MEM_OP(PBL_SPI_MEM_OP_CMD(PBL_FLASH_NOR_CMD_RDSFDP, 1),
                     PBL_SPI_MEM_OP_ADDR(SFDP_ADDR_NBYTES, addr, 1),
                     PBL_SPI_MEM_OP_DUMMY(SFDP_DUMMY, 1), PBL_SPI_MEM_OP_DATA_IN(len, buf, 1));

  return prv_exec_chunked(nor, &op);
}

static int prv_read_table(const struct pbl_flash_spi_nor *nor, const uint8_t *ph, uint32_t *dw,
                          uint8_t max_dwords, uint8_t *ndwords) {
  uint8_t raw[SFDP_BFPT_MAX_DWORDS * sizeof(uint32_t)];
  uint32_t ptp = (uint32_t)ph[4] | ((uint32_t)ph[5] << 8) | ((uint32_t)ph[6] << 16);
  uint8_t n = MIN(ph[3], max_dwords);
  int ret;

  ret = prv_read_sfdp(nor, ptp, raw, n * sizeof(uint32_t));
  if (ret != 0) {
    return ret;
  }

  for (uint8_t i = 0U; i < n; i++) {
    dw[i] = prv_le32(&raw[i * sizeof(uint32_t)]);
  }

  *ndwords = n;

  return 0;
}

// Returns 0, also when the part has no usable SFDP
static int prv_read_sfdp_tables(const struct pbl_flash_spi_nor *nor, struct sfdp *sfdp) {
  uint8_t hdr[SFDP_HEADER_LEN];
  uint8_t params[SFDP_MAX_HEADERS * SFDP_PARAM_HEADER];
  uint8_t nph;
  uint8_t n;
  int ret;

  memset(sfdp, 0, sizeof(*sfdp));

  ret = prv_read_sfdp(nor, 0U, hdr, sizeof(hdr));
  if (ret != 0) {
    return ret;
  }

  if (prv_le32(hdr) != SFDP_SIGNATURE) {
    return 0;
  }

  nph = MIN(hdr[6] + 1U, SFDP_MAX_HEADERS);
  ret = prv_read_sfdp(nor, SFDP_HEADER_LEN, params, nph * SFDP_PARAM_HEADER);
  if (ret != 0) {
    return ret;
  }

  for (uint8_t i = 0U; i < nph; i++) {
    const uint8_t *ph = &params[i * SFDP_PARAM_HEADER];
    uint16_t id = (uint16_t)(ph[0] | (ph[7] << 8));

    if (ph[3] == 0U) {
      continue;
    }

    if (id == SFDP_BFPT_ID && !sfdp->have_bfpt) {
      ret = prv_read_table(nor, ph, sfdp->bfpt, SFDP_BFPT_MAX_DWORDS, &sfdp->bfpt_ndwords);
      sfdp->have_bfpt = ret == 0;
    } else if (id == SFDP_4BAIT_ID && !sfdp->have_fbait) {
      ret = prv_read_table(nor, ph, sfdp->fbait, SFDP_4BAIT_DWORDS, &n);
      sfdp->have_fbait = ret == 0 && n == SFDP_4BAIT_DWORDS;
    }

    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

static struct read_mode prv_bfpt_read_mode(uint16_t half, uint8_t addr_width, uint8_t data_width) {
  return (struct read_mode){
    .opcode = (uint8_t)(half >> 8),
    .dummy_cycles = (uint8_t)((half & 0x1FU) + ((half >> 5) & 0x7U)),
    .addr_width = addr_width,
    .data_width = data_width,
  };
}

static void prv_apply_bfpt(const struct pbl_flash_spi_nor *nor, const struct sfdp *sfdp,
                           struct erase_type *small, struct erase_type *large) {
  struct pbl_flash_geometry *geo = &nor->data->geometry;
  const uint32_t *dw = sfdp->bfpt;
  uint64_t bits;

  bits = ((dw[BFPT_DW2] & BFPT_DW2_DENSITY_EXP) != 0U)
             ? (1ULL << (dw[BFPT_DW2] & ~BFPT_DW2_DENSITY_EXP))
             : ((uint64_t)dw[BFPT_DW2] + 1U);
  geo->size = (uint32_t)(bits / 8U);

  if (sfdp->bfpt_ndwords > BFPT_DW11) {
    geo->page_size = 1U << BFPT_DW11_PAGE_SHIFT(dw[BFPT_DW11]);
  }

  // The smallest erase type is the subsector, the largest the sector
  for (uint8_t i = 0U; i < SFDP_NUM_ERASE_TYPES; i++) {
    uint32_t d = dw[BFPT_DW8 + (i / 2U)];
    uint16_t half = (uint16_t)(((i % 2U) != 0U) ? (d >> 16) : d);
    uint8_t shift = (uint8_t)half;
    struct erase_type type = {
      .size = (shift != 0U) ? (1U << shift) : 0U,
      .opcode = (uint8_t)(half >> 8),
      .idx = i,
    };
    uint16_t time_ms = 0U;

    if (type.size == 0U) {
      continue;
    }

    if (sfdp->bfpt_ndwords > BFPT_DW10) {
      uint32_t t = (dw[BFPT_DW10] >> (4U + (i * 7U))) & 0x7FU;
      time_ms = (uint16_t)(((t & 0x1FU) + 1U) * s_erase_time_units_ms[(t >> 5) & 0x3U]);
    }

    if (small->size == 0U || type.size < small->size) {
      *small = type;
      geo->subsector_size = type.size;
      if (time_ms != 0U) {
        geo->subsector_erase_ms = time_ms;
      }
    }

    if (type.size > large->size) {
      *large = type;
      geo->sector_size = type.size;
      if (time_ms != 0U) {
        geo->sector_erase_ms = time_ms;
      }
    }
  }
}

static uint8_t prv_part_qer(const struct pbl_flash_nor_part *part) {
  switch (part->qe) {
    case PBL_FLASH_NOR_QE_SR2_BIT1_WR_SR1:
      return QER_SR2_BIT1_V1;
    case PBL_FLASH_NOR_QE_SR1_BIT6:
      return QER_SR1_BIT6;
    case PBL_FLASH_NOR_QE_SR2_BIT1:
      return QER_SR2_BIT1_V6;
    default:
      return QER_NONE;
  }
}

static int prv_unprotect(const struct pbl_flash_spi_nor *nor) {
  const uint8_t mask = nor->part->sr1_bp_mask;
  uint8_t sr;
  int ret;

  ret = prv_read_reg(nor, PBL_FLASH_NOR_CMD_RDSR, &sr, 1U);
  if (ret != 0 || (sr & mask) == 0U) {
    return ret;
  }

  ret = prv_write_sr1(nor, sr & ~mask);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read_reg(nor, PBL_FLASH_NOR_CMD_RDSR, &sr, 1U);
  if (ret != 0) {
    return ret;
  }

  return ((sr & mask) == 0U) ? 0 : -EIO;
}

static int prv_set_quad_enable(const struct pbl_flash_spi_nor *nor) {
  const bool sr1 = nor->data->qer == QER_SR1_BIT6;
  const uint8_t rdsr = sr1 ? PBL_FLASH_NOR_CMD_RDSR : PBL_FLASH_NOR_CMD_RDSR2;
  const uint8_t qe = sr1 ? PBL_FLASH_NOR_SR_QE : PBL_FLASH_NOR_SR2_QE;
  uint8_t sr;
  int ret;

  ret = prv_read_reg(nor, rdsr, &sr, 1U);
  if (ret != 0 || (sr & qe) != 0U) {
    return ret;
  }

  sr |= qe;
  ret = sr1 ? prv_write_reg(nor, PBL_FLASH_NOR_CMD_WRSR, &sr, 1U) : prv_write_sr2(nor, sr);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read_reg(nor, rdsr, &sr, 1U);
  if (ret != 0) {
    return ret;
  }

  return ((sr & qe) != 0U) ? 0 : -EIO;
}

// The 4-byte address counterpart of an opcode, or 0 if the part does not have it
static uint8_t prv_opcode_4b(const struct pbl_flash_spi_nor *nor, const struct sfdp *sfdp,
                             uint8_t op3) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_4b_opcodes); i++) {
    if (s_4b_opcodes[i].op3 != op3) {
      continue;
    }

    if (sfdp->have_fbait) {
      return ((sfdp->fbait[0] & s_4b_opcodes[i].fbait) != 0U) ? s_4b_opcodes[i].op4 : 0U;
    }

    return ((nor->part->flags & PBL_FLASH_NOR_F_4B_OPCODES) != 0U) ? s_4b_opcodes[i].op4 : 0U;
  }

  return 0U;
}

static uint8_t prv_erase_opcode_4b(const struct pbl_flash_spi_nor *nor, const struct sfdp *sfdp,
                                   const struct erase_type *type) {
  if (sfdp->have_fbait) {
    if ((sfdp->fbait[0] & FBAIT_ERASE(type->idx)) == 0U) {
      return 0U;
    }

    return (uint8_t)(sfdp->fbait[1] >> (8U * type->idx));
  }

  if ((nor->part->flags & PBL_FLASH_NOR_F_4B_OPCODES) == 0U) {
    return 0U;
  }

  switch (type->opcode) {
    case PBL_FLASH_NOR_CMD_SE:
      return PBL_FLASH_NOR_CMD_SE_4B;
    case PBL_FLASH_NOR_CMD_BE:
      return PBL_FLASH_NOR_CMD_BE_4B;
    default:
      return 0U;
  }
}

static bool prv_can_read(const struct pbl_flash_spi_nor *nor, const struct read_mode *mode,
                         uint8_t addr_nbytes) {
  struct pbl_spi_mem_op op = PBL_SPI_MEM_OP(
      PBL_SPI_MEM_OP_CMD(mode->opcode, 1), PBL_SPI_MEM_OP_ADDR(addr_nbytes, 0U, mode->addr_width),
      PBL_SPI_MEM_OP_DUMMY(mode->dummy_cycles * mode->addr_width / 8U, mode->addr_width),
      PBL_SPI_MEM_OP_DATA_IN(nor->data->geometry.page_size, nullptr, mode->data_width));

  return mode->opcode != 0U && pbl_spi_mem_supports_op(nor->bus, &op);
}

static bool prv_can_program(const struct pbl_flash_spi_nor *nor, uint8_t opcode, uint8_t data_width,
                            uint8_t addr_nbytes) {
  struct pbl_spi_mem_op op =
      PBL_SPI_MEM_OP(PBL_SPI_MEM_OP_CMD(opcode, 1), PBL_SPI_MEM_OP_ADDR(addr_nbytes, 0U, 1),
                     PBL_SPI_MEM_OP_NO_DUMMY,
                     PBL_SPI_MEM_OP_DATA_OUT(nor->data->geometry.page_size, nullptr, data_width));

  return opcode != 0U && pbl_spi_mem_supports_op(nor->bus, &op);
}

static bool prv_can_erase(const struct pbl_flash_spi_nor *nor, uint8_t opcode,
                          uint8_t addr_nbytes) {
  struct pbl_spi_mem_op op =
      PBL_SPI_MEM_OP(PBL_SPI_MEM_OP_CMD(opcode, 1), PBL_SPI_MEM_OP_ADDR(addr_nbytes, 0U, 1),
                     PBL_SPI_MEM_OP_NO_DUMMY, PBL_SPI_MEM_OP_NO_DATA);

  return opcode != 0U && pbl_spi_mem_supports_op(nor->bus, &op);
}

// Picks the widest read and program modes both the part and the controller support, with the
// 4-byte address opcodes if four_byte is set
static int prv_select_modes(const struct pbl_flash_spi_nor *nor, const struct sfdp *sfdp,
                            const struct erase_type *small, const struct erase_type *large,
                            bool four_byte, struct modes *out) {
  const uint8_t addr_nbytes = nor->data->addr_nbytes;
  const bool quad = nor->data->qer != QER_NONE;
  struct read_mode modes[5];
  size_t n = 0U;
  bool found = false;
  uint8_t op;

  if (sfdp->have_bfpt) {
    const uint32_t *dw = sfdp->bfpt;

    if ((dw[BFPT_DW1] & BFPT_DW1_READ_1_4_4) != 0U && quad) {
      modes[n++] = prv_bfpt_read_mode((uint16_t)dw[BFPT_DW3], 4U, 4U);
    }
    if ((dw[BFPT_DW1] & BFPT_DW1_READ_1_1_4) != 0U && quad) {
      modes[n++] = prv_bfpt_read_mode((uint16_t)(dw[BFPT_DW3] >> 16), 1U, 4U);
    }
    if ((dw[BFPT_DW1] & BFPT_DW1_READ_1_2_2) != 0U) {
      modes[n++] = prv_bfpt_read_mode((uint16_t)(dw[BFPT_DW4] >> 16), 2U, 2U);
    }
    if ((dw[BFPT_DW1] & BFPT_DW1_READ_1_1_2) != 0U) {
      modes[n++] = prv_bfpt_read_mode((uint16_t)dw[BFPT_DW4], 1U, 2U);
    }
  }

  modes[n++] = (struct read_mode){
    .opcode = PBL_FLASH_NOR_CMD_FAST_READ,
    .dummy_cycles = FAST_READ_DUMMY * 8U,
    .addr_width = 1U,
    .data_width = 1U,
  };

  for (size_t i = 0U; i < n && !found; i++) {
    out->read = modes[i];
    if (four_byte) {
      out->read.opcode = prv_opcode_4b(nor, sfdp, modes[i].opcode);
    }
    found = prv_can_read(nor, &out->read, addr_nbytes);
  }

  if (!found) {
    return -ENOTSUP;
  }

  op =
      four_byte ? prv_opcode_4b(nor, sfdp, PBL_FLASH_NOR_CMD_PP_1_1_4) : PBL_FLASH_NOR_CMD_PP_1_1_4;
  if (quad && prv_can_program(nor, op, 4U, addr_nbytes)) {
    out->pp_opcode = op;
    out->pp_data_width = 4U;
  } else {
    op = four_byte ? prv_opcode_4b(nor, sfdp, PBL_FLASH_NOR_CMD_PP) : PBL_FLASH_NOR_CMD_PP;
    if (!prv_can_program(nor, op, 1U, addr_nbytes)) {
      return -ENOTSUP;
    }
    out->pp_opcode = op;
    out->pp_data_width = 1U;
  }

  out->subsector_opcode = four_byte ? prv_erase_opcode_4b(nor, sfdp, small) : small->opcode;
  out->sector_opcode = four_byte ? prv_erase_opcode_4b(nor, sfdp, large) : large->opcode;
  if (!prv_can_erase(nor, out->subsector_opcode, addr_nbytes) ||
      !prv_can_erase(nor, out->sector_opcode, addr_nbytes)) {
    return -ENOTSUP;
  }

  return 0;
}

static int prv_set_addr_mode(const struct pbl_flash_spi_nor *nor, uint8_t opcode, bool wren) {
  int ret;

  if (wren) {
    ret = prv_cmd(nor, PBL_FLASH_NOR_CMD_WREN);
    if (ret != 0) {
      return ret;
    }
  }

  return prv_cmd(nor, opcode);
}

static int prv_probe(const struct pbl_flash_spi_nor *nor) {
  const struct pbl_flash_nor_part *part = nor->part;
  struct pbl_flash_spi_nor_data *data = nor->data;
  struct erase_type small = {
    .size = part->geometry.subsector_size,
    .opcode = PBL_FLASH_NOR_CMD_SE,
  };
  struct erase_type large = {
    .size = part->geometry.sector_size,
    .opcode = PBL_FLASH_NOR_CMD_BE,
  };
  struct sfdp sfdp;
  struct modes modes;
  bool four_byte = false;
  bool en4b_wren = false;
  int ret;

  data->geometry = part->geometry;
  data->qer = prv_part_qer(part);

  ret = prv_read_sfdp_tables(nor, &sfdp);
  if (ret != 0) {
    return ret;
  }

  if (sfdp.have_bfpt) {
    small.size = 0U;
    large.size = 0U;
    prv_apply_bfpt(nor, &sfdp, &small, &large);
    if (sfdp.bfpt_ndwords > BFPT_DW16) {
      en4b_wren = (sfdp.bfpt[BFPT_DW16] & BFPT_DW16_EN4B_WREN) != 0U;
      data->ex4b_wren = (sfdp.bfpt[BFPT_DW16] & BFPT_DW16_EX4B_WREN) != 0U;
    }
  } else {
    PBL_LOG_DBG("No SFDP, using the %s description", part->name);
  }

  if (data->geometry.size == 0U || data->geometry.page_size == 0U ||
      data->geometry.subsector_size == 0U || data->geometry.sector_size == 0U) {
    return -ENODEV;
  }

  data->addr_nbytes = (data->geometry.size > ADDR_4BYTE_THRESHOLD) ? ADDR_4BYTE : ADDR_3BYTE;
  if (data->addr_nbytes == ADDR_4BYTE) {
    four_byte = prv_select_modes(nor, &sfdp, &small, &large, true, &modes) == 0;
  }

  if (!four_byte) {
    ret = prv_select_modes(nor, &sfdp, &small, &large, false, &modes);
    if (ret != 0) {
      return ret;
    }
  }

  data->pp_opcode = modes.pp_opcode;
  data->pp_data_width = modes.pp_data_width;
  data->sector_opcode = modes.sector_opcode;
  data->subsector_opcode = modes.subsector_opcode;
  data->en4b = data->addr_nbytes == ADDR_4BYTE && !four_byte;
  data->reg_addr_nbytes = data->en4b ? ADDR_4BYTE : ADDR_3BYTE;

  if (modes.read.data_width == 4U || data->pp_data_width == 4U) {
    ret = prv_set_quad_enable(nor);
    if (ret != 0) {
      return ret;
    }
  }

  ret = prv_unprotect(nor);
  if (ret != 0) {
    return ret;
  }

  data->dirmap = (struct pbl_spi_mem_dirmap){
    .op = PBL_SPI_MEM_OP(PBL_SPI_MEM_OP_CMD(modes.read.opcode, 1),
                         PBL_SPI_MEM_OP_ADDR(data->addr_nbytes, 0U, modes.read.addr_width),
                         PBL_SPI_MEM_OP_DUMMY(modes.read.dummy_cycles * modes.read.addr_width / 8U,
                                              modes.read.addr_width),
                         PBL_SPI_MEM_OP_DATA_IN(0U, nullptr, modes.read.data_width)),
    .offset = 0U,
    .length = data->geometry.size,
  };

  // The window changes before the address mode: with 4-byte opcodes it reads the same in both
  ret = pbl_spi_mem_dirmap_create(nor->bus, &data->dirmap);
  if (ret != 0) {
    return ret;
  }

  if (data->addr_nbytes == ADDR_4BYTE) {
    ret = data->en4b ? prv_set_addr_mode(nor, PBL_FLASH_NOR_CMD_EN4B, en4b_wren)
                     : prv_set_addr_mode(nor, PBL_FLASH_NOR_CMD_EX4B, data->ex4b_wren);
    if (ret != 0) {
      return ret;
    }
  }

  data->susp = (struct pbl_spi_mem_suspend){
    .suspend =
        PBL_SPI_MEM_OP(PBL_SPI_MEM_OP_CMD(PBL_FLASH_NOR_CMD_SUSPEND, 1), PBL_SPI_MEM_OP_NO_ADDR,
                       PBL_SPI_MEM_OP_NO_DUMMY, PBL_SPI_MEM_OP_NO_DATA),
    .resume =
        PBL_SPI_MEM_OP(PBL_SPI_MEM_OP_CMD(PBL_FLASH_NOR_CMD_RESUME, 1), PBL_SPI_MEM_OP_NO_ADDR,
                       PBL_SPI_MEM_OP_NO_DUMMY, PBL_SPI_MEM_OP_NO_DATA),
    .suspend_us = part->suspend_us,
    .resume_us = part->resume_us,
  };

  data->pm = (struct pbl_spi_mem_pm){
    .enter = PBL_SPI_MEM_OP(PBL_SPI_MEM_OP_CMD(PBL_FLASH_NOR_CMD_DPD, 1), PBL_SPI_MEM_OP_NO_ADDR,
                            PBL_SPI_MEM_OP_NO_DUMMY, PBL_SPI_MEM_OP_NO_DATA),
    .enter_us = part->dpd_enter_us,
    .exit = PBL_SPI_MEM_OP(PBL_SPI_MEM_OP_CMD(PBL_FLASH_NOR_CMD_RDPD, 1), PBL_SPI_MEM_OP_NO_ADDR,
                           PBL_SPI_MEM_OP_NO_DUMMY, PBL_SPI_MEM_OP_NO_DATA),
    .exit_us = part->dpd_exit_us,
  };

  return 0;
}

static int prv_init(const struct pbl_flash *flash) {
  const struct pbl_flash_spi_nor *nor = prv_nor(flash);
  uint8_t id[ID_LEN];
  uint32_t jedec_id;
  int ret;

  pbl_spi_mem_set_polling(nor->bus, pbl_flash_in_coredump(flash));

  if (nor->data->initialized) {
    return 0;
  }

  if (!pbl_device_is_ready(&nor->bus->dev)) {
    return -ENODEV;
  }

  // Abort any program or erase left over from before the reset, unless the CPU runs from the part
  if (!pbl_spi_mem_is_xip(nor->bus)) {
    ret = prv_cmd(nor, PBL_FLASH_NOR_CMD_RSTEN);
    if (ret != 0) {
      return ret;
    }

    ret = prv_cmd(nor, PBL_FLASH_NOR_CMD_RST);
    if (ret != 0) {
      return ret;
    }

    if (pbl_flash_in_coredump(flash)) {
      pbl_busy_wait_us(nor->part->reset_ms * US_PER_MS);
    } else {
      pbl_sleep_ms(nor->part->reset_ms);
    }
  }

  ret = prv_read_reg(nor, PBL_FLASH_NOR_CMD_RDID, id, sizeof(id));
  if (ret != 0) {
    return ret;
  }

  jedec_id = (uint32_t)id[0] | ((uint32_t)id[1] << 8) | ((uint32_t)id[2] << 16);
  if (jedec_id != nor->part->id) {
    PBL_LOG_WRN("Flash is not %s (id 0x%06" PRIx32 ")", nor->part->name, jedec_id);
  }

  ret = prv_probe(nor);
  if (ret != 0) {
    return ret;
  }

  PBL_LOG_DBG("%" PRIu32 " bytes, erase %" PRIu32 "/%" PRIu32 ", read 0x%02x, program 0x%02x",
              nor->data->geometry.size, nor->data->geometry.subsector_size,
              nor->data->geometry.sector_size, nor->data->dirmap.op.cmd.opcode,
              nor->data->pp_opcode);

  nor->data->initialized = true;

  return 0;
}

static int prv_read_op(const struct pbl_flash_spi_nor *nor, uint32_t offs, void *buf, size_t len) {
  struct pbl_spi_mem_op op = nor->data->dirmap.op;

  op.addr.val = offs;
  op.data.buf.in = buf;
  op.data.nbytes = len;

  return prv_exec_chunked(nor, &op);
}

// Through the window where it reaches, through operations beyond
static int prv_read(const struct pbl_flash *flash, uint32_t addr, void *buf, size_t len) {
  const struct pbl_flash_spi_nor *nor = prv_nor(flash);
  const struct pbl_spi_mem_dirmap *dm = &nor->data->dirmap;
  uint32_t offs = addr - flash->base;
  uint8_t *dst = buf;
  int ret;

  while (len > 0U) {
    size_t chunk;

    if (offs >= dm->offset && (offs - dm->offset) < dm->length) {
      chunk = MIN(len, dm->length - (offs - dm->offset));
      ret = pbl_spi_mem_dirmap_read(nor->bus, dm, offs, dst, chunk);
    } else {
      chunk = (offs < dm->offset) ? MIN(len, dm->offset - offs) : len;
      ret = prv_read_op(nor, offs, dst, chunk);
    }

    if (ret != 0) {
      return ret;
    }

    offs += chunk;
    dst += chunk;
    len -= chunk;
  }

  return 0;
}

static int prv_write(const struct pbl_flash *flash, uint32_t addr, const void *buf, size_t len) {
  const struct pbl_flash_spi_nor *nor = prv_nor(flash);
  const struct pbl_flash_spi_nor_data *data = nor->data;
  const uint32_t page_size = data->geometry.page_size;
  const uint32_t start = addr - flash->base;
  const size_t total = len;
  uint32_t offset = start;
  const uint8_t *src = buf;
  int ret = 0;

  while (len > 0U) {
    struct pbl_spi_mem_op op =
        PBL_SPI_MEM_OP(PBL_SPI_MEM_OP_CMD(data->pp_opcode, 1),
                       PBL_SPI_MEM_OP_ADDR(data->addr_nbytes, offset, 1), PBL_SPI_MEM_OP_NO_DUMMY,
                       PBL_SPI_MEM_OP_DATA_OUT(MIN(len, page_size - (offset % page_size)), src,
                                               data->pp_data_width));

    ret = pbl_spi_mem_adjust_op_size(nor->bus, &op);
    if (ret == 0 && op.data.nbytes == 0U) {
      ret = -EINVAL;
    }
    if (ret == 0) {
      ret = prv_cmd(nor, PBL_FLASH_NOR_CMD_WREN);
    }
    if (ret == 0) {
      ret = prv_exec_wait(nor, &op);
    }
    if (ret != 0) {
      break;
    }

    offset += op.data.nbytes;
    src += op.data.nbytes;
    len -= op.data.nbytes;
  }

  pbl_spi_mem_dirmap_invalidate(nor->bus, &data->dirmap, start, total);

  return ret;
}

static int prv_erase(const struct pbl_flash *flash, uint32_t addr, uint32_t size) {
  const struct pbl_flash_spi_nor *nor = prv_nor(flash);
  const struct pbl_flash_spi_nor_data *data = nor->data;
  const uint32_t offset = addr - flash->base;
  struct pbl_spi_mem_op op;
  uint8_t opcode;
  int ret;

  if (size == data->geometry.sector_size) {
    opcode = data->sector_opcode;
  } else if (size == data->geometry.subsector_size) {
    opcode = data->subsector_opcode;
  } else {
    return -EINVAL;
  }

  ret = prv_cmd(nor, PBL_FLASH_NOR_CMD_WREN);
  if (ret != 0) {
    return ret;
  }

  op = PBL_SPI_MEM_OP(PBL_SPI_MEM_OP_CMD(opcode, 1),
                      PBL_SPI_MEM_OP_ADDR(data->addr_nbytes, offset, 1), PBL_SPI_MEM_OP_NO_DUMMY,
                      PBL_SPI_MEM_OP_NO_DATA);

  // The CPU cannot run from the part while it erases, so the erase completes here, suspended
  // whenever interrupts need to run
  if (pbl_spi_mem_is_xip(nor->bus)) {
    ret = prv_exec_wait_susp(nor, &op, &data->susp);
    pbl_spi_mem_dirmap_invalidate(nor->bus, &data->dirmap, offset, size);
    return (ret == 0) ? 1 : ret;
  }

  ret = pbl_spi_mem_exec_op(nor->bus, &op);
  pbl_spi_mem_dirmap_invalidate(nor->bus, &data->dirmap, offset, size);

  return ret;
}

static int prv_erase_status(const struct pbl_flash *flash) {
  const struct pbl_flash_spi_nor *nor = prv_nor(flash);
  uint8_t sr2;
  bool busy;
  int ret;

  ret = prv_busy(nor, &busy);
  if (ret != 0) {
    return ret;
  }

  if (busy) {
    return -EBUSY;
  }

  ret = prv_read_reg(nor, PBL_FLASH_NOR_CMD_RDSR2, &sr2, sizeof(sr2));
  if (ret != 0) {
    return ret;
  }

  return ((sr2 & PBL_FLASH_NOR_SR2_SUS) != 0U) ? -EAGAIN : 0;
}

static int prv_erase_suspend(const struct pbl_flash *flash) {
  const struct pbl_flash_spi_nor *nor = prv_nor(flash);
  bool busy;
  int ret;

  ret = prv_busy(nor, &busy);
  if (ret != 0) {
    return ret;
  }

  if (!busy) {
    return 1;
  }

  ret = prv_cmd(nor, PBL_FLASH_NOR_CMD_SUSPEND);
  if (ret != 0) {
    return ret;
  }

  pbl_busy_wait_us(nor->part->suspend_us);

  return 0;
}

static int prv_erase_resume(const struct pbl_flash *flash) {
  return prv_cmd(prv_nor(flash), PBL_FLASH_NOR_CMD_RESUME);
}

static void prv_power_down(const struct pbl_flash *flash, pbl_flash_pm_fn_t fn, void *arg) {
  const struct pbl_flash_spi_nor *nor = prv_nor(flash);

  pbl_spi_mem_power_down(nor->bus, &nor->data->pm, fn, arg);
}

// Leaves the part as it powers up; the next init probes it again
static void prv_stop(const struct pbl_flash *flash) {
  const struct pbl_flash_spi_nor *nor = prv_nor(flash);

  if (nor->data->en4b) {
    (void)prv_set_addr_mode(nor, PBL_FLASH_NOR_CMD_EX4B, nor->data->ex4b_wren);
  }

  nor->data->initialized = false;
}

static uint32_t prv_otp_addr(const struct pbl_flash_spi_nor *nor, uint8_t region, uint32_t offs) {
  const struct pbl_flash_nor_otp *otp = &nor->part->otp;

  return otp->base + ((uint32_t)region * otp->offset) + offs;
}

static int prv_otp_read(const struct pbl_flash *flash, uint8_t region, uint32_t offs, void *buf,
                        size_t len) {
  const struct pbl_flash_spi_nor *nor = prv_nor(flash);
  struct pbl_spi_mem_op op = PBL_SPI_MEM_OP(
      PBL_SPI_MEM_OP_CMD(PBL_FLASH_NOR_CMD_RDSCUR, 1),
      PBL_SPI_MEM_OP_ADDR(nor->data->reg_addr_nbytes, prv_otp_addr(nor, region, offs), 1),
      PBL_SPI_MEM_OP_DUMMY(OTP_READ_DUMMY, 1), PBL_SPI_MEM_OP_DATA_IN(len, buf, 1));

  return prv_exec_chunked(nor, &op);
}

// Programs within the register's 256-byte pages
static int prv_otp_write(const struct pbl_flash *flash, uint8_t region, uint32_t offs,
                         const void *buf, size_t len) {
  const struct pbl_flash_spi_nor *nor = prv_nor(flash);
  uint32_t addr = prv_otp_addr(nor, region, offs);
  const uint8_t *src = buf;
  int ret;

  while (len > 0U) {
    struct pbl_spi_mem_op op = PBL_SPI_MEM_OP(
        PBL_SPI_MEM_OP_CMD(PBL_FLASH_NOR_CMD_PRSCUR, 1),
        PBL_SPI_MEM_OP_ADDR(nor->data->reg_addr_nbytes, addr, 1), PBL_SPI_MEM_OP_NO_DUMMY,
        PBL_SPI_MEM_OP_DATA_OUT(MIN(len, OTP_PAGE_SIZE - (addr % OTP_PAGE_SIZE)), src, 1));

    ret = pbl_spi_mem_adjust_op_size(nor->bus, &op);
    if (ret != 0) {
      return ret;
    }

    if (op.data.nbytes == 0U) {
      return -EINVAL;
    }

    ret = prv_cmd(nor, PBL_FLASH_NOR_CMD_WREN);
    if (ret != 0) {
      return ret;
    }

    ret = prv_exec_wait(nor, &op);
    if (ret != 0) {
      return ret;
    }

    addr += op.data.nbytes;
    src += op.data.nbytes;
    len -= op.data.nbytes;
  }

  return 0;
}

static uint8_t prv_otp_lb(const struct pbl_flash_spi_nor *nor, uint8_t region) {
  return (uint8_t)PBL_BIT(nor->part->otp.lb_bit + region);
}

static int prv_otp_is_locked(const struct pbl_flash *flash, uint8_t region, bool *locked) {
  const struct pbl_flash_spi_nor *nor = prv_nor(flash);
  uint8_t sr2;
  int ret;

  ret = prv_read_reg(nor, PBL_FLASH_NOR_CMD_RDSR2, &sr2, sizeof(sr2));
  if (ret != 0) {
    return ret;
  }

  *locked = (sr2 & prv_otp_lb(nor, region)) != 0U;

  return 0;
}

static int prv_otp_lock(const struct pbl_flash *flash, uint8_t region) {
  const struct pbl_flash_spi_nor *nor = prv_nor(flash);
  uint8_t sr2;
  int ret;

  ret = prv_read_reg(nor, PBL_FLASH_NOR_CMD_RDSR2, &sr2, sizeof(sr2));
  if (ret != 0) {
    return ret;
  }

  ret = prv_write_sr2(nor, sr2 | prv_otp_lb(nor, region));
  if (ret != 0) {
    return ret;
  }

  ret = prv_read_reg(nor, PBL_FLASH_NOR_CMD_RDSR2, &sr2, sizeof(sr2));
  if (ret != 0) {
    return ret;
  }

  return ((sr2 & prv_otp_lb(nor, region)) != 0U) ? 0 : -EIO;
}

static int prv_otp_erase(const struct pbl_flash *flash, uint8_t region) {
  const struct pbl_flash_spi_nor *nor = prv_nor(flash);
  struct pbl_spi_mem_op op = PBL_SPI_MEM_OP(
      PBL_SPI_MEM_OP_CMD(PBL_FLASH_NOR_CMD_ERSCUR, 1),
      PBL_SPI_MEM_OP_ADDR(nor->data->reg_addr_nbytes, prv_otp_addr(nor, region, 0U), 1),
      PBL_SPI_MEM_OP_NO_DUMMY, PBL_SPI_MEM_OP_NO_DATA);
  int ret;

  ret = prv_cmd(nor, PBL_FLASH_NOR_CMD_WREN);
  if (ret != 0) {
    return ret;
  }

  return prv_exec_wait(nor, &op);
}

const struct pbl_flash_ops pbl_flash_spi_nor_ops = {
  .init = prv_init,
  .read = prv_read,
  .write = prv_write,
  .erase = prv_erase,
  .erase_status = prv_erase_status,
  .erase_suspend = prv_erase_suspend,
  .erase_resume = prv_erase_resume,
  .power_down = prv_power_down,
  .stop = prv_stop,
  .otp_read = prv_otp_read,
  .otp_write = prv_otp_write,
  .otp_is_locked = prv_otp_is_locked,
  .otp_lock = prv_otp_lock,
  .otp_erase = prv_otp_erase,
};
