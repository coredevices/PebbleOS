/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/drivers/flash/spi_nor.h>
#include <pbl/drivers/otp.h>
#include <pbl/util/size.h>

#include <board/otp_layout.h>
#include <clar.h>
#include <fake_new_timer.h>

// Fake GigaDevice-like NOR chip behind a fake controller
///////////////////////////////////////////////////////////

#define CHIP_SIZE       (32U * 1024U * 1024U)
#define WINDOW_SIZE     (16U * 1024U * 1024U)
#define SFDP_BFPT_ADDR  0x30U
#define SFDP_4BAIT_ADDR 0x80U
#define MAX_RUN         64U
#define CINSTR_MAX      8U
#define NUM_SEC_REGS    4U
#define SEC_REG_MAX     2048U
#define OTP_PAGE        256U
#define BUSY_READS      2U

#define SR2_SUS2   PBL_BIT(2)
#define SR2_LB_ALL (PBL_BIT(3) | PBL_BIT(4) | PBL_BIT(5))

enum chip_model {
  // 01h takes 8 or 16 bits, 8 clearing status register 2; security registers 2 and 3
  CHIP_GD25LQ255E,
  // 01h and 31h each take exactly 8 bits; security registers 1 to 3
  CHIP_GD25Q256E,
};

static enum chip_model s_model;
static uint8_t *s_mem;
static uint8_t s_sfdp[0x100];
static bool s_has_sfdp;
static bool s_has_4bait;
static uint8_t s_sr1;
static uint8_t s_sr2;
static bool s_wel;
static bool s_4byte;
static uint32_t s_busy;
static int s_wrsr_count;
static uint8_t s_sec[NUM_SEC_REGS][SEC_REG_MAX];

// Controller capabilities
static bool s_quad_controller;
static bool s_4b_controller;
static bool s_cinstr_controller;
static bool s_dirmap_controller;
static bool s_poll_controller;

// Observations
static int s_resets;
static int s_rdsr_polls;
static int s_polls;
static int s_ex4b;
static size_t s_dirmap_bytes;
static size_t s_op_read_bytes;
static int s_invalidates;
static bool s_suspended;
static uint32_t s_suspended_busy;
static uint32_t s_erase_busy;
static uint32_t s_irq_every;
static int s_irq_windows;
static int s_susp_polls;
static uint32_t s_min_resume_to_suspend;

static uint32_t prv_chip_id(void) {
  return (s_model == CHIP_GD25LQ255E) ? 0x1960c8U : 0x1940c8U;
}

static uint32_t prv_sec_size(void) {
  return (s_model == CHIP_GD25LQ255E) ? 1024U : 2048U;
}

static bool prv_sec_exists(uint32_t idx) {
  return (s_model == CHIP_GD25LQ255E) ? (idx == 2U || idx == 3U) : (idx >= 1U && idx <= 3U);
}

static uint8_t prv_lb(uint32_t idx) {
  return (uint8_t)PBL_BIT(2U + idx);
}

static bool prv_is_4b_opcode(uint8_t opcode) {
  switch (opcode) {
    case 0x0CU:
    case 0x3CU:
    case 0xBCU:
    case 0x6CU:
    case 0xECU:
    case 0x12U:
    case 0x34U:
    case 0x21U:
    case 0xDCU:
      return true;
    default:
      return false;
  }
}

static bool prv_is_array_op(uint8_t opcode) {
  switch (opcode) {
    case PBL_FLASH_NOR_CMD_FAST_READ:
    case 0x3BU:
    case 0xBBU:
    case 0x6BU:
    case 0xEBU:
    case PBL_FLASH_NOR_CMD_PP:
    case PBL_FLASH_NOR_CMD_PP_1_1_4:
    case PBL_FLASH_NOR_CMD_SE:
    case PBL_FLASH_NOR_CMD_BE:
      return true;
    default:
      return prv_is_4b_opcode(opcode);
  }
}

// Status register writes keep the read-only and the already set lock bits
static void prv_write_sr2(uint8_t val) {
  const uint8_t ro = (s_model == CHIP_GD25LQ255E) ? (uint8_t)(PBL_BIT(7) | PBL_BIT(3) | SR2_SUS2)
                                                  : (uint8_t)(PBL_BIT(7) | PBL_BIT(0) | SR2_SUS2);

  s_sr2 = (uint8_t)((s_sr2 & ro) | (val & ~ro) | (s_sr2 & SR2_LB_ALL & ~ro));
}

static void prv_sec_access(uint32_t addr, uint32_t *idx, uint32_t *offs) {
  *idx = addr >> 12;
  *offs = addr & 0xFFFU;
  cl_assert(prv_sec_exists(*idx));
  cl_assert(*offs < prv_sec_size());
}

static bool prv_supports_op(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  if (op->data.buswidth == 4U || op->addr.buswidth == 4U) {
    if (!s_quad_controller) {
      return false;
    }
  } else if (op->data.buswidth > 1U || op->addr.buswidth > 1U) {
    return false;
  }

  if (prv_is_4b_opcode(op->cmd.opcode) && !s_4b_controller) {
    return false;
  }

  // Short custom instructions only, besides the opcodes of the array tasks
  if (s_cinstr_controller && !prv_is_array_op(op->cmd.opcode)) {
    return op->addr.nbytes + op->dummy.nbytes + op->data.nbytes <= CINSTR_MAX;
  }

  return true;
}

static int prv_adjust_op_size(const struct pbl_spi_mem *mem, struct pbl_spi_mem_op *op) {
  if (s_cinstr_controller && !prv_is_array_op(op->cmd.opcode)) {
    op->data.nbytes = MIN(op->data.nbytes, CINSTR_MAX - op->addr.nbytes - op->dummy.nbytes);
  } else {
    op->data.nbytes = MIN(op->data.nbytes, MAX_RUN);
  }

  return 0;
}

static void prv_check_addr(const struct pbl_spi_mem_op *op) {
  if (op->addr.nbytes == 0U) {
    return;
  }

  if (op->cmd.opcode == PBL_FLASH_NOR_CMD_RDSFDP) {
    cl_assert_equal_i(op->addr.nbytes, 3);
  } else if (prv_is_4b_opcode(op->cmd.opcode)) {
    cl_assert_equal_i(op->addr.nbytes, 4);
  } else {
    cl_assert_equal_i(op->addr.nbytes, s_4byte ? 4 : 3);
  }
}

static void prv_take_wel(void) {
  cl_assert(s_wel);
  s_wel = false;
  s_busy = BUSY_READS;
}

static void prv_take_wel_erase(void) {
  prv_take_wel();
  s_busy = s_erase_busy;
}

static int prv_exec_op(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  uint8_t *in = op->data.buf.in;
  const uint8_t *out = op->data.buf.out;
  uint32_t addr = op->addr.val;
  size_t n = op->data.nbytes;
  uint32_t idx;
  uint32_t offs;

  prv_check_addr(op);

  switch (op->cmd.opcode) {
    case PBL_FLASH_NOR_CMD_RSTEN:
      return 0;
    case PBL_FLASH_NOR_CMD_RST:
      s_resets++;
      s_4byte = false;
      return 0;
    case PBL_FLASH_NOR_CMD_SUSPEND:
      if (s_busy > 0U && !s_suspended) {
        s_suspended = true;
        s_suspended_busy = s_busy;
        s_busy = 0U;
        s_sr2 |= PBL_FLASH_NOR_SR2_SUS;
      }
      return 0;
    case PBL_FLASH_NOR_CMD_RESUME:
      if (s_suspended) {
        s_suspended = false;
        s_busy = s_suspended_busy;
        s_sr2 &= (uint8_t)~PBL_FLASH_NOR_SR2_SUS;
      }
      return 0;
    case PBL_FLASH_NOR_CMD_DPD:
    case PBL_FLASH_NOR_CMD_RDPD:
      return 0;
    case PBL_FLASH_NOR_CMD_WREN:
      s_wel = true;
      return 0;
    case PBL_FLASH_NOR_CMD_EN4B:
      s_4byte = true;
      return 0;
    case PBL_FLASH_NOR_CMD_EX4B:
      s_ex4b++;
      s_4byte = false;
      return 0;
    case PBL_FLASH_NOR_CMD_RDID:
      in[0] = (uint8_t)prv_chip_id();
      in[1] = (uint8_t)(prv_chip_id() >> 8);
      in[2] = (uint8_t)(prv_chip_id() >> 16);
      return 0;
    case PBL_FLASH_NOR_CMD_RDSFDP:
      if (s_has_sfdp) {
        memcpy(in, &s_sfdp[addr], n);
      } else {
        memset(in, 0xFF, n);
      }
      return 0;
    case PBL_FLASH_NOR_CMD_RDSR:
      in[0] = s_sr1 | ((s_busy > 0U) ? PBL_FLASH_NOR_SR_WIP : 0U);
      if (s_busy > 0U) {
        s_busy--;
        s_rdsr_polls++;
      }
      return 0;
    case PBL_FLASH_NOR_CMD_RDSR2:
      in[0] = s_sr2;
      return 0;
    case PBL_FLASH_NOR_CMD_WRSR:
      prv_take_wel();
      s_wrsr_count++;
      if (s_model == CHIP_GD25Q256E) {
        if (n == 1U) {
          s_sr1 = out[0];
        }
        return 0;
      }
      s_sr1 = out[0];
      prv_write_sr2((n > 1U) ? out[1] : 0U);
      return 0;
    case PBL_FLASH_NOR_CMD_WRSR2:
      cl_assert_equal_i(s_model, CHIP_GD25Q256E);
      cl_assert_equal_i(n, 1);
      prv_take_wel();
      s_wrsr_count++;
      prv_write_sr2(out[0]);
      return 0;
    case PBL_FLASH_NOR_CMD_FAST_READ:
    case 0x0CU:
    case 0xEBU:
    case 0xECU:
    case 0x6BU:
    case 0x6CU:
      cl_assert(addr + n <= CHIP_SIZE);
      memcpy(in, &s_mem[addr], n);
      s_op_read_bytes += n;
      return 0;
    case PBL_FLASH_NOR_CMD_PP:
    case PBL_FLASH_NOR_CMD_PP_1_1_4:
    case 0x12U:
    case 0x34U:
      prv_take_wel();
      cl_assert((addr & 0xFFU) + n <= 256U);
      for (size_t i = 0U; i < n; i++) {
        s_mem[addr + i] &= out[i];
      }
      return 0;
    case PBL_FLASH_NOR_CMD_SE:
    case 0x21U:
      prv_take_wel_erase();
      memset(&s_mem[addr & ~0xFFFU], 0xFF, 0x1000U);
      return 0;
    case PBL_FLASH_NOR_CMD_BE:
    case 0xDCU:
      prv_take_wel_erase();
      memset(&s_mem[addr & ~0xFFFFU], 0xFF, 0x10000U);
      return 0;
    case PBL_FLASH_NOR_CMD_RDSCUR:
      cl_assert_equal_i(op->dummy.nbytes, 1);
      prv_sec_access(addr, &idx, &offs);
      cl_assert(offs + n <= prv_sec_size());
      memcpy(in, &s_sec[idx][offs], n);
      return 0;
    case PBL_FLASH_NOR_CMD_PRSCUR:
      prv_take_wel();
      prv_sec_access(addr, &idx, &offs);
      cl_assert((offs % OTP_PAGE) + n <= OTP_PAGE);
      if ((s_sr2 & prv_lb(idx)) == 0U) {
        for (size_t i = 0U; i < n; i++) {
          s_sec[idx][offs + i] &= out[i];
        }
      }
      return 0;
    case PBL_FLASH_NOR_CMD_ERSCUR:
      prv_take_wel();
      prv_sec_access(addr, &idx, &offs);
      if ((s_sr2 & prv_lb(idx)) == 0U) {
        memset(s_sec[idx], 0xFF, prv_sec_size());
      }
      return 0;
    default:
      return -ENOTSUP;
  }
}

static int prv_poll_status(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op,
                           const struct pbl_spi_mem_op *status, uint8_t mask, uint8_t match,
                           const struct pbl_spi_mem_suspend *susp) {
  int ret;

  if (!s_poll_controller) {
    return -ENOTSUP;
  }

  cl_assert_equal_i(status->cmd.opcode, PBL_FLASH_NOR_CMD_RDSR);
  cl_assert_equal_i(mask, PBL_FLASH_NOR_SR_WIP);
  cl_assert_equal_i(match, 0);

  s_polls++;
  ret = prv_exec_op(mem, op);
  if (ret != 0 || susp == nullptr) {
    s_busy = 0U;
    return ret;
  }

  // Polls the way a controller the CPU runs from does, one microsecond per status read, with an
  // interrupt pending every s_irq_every microseconds
  s_susp_polls++;
  cl_assert_equal_i(susp->suspend.cmd.opcode, PBL_FLASH_NOR_CMD_SUSPEND);
  cl_assert_equal_i(susp->resume.cmd.opcode, PBL_FLASH_NOR_CMD_RESUME);

  uint8_t sr;
  uint32_t t = 0U;
  uint32_t resumed = 0U;
  uint32_t last_irq = 0U;
  const struct pbl_spi_mem_op rdsr = *status;

  while (true) {
    struct pbl_spi_mem_op st = rdsr;

    st.data.buf.in = &sr;
    cl_assert_equal_i(prv_exec_op(mem, &st), 0);
    t++;
    if ((sr & mask) == match) {
      break;
    }

    if (s_irq_every == 0U || (t - last_irq) < s_irq_every || (t - resumed) < susp->resume_us) {
      continue;
    }

    s_min_resume_to_suspend = MIN(s_min_resume_to_suspend, t - resumed);
    cl_assert_equal_i(prv_exec_op(mem, &susp->suspend), 0);
    t += susp->suspend_us;
    do {
      st.data.buf.in = &sr;
      cl_assert_equal_i(prv_exec_op(mem, &st), 0);
      t++;
    } while ((sr & mask) != match);

    // Interrupts run here, with the part readable
    cl_assert(s_suspended || s_busy == 0U);
    s_irq_windows++;
    last_irq = t;

    cl_assert_equal_i(prv_exec_op(mem, &susp->resume), 0);
    resumed = t;
  }

  return 0;
}

static int prv_dirmap_create(const struct pbl_spi_mem *mem, struct pbl_spi_mem_dirmap *desc) {
  if (!s_dirmap_controller) {
    return -ENOTSUP;
  }

  desc->length = MIN(desc->length, WINDOW_SIZE - desc->offset);

  return 0;
}

static int prv_dirmap_read(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_dirmap *desc,
                           uint32_t offs, void *buf, size_t len) {
  cl_assert(offs + len <= WINDOW_SIZE);
  memcpy(buf, &s_mem[offs], len);
  s_dirmap_bytes += len;

  return 0;
}

static void prv_dirmap_invalidate(const struct pbl_spi_mem *mem,
                                  const struct pbl_spi_mem_dirmap *desc, uint32_t offs,
                                  size_t len) {
  s_invalidates++;
}

static const struct pbl_spi_mem_ops s_bus_ops = {
  .supports_op = prv_supports_op,
  .adjust_op_size = prv_adjust_op_size,
  .exec_op = prv_exec_op,
  .poll_status = prv_poll_status,
  .dirmap_create = prv_dirmap_create,
  .dirmap_read = prv_dirmap_read,
  .dirmap_invalidate = prv_dirmap_invalidate,
};

PBL_SPI_MEM_STATE_DEFINE(s_bus);
static const struct pbl_spi_mem s_bus = PBL_SPI_MEM_INIT(s_bus, "bus", nullptr, &s_bus_ops, 0U);
PBL_DEVICE_REGISTER(s_bus, &s_bus.dev);

PBL_SPI_MEM_STATE_DEFINE(s_xip_bus);
static const struct pbl_spi_mem s_xip_bus =
    PBL_SPI_MEM_INIT(s_xip_bus, "xip_bus", nullptr, &s_bus_ops, PBL_SPI_MEM_XIP);
PBL_DEVICE_REGISTER(s_xip_bus, &s_xip_bus.dev);

static const struct pbl_flash_otp_cell s_otp_cells[] = {
  BOARD_OTP_CELLS, BOARD_OTP_CELLS_SOC_CONFIG
};
static const struct pbl_flash_otp_layout s_otp_layout = {s_otp_cells, ARRAY_LENGTH(s_otp_cells)};

PBL_FLASH_SPI_NOR_DEFINE(s_nor_lq, "nor_lq", &s_bus, &pbl_flash_nor_gd25lq255e, 0U, &s_otp_layout);
PBL_FLASH_SPI_NOR_DEFINE(s_nor_q, "nor_q", &s_bus, &pbl_flash_nor_gd25q256e, 0U, &s_otp_layout);
PBL_FLASH_SPI_NOR_DEFINE(s_nor_xip, "nor_xip", &s_xip_bus, &pbl_flash_nor_gd25q256e, 0x12000000U,
                         &s_otp_layout);

const struct pbl_flash *const FLASH = &s_nor_q.flash;

static void prv_put32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

static void prv_param_header(uint8_t *ph, uint16_t id, uint8_t ndwords, uint32_t ptp) {
  ph[0] = (uint8_t)id;
  ph[1] = 0U;
  ph[2] = 1U;
  ph[3] = ndwords;
  ph[4] = (uint8_t)ptp;
  ph[5] = (uint8_t)(ptp >> 8);
  ph[6] = (uint8_t)(ptp >> 16);
  ph[7] = (uint8_t)(id >> 8);
}

// 32 MiB, 4/32/64 KiB erases, 1-1-2, 1-2-2, 1-1-4 and 1-4-4 reads, 256 byte pages, and, if
// enabled, a 4-byte address instruction table
static void prv_build_sfdp(void) {
  uint32_t bfpt[16] = {};

  memset(s_sfdp, 0xFF, sizeof(s_sfdp));
  prv_put32(&s_sfdp[0], 0x50444653U);
  s_sfdp[4] = 6U;
  s_sfdp[5] = 1U;
  s_sfdp[6] = s_has_4bait ? 1U : 0U;
  s_sfdp[7] = 0xFFU;
  prv_param_header(&s_sfdp[8], 0xFF00U, ARRAY_LENGTH(bfpt), SFDP_BFPT_ADDR);
  if (s_has_4bait) {
    prv_param_header(&s_sfdp[16], 0xFF84U, 2U, SFDP_4BAIT_ADDR);
    prv_put32(&s_sfdp[SFDP_4BAIT_ADDR], PBL_BIT(1) | PBL_BIT(4) | PBL_BIT(5) | PBL_BIT(6) |
                                            PBL_BIT(7) | PBL_BIT(9) | PBL_BIT(11));
    prv_put32(&s_sfdp[SFDP_4BAIT_ADDR + 4U], 0x21U | (0x5CU << 8) | (0xDCU << 16));
  }

  bfpt[0] = 0x01U | (0x20U << 8) | (1U << 16) | (1U << 17) | (1U << 20) | (1U << 21) | (1U << 22);
  bfpt[1] = (CHIP_SIZE * 8U) - 1U;
  bfpt[2] = (0xEBU << 8) | (2U << 5) | 4U | ((0x6BU << 8) | 8U) << 16;
  bfpt[3] = (0x3BU << 8) | 8U | ((0xBBU << 8) | 4U) << 16;
  bfpt[7] = 12U | (0x20U << 8) | ((15U | (0x52U << 8)) << 16);
  bfpt[8] = 16U | (0xD8U << 8);
  // Typical erase times: 4 KiB 45 ms, 32 KiB 120 ms, 64 KiB 150 ms
  bfpt[9] = ((((1U << 5) | 2U) << 4) | (((1U << 5) | 7U) << 11) | (((1U << 5) | 9U) << 18));
  bfpt[10] = 8U << 4;
  bfpt[14] = 1U << 20;

  for (size_t i = 0U; i < ARRAY_LENGTH(bfpt); i++) {
    prv_put32(&s_sfdp[SFDP_BFPT_ADDR + (i * 4U)], bfpt[i]);
  }
}

static void prv_erase_cb(int status, void *arg) {
}

static void prv_reset_instance(struct pbl_device_state *dev, struct pbl_flash_state *st,
                               struct pbl_flash_spi_nor_data *data) {
  *dev = (struct pbl_device_state){};
  *st = (struct pbl_flash_state){};
  *data = (struct pbl_flash_spi_nor_data){};
}

static void prv_reset(enum chip_model model) {
  s_bus_device_state = (struct pbl_device_state){};
  s_bus_spi_mem_state = (struct pbl_spi_mem_state){};
  s_xip_bus_device_state = (struct pbl_device_state){};
  s_xip_bus_spi_mem_state = (struct pbl_spi_mem_state){};
  prv_reset_instance(&s_nor_lq_device_state, &s_nor_lq_flash_state, &s_nor_lq_data);
  prv_reset_instance(&s_nor_q_device_state, &s_nor_q_flash_state, &s_nor_q_data);
  prv_reset_instance(&s_nor_xip_device_state, &s_nor_xip_flash_state, &s_nor_xip_data);

  s_model = model;
  memset(s_mem, 0xFF, CHIP_SIZE);
  memset(s_sec, 0xFF, sizeof(s_sec));
  s_has_sfdp = true;
  s_has_4bait = false;
  s_sr1 = 0U;
  s_sr2 = 0U;
  s_wel = false;
  s_4byte = false;
  s_busy = 0U;
  s_wrsr_count = 0;
  s_quad_controller = true;
  s_4b_controller = true;
  s_cinstr_controller = false;
  s_dirmap_controller = false;
  s_poll_controller = false;
  s_resets = 0;
  s_rdsr_polls = 0;
  s_polls = 0;
  s_ex4b = 0;
  s_dirmap_bytes = 0U;
  s_op_read_bytes = 0U;
  s_invalidates = 0;
  s_suspended = false;
  s_suspended_busy = 0U;
  s_erase_busy = BUSY_READS;
  s_irq_every = 0U;
  s_irq_windows = 0;
  s_susp_polls = 0;
  s_min_resume_to_suspend = UINT32_MAX;
}

static const struct pbl_flash *prv_init(const struct pbl_flash_spi_nor *nor) {
  prv_build_sfdp();
  cl_assert_equal_i(pbl_device_init(&nor->flash.dev), 0);

  return &nor->flash;
}

void test_spi_nor__initialize(void) {
  s_mem = malloc(CHIP_SIZE);
  cl_assert(s_mem != nullptr);
  prv_reset(CHIP_GD25LQ255E);
}

void test_spi_nor__cleanup(void) {
  free(s_mem);
  stub_new_timer_cleanup();
}

// Geometry and modes
///////////////////////////////////////////////////////////

void test_spi_nor__sfdp_geometry(void) {
  prv_init(&s_nor_lq);

  cl_assert_equal_i(s_nor_lq_data.geometry.size, CHIP_SIZE);
  cl_assert_equal_i(s_nor_lq_data.geometry.page_size, 256);
  cl_assert_equal_i(s_nor_lq_data.geometry.subsector_size, 0x1000);
  cl_assert_equal_i(s_nor_lq_data.geometry.sector_size, 0x10000);
  cl_assert_equal_i(s_nor_lq_data.geometry.subsector_erase_ms, 3 * 16);
  cl_assert_equal_i(s_nor_lq_data.geometry.sector_erase_ms, 10 * 16);
  cl_assert_equal_i(s_nor_lq_data.addr_nbytes, 4);
  cl_assert_equal_i(s_resets, 1);
}

void test_spi_nor__4byte_opcodes_leave_3byte_mode(void) {
  prv_init(&s_nor_lq);

  cl_assert_equal_i(s_nor_lq_data.dirmap.op.cmd.opcode, 0xEC);
  cl_assert_equal_i(s_nor_lq_data.pp_opcode, 0x34);
  cl_assert_equal_i(s_nor_lq_data.subsector_opcode, 0x21);
  cl_assert_equal_i(s_nor_lq_data.sector_opcode, 0xDC);
  cl_assert(!s_nor_lq_data.en4b);
  cl_assert(!s_4byte);
  cl_assert_equal_i(s_ex4b, 1);
}

void test_spi_nor__4byte_opcodes_from_4bait(void) {
  s_has_4bait = true;
  prv_init(&s_nor_lq);

  cl_assert_equal_i(s_nor_lq_data.dirmap.op.cmd.opcode, 0xEC);
  cl_assert_equal_i(s_nor_lq_data.subsector_opcode, 0x21);
  cl_assert_equal_i(s_nor_lq_data.sector_opcode, 0xDC);
  cl_assert(!s_4byte);
}

void test_spi_nor__en4b_without_4byte_opcodes(void) {
  const struct pbl_flash *flash;
  uint8_t data[16] = {1, 2, 3};
  uint8_t back[sizeof(data)];

  s_4b_controller = false;
  flash = prv_init(&s_nor_lq);

  cl_assert_equal_i(s_nor_lq_data.dirmap.op.cmd.opcode, 0xEB);
  cl_assert_equal_i(s_nor_lq_data.subsector_opcode, 0x20);
  cl_assert(s_nor_lq_data.en4b);
  cl_assert(s_4byte);

  cl_assert_equal_i(pbl_flash_write(flash, 0x100U, data, sizeof(data)), 0);
  cl_assert_equal_i(pbl_flash_read(flash, 0x100U, back, sizeof(back)), 0);
  cl_assert_equal_m(back, data, sizeof(data));

  pbl_flash_stop(flash);
  cl_assert(!s_4byte);
  cl_assert_equal_i(pbl_flash_read(flash, 0x100U, back, sizeof(back)), -ENODEV);
}

void test_spi_nor__quad_enabled_once(void) {
  prv_init(&s_nor_lq);

  cl_assert_equal_i(s_nor_lq_data.dirmap.op.data.buswidth, 4);
  cl_assert(s_sr2 & PBL_FLASH_NOR_SR2_QE);
  cl_assert_equal_i(s_wrsr_count, 1);
}

void test_spi_nor__quad_enable_with_wrsr2(void) {
  prv_reset(CHIP_GD25Q256E);
  s_sr1 = 0x80U;
  prv_init(&s_nor_q);

  cl_assert(s_sr2 & PBL_FLASH_NOR_SR2_QE);
  cl_assert_equal_i(s_sr1, 0x80);
}

void test_spi_nor__block_protection_cleared(void) {
  s_sr1 = 0x80U | 0x1CU;
  s_sr2 = PBL_FLASH_NOR_SR2_QE;
  prv_init(&s_nor_lq);

  cl_assert_equal_i(s_sr1, 0x80);
  cl_assert_equal_i(s_sr2, PBL_FLASH_NOR_SR2_QE);
  cl_assert_equal_i(s_wrsr_count, 1);

  prv_reset(CHIP_GD25Q256E);
  s_sr1 = 0x80U | 0x1CU;
  s_sr2 = PBL_FLASH_NOR_SR2_QE;
  prv_init(&s_nor_q);

  cl_assert_equal_i(s_sr1, 0x80);
  cl_assert_equal_i(s_sr2, PBL_FLASH_NOR_SR2_QE);
}

void test_spi_nor__quad_already_enabled(void) {
  s_sr2 = PBL_FLASH_NOR_SR2_QE;
  prv_init(&s_nor_lq);
  cl_assert_equal_i(s_wrsr_count, 0);
}

void test_spi_nor__single_line_controller(void) {
  s_quad_controller = false;
  prv_init(&s_nor_lq);

  cl_assert_equal_i(s_nor_lq_data.dirmap.op.cmd.opcode, 0x0C);
  cl_assert_equal_i(s_nor_lq_data.pp_opcode, 0x12);
  cl_assert_equal_i(s_wrsr_count, 0);
}

void test_spi_nor__part_fallback_without_sfdp(void) {
  s_has_sfdp = false;
  s_quad_controller = false;
  prv_init(&s_nor_lq);

  cl_assert_equal_i(s_nor_lq_data.geometry.size, pbl_flash_nor_gd25lq255e.geometry.size);
  cl_assert_equal_i(s_nor_lq_data.geometry.sector_size, 0x10000);
  cl_assert_equal_i(s_nor_lq_data.subsector_opcode, PBL_FLASH_NOR_CMD_SE_4B);
}

// Array access
///////////////////////////////////////////////////////////

void test_spi_nor__read_write_erase(void) {
  const struct pbl_flash *flash = prv_init(&s_nor_lq);
  uint8_t data[300];
  uint8_t back[sizeof(data)];

  for (size_t i = 0U; i < sizeof(data); i++) {
    data[i] = (uint8_t)(i * 7U);
  }

  // Crosses a page boundary
  cl_assert_equal_i(pbl_flash_write(flash, 0x1F0U, data, sizeof(data)), 0);
  cl_assert_equal_i(pbl_flash_read(flash, 0x1F0U, back, sizeof(back)), 0);
  cl_assert_equal_m(back, data, sizeof(data));

  cl_assert_equal_i(pbl_flash_erase(flash, 0x0U, 0x1000U), 0);
  cl_assert(pbl_flash_is_erased(flash, 0x0U, 0x1000U));
}

void test_spi_nor__status_polled_without_controller_support(void) {
  const struct pbl_flash *flash = prv_init(&s_nor_lq);
  uint8_t data[4] = {};

  s_rdsr_polls = 0;
  cl_assert_equal_i(pbl_flash_write(flash, 0x0U, data, sizeof(data)), 0);
  cl_assert_equal_i(s_rdsr_polls, BUSY_READS);
  cl_assert_equal_i(s_polls, 0);
}

void test_spi_nor__status_polled_by_controller(void) {
  const struct pbl_flash *flash;
  uint8_t data[4] = {};

  s_poll_controller = true;
  flash = prv_init(&s_nor_lq);

  s_rdsr_polls = 0;
  s_polls = 0;
  cl_assert_equal_i(pbl_flash_write(flash, 0x0U, data, sizeof(data)), 0);
  cl_assert_equal_i(s_rdsr_polls, 0);
  cl_assert_equal_i(s_polls, 1);
}

void test_spi_nor__reads_through_window_then_operations(void) {
  const struct pbl_flash *flash;
  uint8_t data[64];
  uint8_t back[sizeof(data)];

  s_dirmap_controller = true;
  flash = prv_init(&s_nor_lq);
  cl_assert(s_nor_lq_data.dirmap.mapped);
  cl_assert_equal_i(s_nor_lq_data.dirmap.length, WINDOW_SIZE);

  for (size_t i = 0U; i < sizeof(data); i++) {
    data[i] = (uint8_t)(i + 1U);
  }

  cl_assert_equal_i(pbl_flash_write(flash, WINDOW_SIZE - 32U, data, sizeof(data)), 0);
  cl_assert(s_invalidates > 0);

  s_dirmap_bytes = 0U;
  s_op_read_bytes = 0U;
  cl_assert_equal_i(pbl_flash_read(flash, WINDOW_SIZE - 32U, back, sizeof(back)), 0);
  cl_assert_equal_m(back, data, sizeof(data));
  cl_assert_equal_i(s_dirmap_bytes, 32);
  cl_assert_equal_i(s_op_read_bytes, 32);
}

void test_spi_nor__reads_operations_without_window(void) {
  const struct pbl_flash *flash = prv_init(&s_nor_lq);
  uint8_t back[16];

  cl_assert(!s_nor_lq_data.dirmap.mapped);
  s_op_read_bytes = 0U;
  cl_assert_equal_i(pbl_flash_read(flash, 0x100U, back, sizeof(back)), 0);
  cl_assert_equal_i(s_dirmap_bytes, 0);
  cl_assert_equal_i(s_op_read_bytes, sizeof(back));
}

void test_spi_nor__xip_adopts_and_erases_synchronously(void) {
  const struct pbl_flash *flash;

  prv_reset(CHIP_GD25Q256E);
  s_sr2 = PBL_FLASH_NOR_SR2_QE;
  s_4byte = true;
  s_poll_controller = true;
  s_dirmap_controller = true;
  flash = prv_init(&s_nor_xip);

  cl_assert_equal_i(s_resets, 0);
  cl_assert_equal_i(s_wrsr_count, 0);
  cl_assert(!s_4byte);

  memset(s_mem, 0x00, 0x1000U);
  s_polls = 0;
  cl_assert_equal_i(pbl_flash_erase(flash, 0x12000000U, 0x1000U), 0);
  cl_assert_equal_i(s_polls, 1);
  cl_assert(pbl_flash_is_erased(flash, 0x12000000U, 0x1000U));
}

void test_spi_nor__xip_erase_suspended_for_interrupts(void) {
  const struct pbl_flash *flash;

  prv_reset(CHIP_GD25Q256E);
  s_sr2 = PBL_FLASH_NOR_SR2_QE;
  s_poll_controller = true;
  s_dirmap_controller = true;
  flash = prv_init(&s_nor_xip);

  memset(s_mem, 0x00, 0x10000U);
  s_erase_busy = 2000U;
  s_irq_every = 10U;
  cl_assert_equal_i(pbl_flash_erase(flash, 0x12000000U, 0x10000U), 0);

  cl_assert_equal_i(s_susp_polls, 1);
  cl_assert(s_irq_windows >= 10);
  cl_assert(s_min_resume_to_suspend >= pbl_flash_nor_gd25q256e.resume_us);
  cl_assert(!s_suspended);
  cl_assert_equal_i(s_sr2 & PBL_FLASH_NOR_SR2_SUS, 0);
  cl_assert(pbl_flash_is_erased(flash, 0x12000000U, 0x10000U));
}

void test_spi_nor__xip_program_not_suspended(void) {
  const struct pbl_flash *flash;
  uint8_t data[16] = {};

  prv_reset(CHIP_GD25Q256E);
  s_sr2 = PBL_FLASH_NOR_SR2_QE;
  s_poll_controller = true;
  s_dirmap_controller = true;
  flash = prv_init(&s_nor_xip);

  s_irq_every = 1U;
  cl_assert_equal_i(pbl_flash_write(flash, 0x12000000U, data, sizeof(data)), 0);
  cl_assert_equal_i(s_susp_polls, 0);
  cl_assert_equal_i(s_irq_windows, 0);
}

// The flash class suspends an erase in progress around reads on controllers the CPU does not run
// from, with the part's own suspend and resume
void test_spi_nor__erase_suspended_for_reads(void) {
  const struct pbl_flash *flash = prv_init(&s_nor_lq);
  uint8_t byte;

  s_erase_busy = UINT32_MAX;
  memset(s_mem, 0x00, 0x1000U);
  cl_assert_equal_i(pbl_flash_erase_async(flash, 0x0U, 0x1000U, prv_erase_cb, nullptr), 0);
  cl_assert(s_busy > 0U);

  cl_assert_equal_i(pbl_flash_read(flash, 0x2000U, &byte, 1U), 0);
  cl_assert(s_suspended);
  cl_assert(s_sr2 & PBL_FLASH_NOR_SR2_SUS);

  stub_new_timer_fire(stub_new_timer_get_next());
  cl_assert(!s_suspended);
  cl_assert(s_busy > 0U);
}

// OTP
///////////////////////////////////////////////////////////

void test_spi_nor__otp_info(void) {
  struct pbl_flash_otp_info info;

  cl_assert_equal_i(pbl_flash_otp_get_info(prv_init(&s_nor_lq), &info), 0);
  cl_assert_equal_i(info.region_len, 1024);
  cl_assert_equal_i(info.n_regions, 2);

  prv_reset(CHIP_GD25Q256E);
  cl_assert_equal_i(pbl_flash_otp_get_info(prv_init(&s_nor_q), &info), 0);
  cl_assert_equal_i(info.region_len, 2048);
  cl_assert_equal_i(info.n_regions, 3);
}

void test_spi_nor__otp_regions_map_to_registers(void) {
  const struct pbl_flash *flash = prv_init(&s_nor_lq);
  uint8_t data[2] = {0x5AU, 0xA5U};
  uint8_t back[2];

  cl_assert_equal_i(pbl_flash_otp_write(flash, 0x005U, data, 1U), 0);
  cl_assert_equal_i(pbl_flash_otp_write(flash, 0x407U, &data[1], 1U), 0);
  cl_assert_equal_i(s_sec[2][0x005], 0x5A);
  cl_assert_equal_i(s_sec[3][0x007], 0xA5);

  cl_assert_equal_i(pbl_flash_otp_read(flash, 0x005U, back, 1U), 0);
  cl_assert_equal_i(pbl_flash_otp_read(flash, 0x407U, &back[1], 1U), 0);
  cl_assert_equal_m(back, data, sizeof(data));

  cl_assert_equal_i(pbl_flash_otp_read(flash, 0x7FFU, back, 2U), -EINVAL);
}

void test_spi_nor__otp_spans_regions_and_pages(void) {
  const struct pbl_flash *flash;
  uint8_t data[600];
  uint8_t back[sizeof(data)];

  prv_reset(CHIP_GD25Q256E);
  flash = prv_init(&s_nor_q);

  for (size_t i = 0U; i < sizeof(data); i++) {
    data[i] = (uint8_t)(i ^ 0x3CU);
  }

  cl_assert_equal_i(pbl_flash_otp_write(flash, 2048U - 300U, data, sizeof(data)), 0);
  cl_assert_equal_i(pbl_flash_otp_read(flash, 2048U - 300U, back, sizeof(back)), 0);
  cl_assert_equal_m(back, data, sizeof(data));
  cl_assert_equal_i(s_sec[1][2047], data[299]);
  cl_assert_equal_i(s_sec[2][0], data[300]);
}

void test_spi_nor__otp_in_short_instructions(void) {
  const struct pbl_flash *flash;
  uint8_t data[40];
  uint8_t back[sizeof(data)];

  s_cinstr_controller = true;
  s_4b_controller = false;
  flash = prv_init(&s_nor_lq);
  cl_assert(s_4byte);

  for (size_t i = 0U; i < sizeof(data); i++) {
    data[i] = (uint8_t)(0x80U + i);
  }

  cl_assert_equal_i(pbl_flash_otp_write(flash, 0x20U, data, sizeof(data)), 0);
  cl_assert_equal_i(pbl_flash_otp_read(flash, 0x20U, back, sizeof(back)), 0);
  cl_assert_equal_m(back, data, sizeof(data));
  cl_assert_equal_m(&s_sec[2][0x20], data, sizeof(data));
}

void test_spi_nor__otp_locked_region_is_read_only(void) {
  const struct pbl_flash *flash = prv_init(&s_nor_lq);
  uint8_t byte = 0x00U;
  bool locked;

  s_sr2 |= PBL_BIT(5);
  cl_assert_equal_i(pbl_flash_otp_is_locked(flash, 0U, &locked), 0);
  cl_assert(!locked);
  cl_assert_equal_i(pbl_flash_otp_is_locked(flash, 1U, &locked), 0);
  cl_assert(locked);

  cl_assert_equal_i(pbl_flash_otp_write(flash, 0x400U, &byte, 1U), -EROFS);
  cl_assert_equal_i(s_sec[3][0], 0xFF);
  cl_assert_equal_i(pbl_flash_otp_is_locked(flash, 2U, &locked), -EINVAL);
}

void test_spi_nor__otp_erase(void) {
  const struct pbl_flash *flash = prv_init(&s_nor_lq);
  uint8_t byte = 0x00U;

  cl_assert_equal_i(pbl_flash_otp_write(flash, 0x10U, &byte, 1U), 0);
  cl_assert_equal_i(pbl_flash_otp_erase(flash, 0U), 0);
  cl_assert_equal_i(s_sec[2][0x10], 0xFF);

  s_sr2 |= PBL_BIT(4);
  cl_assert_equal_i(pbl_flash_otp_write(flash, 0x10U, &byte, 1U), -EROFS);
  cl_assert_equal_i(pbl_flash_otp_erase(flash, 0U), -EROFS);
}

void test_spi_nor__otp_lock_keeps_status(void) {
  const struct pbl_flash *flash;
  bool locked;

  s_sr1 = 0x80U;
  s_sr2 = PBL_FLASH_NOR_SR2_QE;
  flash = prv_init(&s_nor_lq);

  cl_assert_equal_i(pbl_flash_otp_lock(flash, 1U), 0);
  cl_assert_equal_i(s_sr2, PBL_FLASH_NOR_SR2_QE | PBL_BIT(5));
  cl_assert_equal_i(s_sr1, 0x80);
  cl_assert_equal_i(pbl_flash_otp_is_locked(flash, 1U, &locked), 0);
  cl_assert(locked);
  cl_assert_equal_i(pbl_flash_otp_is_locked(flash, 0U, &locked), 0);
  cl_assert(!locked);

  prv_reset(CHIP_GD25Q256E);
  s_sr1 = 0x80U;
  s_sr2 = PBL_FLASH_NOR_SR2_QE;
  flash = prv_init(&s_nor_q);
  cl_assert_equal_i(pbl_flash_otp_lock(flash, 0U), 0);
  cl_assert_equal_i(s_sr2, PBL_FLASH_NOR_SR2_QE | PBL_BIT(3));
  cl_assert_equal_i(s_sr1, 0x80);
}

// A status register write the part ignores is reported, not taken for a lock
void test_spi_nor__otp_lock_checked(void) {
  const struct pbl_flash *flash;

  prv_reset(CHIP_GD25Q256E);
  s_sr2 = PBL_FLASH_NOR_SR2_QE;
  flash = prv_init(&s_nor_lq);

  cl_assert_equal_i(pbl_flash_otp_lock(flash, 0U), -EIO);
}

void test_spi_nor__quad_enable_checked(void) {
  prv_reset(CHIP_GD25Q256E);
  prv_build_sfdp();
  cl_assert(pbl_device_init(&s_nor_lq.flash.dev) != 0);
}

void test_spi_nor__otp_cells(void) {
  const struct pbl_flash *flash = prv_init(&s_nor_lq);
  const struct pbl_flash_otp_cell *cell;

  cell = pbl_flash_otp_cell_get(flash, "serial-number");
  cl_assert(cell != nullptr);
  cl_assert_equal_i(cell->offset, 0x20);
  cl_assert_equal_i(cell->len, 32);
  cl_assert(pbl_flash_otp_cell_get(flash, "nope") == nullptr);
}

// The factory slots must keep landing on the bytes shipped devices hold them in: security
// register 2 on the GD25LQ255E (asterix), security register 1 on the GD25Q256E (obelix, getafix),
// 32 bytes per slot, locked by that register's lock bit
static void prv_check_slot_addresses(const struct pbl_flash *flash, uint32_t first_reg,
                                     uint8_t lb_bit) {
  static const char *const names[] = {"hw-version", "serial-number", "pcba-serial-number"};
  static const uint8_t slots[] = {OTP_HWVER, OTP_SERIAL, OTP_PCBA_SERIAL};
  struct pbl_flash_otp_info info;

  cl_assert_equal_i(pbl_flash_otp_get_info(flash, &info), 0);

  for (size_t i = 0U; i < ARRAY_LENGTH(names); i++) {
    const struct pbl_flash_otp_cell *cell = pbl_flash_otp_cell_get(flash, names[i]);
    uint8_t byte = (uint8_t)(0x10U + i);
    uint32_t phys = (first_reg << 12) + (slots[i] * 32U);
    bool locked;

    cl_assert(cell != nullptr);
    cl_assert_equal_i(cell->len, 32);
    cl_assert_equal_i(pbl_flash_otp_write(flash, cell->offset, &byte, 1U), 0);
    cl_assert_equal_i(s_sec[phys >> 12][phys & 0xFFFU], byte);

    s_sr2 = (uint8_t)PBL_BIT(lb_bit);
    cl_assert_equal_i(
        pbl_flash_otp_is_locked(flash, (uint8_t)(cell->offset / info.region_len), &locked), 0);
    cl_assert(locked);
    s_sr2 = 0U;
  }
}

void test_spi_nor__slot_addresses_gd25lq255e(void) {
  prv_check_slot_addresses(prv_init(&s_nor_lq), 2U, 4U);
}

void test_spi_nor__slot_addresses_gd25q256e(void) {
  prv_reset(CHIP_GD25Q256E);
  prv_check_slot_addresses(prv_init(&s_nor_q), 1U, 3U);
}

// The SoC factory configuration of SF32LB52 boards sits in security registers 2 and 3 of their
// GD25Q256E, apart from the factory slots in register 1; its system page, the first 32 bytes of
// register 1, is the hw-version cell
void test_spi_nor__soc_config_addresses_gd25q256e(void) {
  static const struct {
    const char *name;
    uint32_t phys;
    uint32_t len;
  } cells[] = {
    {"hw-version", 0x1000U, 32U},         {"serial-number", 0x1020U, 32U},
    {"pcba-serial-number", 0x1040U, 32U}, {"soc-config-user", 0x2000U, 256U},
    {"soc-config-cust", 0x3000U, 256U},
  };
  const struct pbl_flash *flash;

  prv_reset(CHIP_GD25Q256E);
  flash = prv_init(&s_nor_q);

  for (size_t i = 0U; i < ARRAY_LENGTH(cells); i++) {
    const struct pbl_flash_otp_cell *cell = pbl_flash_otp_cell_get(flash, cells[i].name);
    uint32_t last = cells[i].phys + cells[i].len - 1U;
    uint8_t byte = (uint8_t)(0x20U + i);

    cl_assert(cell != nullptr);
    cl_assert_equal_i(cell->len, cells[i].len);
    cl_assert_equal_i(pbl_flash_otp_write(flash, cell->offset, &byte, 1U), 0);
    cl_assert_equal_i(pbl_flash_otp_write(flash, cell->offset + cell->len - 1U, &byte, 1U), 0);
    cl_assert_equal_i(s_sec[cells[i].phys >> 12][cells[i].phys & 0xFFFU], byte);
    cl_assert_equal_i(s_sec[last >> 12][last & 0xFFFU], byte);

    for (size_t j = 0U; j < i; j++) {
      cl_assert(cells[j].phys + cells[j].len <= cells[i].phys ||
                cells[i].phys + cells[i].len <= cells[j].phys);
    }
  }
}

void test_spi_nor__otp_slots(void) {
  prv_reset(CHIP_GD25Q256E);
  prv_init(&s_nor_q);

  cl_assert_equal_i(otp_write_slot(OTP_SERIAL, "Q123456789AB"), OtpWriteSuccess);
  cl_assert_equal_s((const char *)&s_sec[1][0x20], "Q123456789AB");
  cl_assert_equal_s(otp_get_slot(OTP_SERIAL), "Q123456789AB");
  cl_assert_equal_i(otp_write_slot(OTP_SERIAL, "X"), OtpWriteFailAlreadyWritten);
  cl_assert_equal_i(otp_write_slot(OTP_HWVER, "0123456789012345678901234567890123"),
                    OtpWriteFailCorrupt);
  cl_assert(!otp_is_locked(OTP_SERIAL));

  s_sr2 |= PBL_BIT(3);
  cl_assert(otp_is_locked(OTP_SERIAL));
  cl_assert(otp_is_locked(OTP_PCBA_SERIAL));
}
