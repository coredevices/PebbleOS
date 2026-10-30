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
#include <pbl/logging/logging.h>
#include <pbl/soc/nrf/sleep.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <hal/nrf_qspi.h>

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
  nrf_qspi_readoc_t readoc;
} s_reads[] = {
  {OP_FASTREAD, NRF_QSPI_READOC_FASTREAD}, {OP_READ2O, NRF_QSPI_READOC_READ2O},
  {OP_READ2IO, NRF_QSPI_READOC_READ2IO},   {OP_READ4O, NRF_QSPI_READOC_READ4O},
  {OP_READ4IO, NRF_QSPI_READOC_READ4IO},
};

static const struct {
  uint8_t opcode;
  nrf_qspi_writeoc_t writeoc;
} s_writes[] = {
  {OP_PP, NRF_QSPI_WRITEOC_PP},
  {OP_PP2O, NRF_QSPI_WRITEOC_PP2O},
  {OP_PP4O, NRF_QSPI_WRITEOC_PP4O},
  {OP_PP4IO, NRF_QSPI_WRITEOC_PP4IO},
};

static const struct {
  uint8_t opcode;
  nrf_qspi_erase_len_t len;
} s_erases[] = {
  {OP_ERASE_4K, NRF_QSPI_ERASE_LEN_4KB},
  {OP_ERASE_64K, NRF_QSPI_ERASE_LEN_64KB},
};

static uint8_t PBL_ALIGNED(4) s_bounce_buf[32];

static const struct pbl_spi_mem_nrf5_qspi *prv_qspi(const struct pbl_spi_mem *mem) {
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
  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
  pbl_sem_give(&qspi->data->done);
}

static void prv_wait_ready(void) {
  while (!nrf_qspi_event_check(NRF_QSPI, NRF_QSPI_EVENT_READY)) {
  }
  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
}

static void prv_activate(void) {
  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
  nrf_qspi_task_trigger(NRF_QSPI, NRF_QSPI_TASK_ACTIVATE);
  prv_wait_ready();
}

// nRF52 anomaly 215: after XIP, accessing a QSPI register above offset 0x600 halts the CPU unless
// the peripheral is activated again first. Pins are disconnected meanwhile so that nothing waits
// on the memory.
static void prv_anomaly_215_workaround(void) {
  nrf_qspi_pins_t pins;
  nrf_qspi_pins_t disconnected = {
    .sck_pin = NRF_QSPI_PIN_NOT_CONNECTED,
    .csn_pin = NRF_QSPI_PIN_NOT_CONNECTED,
    .io0_pin = NRF_QSPI_PIN_NOT_CONNECTED,
    .io1_pin = NRF_QSPI_PIN_NOT_CONNECTED,
    .io2_pin = NRF_QSPI_PIN_NOT_CONNECTED,
    .io3_pin = NRF_QSPI_PIN_NOT_CONNECTED,
  };

  nrf_qspi_pins_get(NRF_QSPI, &pins);
  nrf_qspi_pins_set(NRF_QSPI, &disconnected);
  prv_activate();
  nrf_qspi_pins_set(NRF_QSPI, &pins);
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

static void prv_set_ifconfig0(const struct pbl_spi_mem_nrf5_qspi *qspi, uint8_t readoc,
                              uint8_t writeoc, uint8_t addr_nbytes) {
  struct pbl_spi_mem_nrf5_qspi_data *data = qspi->data;
  uint8_t addrmode =
      (addr_nbytes == ADDR_4BYTE) ? NRF_QSPI_ADDRMODE_32BIT : NRF_QSPI_ADDRMODE_24BIT;
  nrf_qspi_prot_conf_t conf = {};

  if (readoc == data->readoc && writeoc == data->writeoc && addrmode == data->addrmode) {
    return;
  }

  conf.readoc = (nrf_qspi_readoc_t)readoc;
  conf.writeoc = (nrf_qspi_writeoc_t)writeoc;
  conf.addrmode = (nrf_qspi_addrmode_t)addrmode;
  nrf_qspi_ifconfig0_set(NRF_QSPI, &conf);

  data->readoc = readoc;
  data->writeoc = writeoc;
  data->addrmode = addrmode;
}

static void prv_xfer(const struct pbl_spi_mem *mem, nrf_qspi_task_t task, size_t len) {
  bool irq = len > MIN_IRQ_XFER_SIZE && !pbl_spi_mem_is_polling(mem);

  if (irq) {
    nrf_qspi_int_enable(NRF_QSPI, NRF_QSPI_INT_READY_MASK);
    soc_nrf_sleep_full_block();
  } else {
    nrf_qspi_int_disable(NRF_QSPI, NRF_QSPI_INT_READY_MASK);
  }

  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
  nrf_qspi_task_trigger(NRF_QSPI, task);

  if (irq) {
    pbl_sem_take(&prv_qspi(mem)->data->done, PBL_FOREVER);
    soc_nrf_sleep_full_release();
  } else {
    prv_wait_ready();
  }
}

static void prv_exec_cinstr(const struct pbl_spi_mem_op *op) {
  uint8_t buf[CINSTR_MAX_DATA] = {};
  size_t len = 0U;
  size_t data_off;
  nrf_qspi_cinstr_conf_t conf = {
    .opcode = op->cmd.opcode,
    .io2_level = true,
    .io3_level = true,
  };

  for (size_t i = op->addr.nbytes; i > 0U; i--) {
    buf[len++] = (uint8_t)(op->addr.val >> (8U * (i - 1U)));
  }
  len += op->dummy.nbytes;
  data_off = len;
  if (op->data.dir == PBL_SPI_MEM_DATA_OUT) {
    memcpy(&buf[len], op->data.buf.out, op->data.nbytes);
  }
  len += op->data.nbytes;
  conf.length = len + 1U;

  nrf_qspi_int_disable(NRF_QSPI, NRF_QSPI_INT_READY_MASK);
  prv_anomaly_215_workaround();

  if (len > 0U) {
    nrf_qspi_cinstrdata_set(NRF_QSPI, conf.length, buf);
  }

  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
  nrf_qspi_cinstr_transfer_start(NRF_QSPI, &conf);
  prv_wait_ready();

  if (op->data.dir == PBL_SPI_MEM_DATA_IN) {
    nrf_qspi_cinstrdata_get(NRF_QSPI, conf.length, buf);
    memcpy(op->data.buf.in, &buf[data_off], op->data.nbytes);
  }
}

static bool prv_is_direct(const void *buf, size_t len) {
  return IS_IN_RAM(buf) && ((uintptr_t)buf % WORD_SIZE) == 0U && (len % WORD_SIZE) == 0U;
}

// One run is one READ task: straight into a word-aligned buffer, or through a bounce word
static void prv_exec_read(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  const struct pbl_spi_mem_nrf5_qspi *qspi = prv_qspi(mem);
  uint8_t PBL_ALIGNED(4) word[WORD_SIZE];

  prv_set_ifconfig0(qspi, s_reads[prv_read_idx(op->cmd.opcode)].readoc, qspi->data->writeoc,
                    op->addr.nbytes);

  if (prv_is_direct(op->data.buf.in, op->data.nbytes)) {
    nrf_qspi_read_buffer_set(NRF_QSPI, op->data.buf.in, op->data.nbytes, op->addr.val);
    prv_xfer(mem, NRF_QSPI_TASK_READSTART, op->data.nbytes);
    return;
  }

  nrf_qspi_read_buffer_set(NRF_QSPI, word, sizeof(word), op->addr.val);
  prv_xfer(mem, NRF_QSPI_TASK_READSTART, sizeof(word));
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

  nrf_qspi_write_buffer_set(NRF_QSPI, src, len, op->addr.val);
  prv_xfer(mem, NRF_QSPI_TASK_WRITESTART, len);
}

static void prv_exec_erase(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  const struct pbl_spi_mem_nrf5_qspi *qspi = prv_qspi(mem);

  prv_set_ifconfig0(qspi, qspi->data->readoc, qspi->data->writeoc, op->addr.nbytes);

  nrf_qspi_int_disable(NRF_QSPI, NRF_QSPI_INT_READY_MASK);
  nrf_qspi_erase_ptr_set(NRF_QSPI, op->addr.val, s_erases[prv_erase_idx(op->cmd.opcode)].len);
  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
  nrf_qspi_task_trigger(NRF_QSPI, NRF_QSPI_TASK_ERASESTART);
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
    prv_exec_cinstr(op);
  }

  return 0;
}

static size_t prv_adjust_len(const void *buf, size_t len, size_t bounce_size) {
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

  prv_set_ifconfig0(qspi, s_reads[prv_read_idx(desc->op.cmd.opcode)].readoc, qspi->data->writeoc,
                    desc->op.addr.nbytes);

  return 0;
}

// The window is the READ task, which issues the configured read opcode itself
static int prv_dirmap_read(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_dirmap *desc,
                           uint32_t offs, void *buf, size_t len) {
  struct pbl_spi_mem_op op = desc->op;
  uint8_t *dst = buf;

  while (len > 0U) {
    op.addr.val = offs;
    op.data.buf.in = dst;
    op.data.nbytes = prv_adjust_len(dst, len, WORD_SIZE);
    prv_exec_read(mem, &op);

    offs += op.data.nbytes;
    dst += op.data.nbytes;
    len -= op.data.nbytes;
  }

  return 0;
}

static void prv_power_down(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_pm *pm,
                           pbl_spi_mem_pm_fn_t fn, void *arg) {
  if (pm != nullptr) {
    prv_exec_cinstr(&pm->enter);
    pbl_busy_wait_us(pm->enter_us);
  }

  nrf_qspi_int_disable(NRF_QSPI, NRF_QSPI_INT_READY_MASK);
  nrf_qspi_task_trigger(NRF_QSPI, NRF_QSPI_TASK_DEACTIVATE);
  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
  nrf_qspi_disable(NRF_QSPI);

  fn(arg);

  nrf_qspi_enable(NRF_QSPI);
  prv_activate();

  if (pm != nullptr) {
    prv_exec_cinstr(&pm->exit);
    pbl_busy_wait_us(pm->exit_us);
  }
}

static int prv_init(const struct pbl_spi_mem *mem) {
  const struct pbl_spi_mem_nrf5_qspi *qspi = prv_qspi(mem);
  nrf_qspi_pins_t pins;
  nrf_qspi_prot_conf_t prot = {
    .readoc = NRF_QSPI_READOC_FASTREAD,
    .writeoc = NRF_QSPI_WRITEOC_PP,
    .addrmode = NRF_QSPI_ADDRMODE_24BIT,
  };
  nrf_qspi_phy_conf_t phy = {
    .sck_delay = QSPI_SCK_DELAY,
    .spi_mode = NRF_QSPI_MODE_0,
  };

  if (qspi->freq_hz > QSPI_BASE_HZ || qspi->freq_hz < QSPI_MIN_HZ) {
    return -EINVAL;
  }

  if (qspi->freq_hz > ANOMALY_244_MAX_HZ) {
    PBL_LOG_WRN("QSPI at %" PRIu32 " Hz is exposed to anomaly 244", qspi->freq_hz);
  }

  pins.sck_pin = pbl_gpio_nrf5_pin(&qspi->sck);
  pins.csn_pin = pbl_gpio_nrf5_pin(&qspi->csn);
  pins.io0_pin = pbl_gpio_nrf5_pin(&qspi->io[0]);
  pins.io1_pin = pbl_gpio_nrf5_pin(&qspi->io[1]);
  pins.io2_pin = pbl_gpio_nrf5_pin(&qspi->io[2]);
  pins.io3_pin = pbl_gpio_nrf5_pin(&qspi->io[3]);
  nrf_qspi_pins_set(NRF_QSPI, &pins);

  nrf_qspi_ifconfig0_set(NRF_QSPI, &prot);
  qspi->data->readoc = prot.readoc;
  qspi->data->writeoc = prot.writeoc;
  qspi->data->addrmode = prot.addrmode;

  phy.sck_freq = (QSPI_BASE_HZ / qspi->freq_hz) - 1U;
  nrf_qspi_ifconfig1_set(NRF_QSPI, &phy);

  nrf_qspi_enable(NRF_QSPI);
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
