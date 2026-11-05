/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/drivers/spi_mem/nrf5_qspi.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/logging/logging.h>
#include <pbl/soc/nrf/sleep.h>
#include <pbl/util/bits.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <hardware/nrf52_qspi.h>

PBL_LOG_MODULE_DECLARE(driver_spi_mem, CONFIG_DRIVER_SPI_MEM_LOG_LEVEL);

// Anomaly 244 (data corruption when the HF clock source switches) has not been seen at or below
// this frequency, so it is not worked around.
#define ANOMALY_244_MAX_HZ 8000000U

#define QSPI_BASE_HZ   32000000U
#define QSPI_MIN_HZ    (QSPI_BASE_HZ / 16U)
#define QSPI_SCK_DELAY 5U

#define CINSTR_MAX_DATA 8U
#define WORD_SIZE       sizeof(uint32_t)

// Interrupt-driven transfers only pay off above this size
#define MIN_IRQ_XFER_SIZE 256U

// EasyDMA only reaches data RAM
#define IS_IN_RAM(p) (((uintptr_t)(p) & 0xe0000000U) == 0x20000000U)

#define ADDR_3BYTE 3U
#define ADDR_4BYTE 4U

// Opcodes the READ, WRITE and ERASE tasks issue in each mode
#define OP_FASTREAD  0x0BU
#define OP_READ2O    0x3BU
#define OP_READ2IO   0xBBU
#define OP_READ4O    0x6BU
#define OP_READ4IO   0xEBU
#define OP_PP        0x02U
#define OP_PP2O      0xA2U
#define OP_PP4O      0x32U
#define OP_PP4IO     0x38U
#define OP_ERASE_4K  0x20U
#define OP_ERASE_64K 0xD8U

static const struct {
  uint8_t opcode;
  uint8_t readoc;
} s_reads[] = {
  {OP_FASTREAD, QSPI_IFCONFIG0_READOC_FAST},   {OP_READ2O, QSPI_IFCONFIG0_READOC_READ2O},
  {OP_READ2IO, QSPI_IFCONFIG0_READOC_READ2IO}, {OP_READ4O, QSPI_IFCONFIG0_READOC_READ4O},
  {OP_READ4IO, QSPI_IFCONFIG0_READOC_READ4IO},
};

static const struct {
  uint8_t opcode;
  uint8_t writeoc;
} s_writes[] = {
  {OP_PP, QSPI_IFCONFIG0_WRITEOC_PP},
  {OP_PP2O, QSPI_IFCONFIG0_WRITEOC_PP2O},
  {OP_PP4O, QSPI_IFCONFIG0_WRITEOC_PP4O},
  {OP_PP4IO, QSPI_IFCONFIG0_WRITEOC_PP4IO},
};

static const struct {
  uint8_t opcode;
  uint32_t len;
} s_erases[] = {
  {OP_ERASE_4K, QSPI_ERASE_LEN_4KB},
  {OP_ERASE_64K, QSPI_ERASE_LEN_64KB},
};

static uint8_t PBL_ALIGNED(4) s_bounce_buf[32];

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(NRF52_QSPI_BASE + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, NRF52_QSPI_BASE + offset);
}

static PBL_ALWAYS_INLINE void prv_event_clear(void) {
  prv_putreg(NRF52_QSPI_EVENTS_READY_OFFSET, 0U);
  (void)prv_getreg(NRF52_QSPI_EVENTS_READY_OFFSET);
}

static PBL_ALWAYS_INLINE void prv_task(uint32_t offset) {
  prv_event_clear();
  prv_putreg(offset, 1U);
}

static void prv_psel_set(const uint32_t psel[6]) {
  prv_putreg(NRF52_QSPI_PSEL_SCK_OFFSET, psel[0]);
  prv_putreg(NRF52_QSPI_PSEL_CSN_OFFSET, psel[1]);
  prv_putreg(NRF52_QSPI_PSEL_IO0_OFFSET, psel[2]);
  prv_putreg(NRF52_QSPI_PSEL_IO1_OFFSET, psel[3]);
  prv_putreg(NRF52_QSPI_PSEL_IO2_OFFSET, psel[4]);
  prv_putreg(NRF52_QSPI_PSEL_IO3_OFFSET, psel[5]);
}

static PBL_ALWAYS_INLINE const struct pbl_spi_mem_nrf5_qspi *prv_qspi(
    const struct pbl_spi_mem *mem) {
  return container_of(mem, const struct pbl_spi_mem_nrf5_qspi, mem);
}

static int prv_read_idx(uint8_t opcode) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_reads); i++) {
    if (s_reads[i].opcode == opcode) {
      return (int)i;
    }
  }

  return -1;
}

static int prv_write_idx(uint8_t opcode) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_writes); i++) {
    if (s_writes[i].opcode == opcode) {
      return (int)i;
    }
  }

  return -1;
}

static int prv_erase_idx(uint8_t opcode) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_erases); i++) {
    if (s_erases[i].opcode == opcode) {
      return (int)i;
    }
  }

  return -1;
}

void pbl_spi_mem_nrf5_qspi_irq_handler(const struct pbl_spi_mem_nrf5_qspi *qspi) {
  prv_event_clear();
  pbl_sem_give(&qspi->data->done);
}

static PBL_ALWAYS_INLINE void prv_wait_ready(void) {
  while (prv_getreg(NRF52_QSPI_EVENTS_READY_OFFSET) == 0U) {
  }
  prv_event_clear();
}

static void prv_activate(void) {
  prv_task(NRF52_QSPI_TASKS_ACTIVATE_OFFSET);
  prv_wait_ready();
}

// nRF52 anomaly 215: after XIP, accessing a QSPI register above offset 0x600 halts the CPU unless
// the peripheral is activated again first. Pins are disconnected meanwhile so that nothing waits
// on the memory.
static void prv_anomaly_215_workaround(const struct pbl_spi_mem_nrf5_qspi *qspi) {
  static const uint32_t disconnected[6] = {
    QSPI_PSEL_DISCONNECTED, QSPI_PSEL_DISCONNECTED, QSPI_PSEL_DISCONNECTED,
    QSPI_PSEL_DISCONNECTED, QSPI_PSEL_DISCONNECTED, QSPI_PSEL_DISCONNECTED,
  };

  prv_psel_set(disconnected);
  prv_activate();
  prv_psel_set(qspi->data->psel);
}

static bool prv_addr_ok(const struct pbl_spi_mem_op *op) {
  return op->addr.nbytes == ADDR_3BYTE || op->addr.nbytes == ADDR_4BYTE;
}

// A custom instruction carries its address, dummy and data bytes in its data registers
static bool prv_is_cinstr_shape(const struct pbl_spi_mem_op *op) {
  return op->cmd.buswidth == 1U && op->addr.buswidth <= 1U && op->dummy.buswidth <= 1U &&
         op->data.buswidth <= 1U && (op->addr.nbytes + op->dummy.nbytes) < CINSTR_MAX_DATA;
}

static bool prv_is_cinstr(const struct pbl_spi_mem_op *op) {
  return prv_is_cinstr_shape(op) &&
         (op->addr.nbytes + op->dummy.nbytes + op->data.nbytes) <= CINSTR_MAX_DATA;
}

static bool prv_is_read(const struct pbl_spi_mem_op *op) {
  return op->cmd.buswidth == 1U && op->data.dir == PBL_SPI_MEM_DATA_IN && prv_addr_ok(op) &&
         prv_read_idx(op->cmd.opcode) >= 0;
}

static bool prv_is_program(const struct pbl_spi_mem_op *op) {
  return op->cmd.buswidth == 1U && op->data.dir == PBL_SPI_MEM_DATA_OUT && op->dummy.nbytes == 0U &&
         prv_addr_ok(op) && prv_write_idx(op->cmd.opcode) >= 0;
}

static bool prv_is_erase(const struct pbl_spi_mem_op *op) {
  return op->cmd.buswidth == 1U && op->data.dir == PBL_SPI_MEM_NO_DATA && op->dummy.nbytes == 0U &&
         prv_addr_ok(op) && prv_erase_idx(op->cmd.opcode) >= 0;
}

static bool prv_supports_op(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  return prv_is_read(op) || prv_is_program(op) || prv_is_erase(op) || prv_is_cinstr(op);
}

static PBL_ALWAYS_INLINE void prv_set_ifconfig0(const struct pbl_spi_mem_nrf5_qspi *qspi,
                                                uint8_t readoc, uint8_t writeoc,
                                                uint8_t addr_nbytes) {
  struct pbl_spi_mem_nrf5_qspi_data *data = qspi->data;
  bool addr32 = addr_nbytes == ADDR_4BYTE;

  if (readoc == data->readoc && writeoc == data->writeoc && addr32 == data->addr32) {
    return;
  }

  prv_putreg(NRF52_QSPI_IFCONFIG0_OFFSET, PBL_FIELD_PREP(QSPI_IFCONFIG0_READOC_MASK, readoc) |
                                              PBL_FIELD_PREP(QSPI_IFCONFIG0_WRITEOC_MASK, writeoc) |
                                              (addr32 ? QSPI_IFCONFIG0_ADDRMODE_32BIT : 0U));

  data->readoc = readoc;
  data->writeoc = writeoc;
  data->addr32 = addr32;
}

// The interrupt is only enabled during interrupt driven transfers: writing INTENCLR costs about
// as much as a short polled read
static PBL_ALWAYS_INLINE void prv_xfer(const struct pbl_spi_mem *mem, uint32_t task, size_t len) {
  bool polling = pbl_spi_mem_is_polling(mem);
  bool irq = len > MIN_IRQ_XFER_SIZE && !polling;

  if (irq) {
    prv_putreg(NRF52_QSPI_INTENSET_OFFSET, QSPI_INT_READY);
    soc_nrf_sleep_full_block();
  } else if (polling) {
    prv_putreg(NRF52_QSPI_INTENCLR_OFFSET, QSPI_INT_READY);
  }

  prv_task(task);

  if (irq) {
    pbl_sem_take(&prv_qspi(mem)->data->done, PBL_FOREVER);
    prv_putreg(NRF52_QSPI_INTENCLR_OFFSET, QSPI_INT_READY);
    soc_nrf_sleep_full_release();
  } else {
    prv_wait_ready();
  }
}

// The data registers hold the bytes after the opcode, the first one in the low byte of CINSTRDAT0
static void prv_exec_cinstr(const struct pbl_spi_mem_nrf5_qspi *qspi,
                            const struct pbl_spi_mem_op *op) {
  uint8_t buf[CINSTR_MAX_DATA] = {};
  uint32_t dat[CINSTR_MAX_DATA / WORD_SIZE];
  size_t len = 0U;
  size_t data_off;

  for (size_t i = op->addr.nbytes; i > 0U; i--) {
    buf[len++] = (uint8_t)(op->addr.val >> (8U * (i - 1U)));
  }
  len += op->dummy.nbytes;
  data_off = len;
  if (op->data.dir == PBL_SPI_MEM_DATA_OUT) {
    memcpy(&buf[len], op->data.buf.out, op->data.nbytes);
  }
  len += op->data.nbytes;

  prv_putreg(NRF52_QSPI_INTENCLR_OFFSET, QSPI_INT_READY);
  prv_anomaly_215_workaround(qspi);

  memcpy(dat, buf, sizeof(dat));
  prv_putreg(NRF52_QSPI_CINSTRDAT0_OFFSET, pbl_le32_to_cpu(dat[0]));
  prv_putreg(NRF52_QSPI_CINSTRDAT1_OFFSET, pbl_le32_to_cpu(dat[1]));

  prv_event_clear();
  prv_putreg(NRF52_QSPI_CINSTRCONF_OFFSET,
             PBL_FIELD_PREP(QSPI_CINSTRCONF_OPCODE_MASK, op->cmd.opcode) |
                 PBL_FIELD_PREP(QSPI_CINSTRCONF_LENGTH_MASK, len + 1U) | QSPI_CINSTRCONF_LIO2 |
                 QSPI_CINSTRCONF_LIO3);
  prv_wait_ready();

  if (op->data.dir == PBL_SPI_MEM_DATA_IN) {
    dat[0] = pbl_cpu_to_le32(prv_getreg(NRF52_QSPI_CINSTRDAT0_OFFSET));
    dat[1] = pbl_cpu_to_le32(prv_getreg(NRF52_QSPI_CINSTRDAT1_OFFSET));
    memcpy(buf, dat, sizeof(dat));
    memcpy(op->data.buf.in, &buf[data_off], op->data.nbytes);
  }
}

static PBL_ALWAYS_INLINE void prv_read_setup(uint32_t addr, void *dst, size_t len) {
  prv_putreg(NRF52_QSPI_READ_SRC_OFFSET, addr);
  prv_putreg(NRF52_QSPI_READ_DST_OFFSET, (uintptr_t)dst);
  prv_putreg(NRF52_QSPI_READ_CNT_OFFSET, len);
}

static PBL_ALWAYS_INLINE bool prv_is_direct(const void *buf, size_t len) {
  return IS_IN_RAM(buf) && ((uintptr_t)buf % WORD_SIZE) == 0U && (len % WORD_SIZE) == 0U;
}

// One run is one READ task: straight into a word-aligned buffer, or through a bounce word
static void prv_exec_read(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  const struct pbl_spi_mem_nrf5_qspi *qspi = prv_qspi(mem);
  uint8_t PBL_ALIGNED(4) word[WORD_SIZE];

  prv_set_ifconfig0(qspi, s_reads[prv_read_idx(op->cmd.opcode)].readoc, qspi->data->writeoc,
                    op->addr.nbytes);

  if (prv_is_direct(op->data.buf.in, op->data.nbytes)) {
    prv_read_setup(op->addr.val, op->data.buf.in, op->data.nbytes);
    prv_xfer(mem, NRF52_QSPI_TASKS_READSTART_OFFSET, op->data.nbytes);
    return;
  }

  prv_read_setup(op->addr.val, word, sizeof(word));
  prv_xfer(mem, NRF52_QSPI_TASKS_READSTART_OFFSET, sizeof(word));
  memcpy(op->data.buf.in, word, op->data.nbytes);
}

// One run is one WRITE task: straight from a word-aligned RAM buffer, or through the bounce
// buffer padded with erased bytes, which program nothing
static void prv_exec_program(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  const struct pbl_spi_mem_nrf5_qspi *qspi = prv_qspi(mem);
  const void *src = op->data.buf.out;
  size_t len = op->data.nbytes;

  prv_set_ifconfig0(qspi, qspi->data->readoc, s_writes[prv_write_idx(op->cmd.opcode)].writeoc,
                    op->addr.nbytes);

  if (!prv_is_direct(src, len)) {
    memset(s_bounce_buf, 0xFF, sizeof(s_bounce_buf));
    memcpy(s_bounce_buf, src, len);
    src = s_bounce_buf;
    len = ((len + WORD_SIZE - 1U) / WORD_SIZE) * WORD_SIZE;
  }

  prv_putreg(NRF52_QSPI_WRITE_DST_OFFSET, op->addr.val);
  prv_putreg(NRF52_QSPI_WRITE_SRC_OFFSET, (uintptr_t)src);
  prv_putreg(NRF52_QSPI_WRITE_CNT_OFFSET, len);
  prv_xfer(mem, NRF52_QSPI_TASKS_WRITESTART_OFFSET, len);
}

static void prv_exec_erase(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  const struct pbl_spi_mem_nrf5_qspi *qspi = prv_qspi(mem);

  prv_set_ifconfig0(qspi, qspi->data->readoc, qspi->data->writeoc, op->addr.nbytes);

  prv_putreg(NRF52_QSPI_INTENCLR_OFFSET, QSPI_INT_READY);
  prv_putreg(NRF52_QSPI_ERASE_PTR_OFFSET, op->addr.val);
  prv_putreg(NRF52_QSPI_ERASE_LEN_OFFSET, s_erases[prv_erase_idx(op->cmd.opcode)].len);
  prv_task(NRF52_QSPI_TASKS_ERASESTART_OFFSET);
  prv_wait_ready();
}

static int prv_exec_op(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  if (prv_is_read(op)) {
    prv_exec_read(mem, op);
  } else if (prv_is_program(op)) {
    prv_exec_program(mem, op);
  } else if (prv_is_erase(op)) {
    prv_exec_erase(mem, op);
  } else {
    prv_exec_cinstr(prv_qspi(mem), op);
  }

  return 0;
}

static PBL_ALWAYS_INLINE size_t prv_adjust_len(const void *buf, size_t len, size_t bounce_size) {
  size_t misalign = (uintptr_t)buf % WORD_SIZE;

  if (IS_IN_RAM(buf) && misalign == 0U && len >= WORD_SIZE) {
    return len - (len % WORD_SIZE);
  }

  // Up to the next word boundary of the buffer, or what the bounce buffer holds
  if (IS_IN_RAM(buf) && misalign != 0U) {
    return MIN(len, WORD_SIZE - misalign);
  }

  return MIN(len, bounce_size);
}

static int prv_adjust_op_size(const struct pbl_spi_mem *mem, struct pbl_spi_mem_op *op) {
  if (prv_is_read(op)) {
    op->data.nbytes = prv_adjust_len(op->data.buf.in, op->data.nbytes, WORD_SIZE);
  } else if (prv_is_program(op)) {
    op->data.nbytes = prv_adjust_len(op->data.buf.out, op->data.nbytes, sizeof(s_bounce_buf));
  } else if (prv_is_cinstr_shape(op)) {
    op->data.nbytes = MIN(op->data.nbytes, CINSTR_MAX_DATA - op->addr.nbytes - op->dummy.nbytes);
  }

  return 0;
}

static int prv_dirmap_create(const struct pbl_spi_mem *mem, struct pbl_spi_mem_dirmap *desc) {
  const struct pbl_spi_mem_nrf5_qspi *qspi = prv_qspi(mem);

  if (!prv_is_read(&desc->op)) {
    return -ENOTSUP;
  }

  qspi->data->dirmap_readoc = s_reads[prv_read_idx(desc->op.cmd.opcode)].readoc;
  prv_set_ifconfig0(qspi, qspi->data->dirmap_readoc, qspi->data->writeoc, desc->op.addr.nbytes);

  return 0;
}

// The window is the READ task, which issues the configured read opcode itself
static int prv_dirmap_read(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_dirmap *desc,
                           uint32_t offs, void *buf, size_t len) {
  const struct pbl_spi_mem_nrf5_qspi *qspi = prv_qspi(mem);
  struct pbl_spi_mem_nrf5_qspi_data *data = qspi->data;
  uint8_t *dst = buf;

  prv_set_ifconfig0(qspi, data->dirmap_readoc, data->writeoc, desc->op.addr.nbytes);

  while (len > 0U) {
    size_t n = prv_adjust_len(dst, len, WORD_SIZE);

    if (prv_is_direct(dst, n)) {
      prv_read_setup(offs, dst, n);
      prv_xfer(mem, NRF52_QSPI_TASKS_READSTART_OFFSET, n);
    } else {
      uint8_t PBL_ALIGNED(4) word[WORD_SIZE];

      prv_read_setup(offs, word, sizeof(word));
      prv_xfer(mem, NRF52_QSPI_TASKS_READSTART_OFFSET, sizeof(word));
      memcpy(dst, word, n);
    }

    offs += n;
    dst += n;
    len -= n;
  }

  return 0;
}

static void prv_power_down(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_pm *pm,
                           pbl_spi_mem_pm_fn_t fn, void *arg) {
  const struct pbl_spi_mem_nrf5_qspi *qspi = prv_qspi(mem);

  if (pm != nullptr) {
    prv_exec_cinstr(qspi, &pm->enter);
    pbl_busy_wait_us(pm->enter_us);
  }

  prv_putreg(NRF52_QSPI_INTENCLR_OFFSET, QSPI_INT_READY);
  prv_putreg(NRF52_QSPI_TASKS_DEACTIVATE_OFFSET, 1U);
  prv_event_clear();
  prv_putreg(NRF52_QSPI_ENABLE_OFFSET, QSPI_ENABLE_DISABLED);

  fn(arg);

  prv_putreg(NRF52_QSPI_ENABLE_OFFSET, QSPI_ENABLE_ENABLED);
  prv_activate();

  if (pm != nullptr) {
    prv_exec_cinstr(qspi, &pm->exit);
    pbl_busy_wait_us(pm->exit_us);
  }
}

static int prv_init(const struct pbl_spi_mem *mem) {
  const struct pbl_spi_mem_nrf5_qspi *qspi = prv_qspi(mem);
  struct pbl_spi_mem_nrf5_qspi_data *data = qspi->data;

  if (qspi->freq_hz > QSPI_BASE_HZ || qspi->freq_hz < QSPI_MIN_HZ) {
    return -EINVAL;
  }

  if (qspi->freq_hz > ANOMALY_244_MAX_HZ) {
    PBL_LOG_WRN("QSPI at %" PRIu32 " Hz is exposed to anomaly 244", qspi->freq_hz);
  }

  data->psel[0] = pbl_gpio_nrf5_pin(&qspi->sck);
  data->psel[1] = pbl_gpio_nrf5_pin(&qspi->csn);
  for (size_t i = 0U; i < ARRAY_LENGTH(qspi->io); i++) {
    data->psel[2U + i] = pbl_gpio_nrf5_pin(&qspi->io[i]);
  }
  prv_psel_set(data->psel);

  data->readoc = QSPI_IFCONFIG0_READOC_FAST;
  data->writeoc = QSPI_IFCONFIG0_WRITEOC_PP;
  data->addr32 = false;
  prv_putreg(NRF52_QSPI_IFCONFIG0_OFFSET,
             PBL_FIELD_PREP(QSPI_IFCONFIG0_READOC_MASK, data->readoc) |
                 PBL_FIELD_PREP(QSPI_IFCONFIG0_WRITEOC_MASK, data->writeoc));

  // SPI mode 0, no deep power-down by the peripheral itself
  prv_putreg(NRF52_QSPI_IFCONFIG1_OFFSET,
             (prv_getreg(NRF52_QSPI_IFCONFIG1_OFFSET) & QSPI_IFCONFIG1_RESERVED_MASK) |
                 PBL_FIELD_PREP(QSPI_IFCONFIG1_SCKDELAY_MASK, QSPI_SCK_DELAY) |
                 PBL_FIELD_PREP(QSPI_IFCONFIG1_SCKFREQ_MASK, (QSPI_BASE_HZ / qspi->freq_hz) - 1U));

  prv_putreg(NRF52_QSPI_ENABLE_OFFSET, QSPI_ENABLE_ENABLED);
  prv_activate();

  pbl_irq_enable(PBL_IRQN(QSPI));

  return 0;
}

const struct pbl_spi_mem_ops pbl_spi_mem_nrf5_qspi_ops = {
  .init = prv_init,
  .supports_op = prv_supports_op,
  .adjust_op_size = prv_adjust_op_size,
  .exec_op = prv_exec_op,
  .dirmap_create = prv_dirmap_create,
  .dirmap_read = prv_dirmap_read,
  .power_down = prv_power_down,
};
