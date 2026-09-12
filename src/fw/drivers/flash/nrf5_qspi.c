/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/flash.h>
#include <pbl/drivers/flash/nor_part.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/logging/logging.h>
#include "kernel/util/delay.h"
#include "kernel/util/sleep.h"
#include "pbl/kernel/sem.h"
#include "pbl/soc/nrf/sleep.h"
#include "pbl/util/math.h"
#include "pbl/util/misc.h"
#include "system/passert.h"

#include <hal/nrf_qspi.h>
#include <nrfx.h>

PBL_LOG_MODULE_DECLARE(driver_flash, CONFIG_DRIVER_FLASH_LOG_LEVEL);

// NOTE: This driver does not cover anomaly 244, which may cause data corruption
// if HF clock source is switching between HFXO and HFINT (e.g. by BLE). This
// issue has not been observed at operating speeds <= 8MHz, therefore, no
// workaround is implemented here. A warning log will be emitted if driver is
// initialized at higher frequencies.

// Asynchronous read/write adds overhead for small sizes due to context
// switching and semaphore handling, so we define a minimum size for async ops.
#define MIN_RW_ASYNC_SIZE 256U

// Bits 15-12 of a security register address are its one-based index.
#define SEC_ADDR_TO_IDX(addr) (((addr) >> 12U) - 1U)

// Minimum size to enable 4-byte addressing
#define ADDR_4BYTE_THRESHOLD 0x1000000UL

enum read_mode {
  READ_FASTREAD,
  READ_READ2O,
  READ_READ2IO,
  READ_READ4O,
  READ_READ4IO,
};

enum write_mode {
  WRITE_PP,
  WRITE_PP2O,
  WRITE_PP4O,
  WRITE_PP4IO,
};

struct pbl_flash_nrf5_qspi_state {
  struct pbl_flash_device_state flash;
  struct pbl_sem sem;
  bool initialized;
};

struct pbl_flash_nrf5_qspi {
  struct pbl_flash_device dev;
  const struct pbl_flash_nor_part *part;
  uint32_t clk_freq_hz;
  uint32_t cs_gpio;
  uint32_t clk_gpio;
  uint32_t data_gpio[4];
  enum read_mode read_mode;
  enum write_mode write_mode;
};

static uint8_t __attribute__((aligned(4))) s_bounce_buf[32];
static struct pbl_flash_nrf5_qspi_state *s_state;

static inline const struct pbl_flash_nrf5_qspi *prv_cfg(const struct pbl_flash_device *dev) {
  return container_of(dev, const struct pbl_flash_nrf5_qspi, dev);
}

static inline struct pbl_flash_nrf5_qspi_state *prv_state(const struct pbl_flash_device *dev) {
  return container_of(dev->state, struct pbl_flash_nrf5_qspi_state, flash);
}

void QSPI_IRQHandler(void) {
  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
  pbl_sem_give(&s_state->sem);
}

// Workaround for nRF52 Anomaly 215: trigger TASKS_ACTIVATE and wait for
// EVENTS_READY before accessing any QSPI register with an offset above 0x600.
static void prv_workaround_215_apply(void) {
  nrf_qspi_pins_t pins;
  nrf_qspi_pins_t disconnected_pins = {
      .sck_pin = NRF_QSPI_PIN_NOT_CONNECTED,
      .csn_pin = NRF_QSPI_PIN_NOT_CONNECTED,
      .io0_pin = NRF_QSPI_PIN_NOT_CONNECTED,
      .io1_pin = NRF_QSPI_PIN_NOT_CONNECTED,
      .io2_pin = NRF_QSPI_PIN_NOT_CONNECTED,
      .io3_pin = NRF_QSPI_PIN_NOT_CONNECTED,
  };

  // Disconnect pins to not wait for response from external memory
  nrf_qspi_pins_get(NRF_QSPI, &pins);
  nrf_qspi_pins_set(NRF_QSPI, &disconnected_pins);

  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
  nrf_qspi_task_trigger(NRF_QSPI, NRF_QSPI_TASK_ACTIVATE);
  while (!nrf_qspi_event_check(NRF_QSPI, NRF_QSPI_EVENT_READY)) {
  }

  nrf_qspi_pins_set(NRF_QSPI, &pins);
}

static void prv_cinstr_write_read(uint8_t instr, const void *data, void *buf, size_t len) {
  nrf_qspi_cinstr_conf_t conf = {
      .opcode = instr,
      .length = len + 1U,
      .io2_level = true,
      .io3_level = true,
  };

  PBL_ASSERTN(len <= 8U);

  nrf_qspi_int_disable(NRF_QSPI, NRF_QSPI_INT_READY_MASK);
  prv_workaround_215_apply();
  if (data != NULL) {
    nrf_qspi_cinstrdata_set(NRF_QSPI, conf.length, data);
  }
  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
  nrf_qspi_cinstr_transfer_start(NRF_QSPI, &conf);
  while (!nrf_qspi_event_check(NRF_QSPI, NRF_QSPI_EVENT_READY)) {
  }
  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);

  if (buf != NULL) {
    nrf_qspi_cinstrdata_get(NRF_QSPI, conf.length, buf);
  }
}

static inline void prv_cinstr(uint8_t instr) {
  prv_cinstr_write_read(instr, NULL, NULL, 0U);
}

static inline void prv_cinstr_read(uint8_t instr, void *buf, size_t len) {
  prv_cinstr_write_read(instr, NULL, buf, len);
}

static inline void prv_cinstr_write(uint8_t instr, const void *data, size_t len) {
  prv_cinstr_write_read(instr, data, NULL, len);
}

static bool prv_use_irq(const struct pbl_flash_device *dev, size_t len) {
  return len > MIN_RW_ASYNC_SIZE && !dev->state->coredump;
}

static void prv_xfer_begin(const struct pbl_flash_device *dev, size_t len) {
  if (prv_use_irq(dev, len)) {
    nrf_qspi_int_enable(NRF_QSPI, NRF_QSPI_INT_READY_MASK);
    soc_nrf_sleep_full_block();
  } else {
    nrf_qspi_int_disable(NRF_QSPI, NRF_QSPI_INT_READY_MASK);
  }
  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
}

static void prv_xfer_wait(const struct pbl_flash_device *dev, size_t len) {
  if (prv_use_irq(dev, len)) {
    pbl_sem_take(&prv_state(dev)->sem, PBL_FOREVER);
    soc_nrf_sleep_full_release();
  } else {
    while (!nrf_qspi_event_check(NRF_QSPI, NRF_QSPI_EVENT_READY)) {
    }
    nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
  }
}

static void prv_read(const struct pbl_flash_device *dev, void *buf, size_t len, uint32_t addr) {
  prv_xfer_begin(dev, len);
  nrf_qspi_read_buffer_set(NRF_QSPI, buf, len, addr);
  nrf_qspi_task_trigger(NRF_QSPI, NRF_QSPI_TASK_READSTART);
  prv_xfer_wait(dev, len);
}

static void prv_write(const struct pbl_flash_device *dev, const void *buf, size_t len,
                      uint32_t addr) {
  prv_xfer_begin(dev, len);
  nrf_qspi_write_buffer_set(NRF_QSPI, buf, len, addr);
  nrf_qspi_task_trigger(NRF_QSPI, NRF_QSPI_TASK_WRITESTART);
  prv_xfer_wait(dev, len);
}

static inline void prv_read_sr1(const struct pbl_flash_nor_part *part, uint8_t *sr1) {
  prv_cinstr_read(part->cmd.rdsr1, sr1, 1U);
}

static inline void prv_read_sr2(const struct pbl_flash_nor_part *part, uint8_t *sr2) {
  prv_cinstr_read(part->cmd.rdsr2, sr2, 1U);
}

static inline bool prv_busy(const struct pbl_flash_nor_part *part) {
  uint8_t sr1;

  prv_read_sr1(part, &sr1);
  return (sr1 & part->mask.sr1_busy) != 0U;
}

static inline void prv_wait_idle(const struct pbl_flash_nor_part *part) {
  while (prv_busy(part)) {
  }
}

static void prv_configure_qe(const struct pbl_flash_nrf5_qspi *cfg) {
  const struct pbl_flash_nor_part *part = cfg->part;
  uint8_t sr[2];

  if (!(cfg->read_mode == READ_READ2IO || cfg->read_mode == READ_READ4O ||
        cfg->read_mode == READ_READ4IO || cfg->write_mode == WRITE_PP4O ||
        cfg->write_mode == WRITE_PP4IO)) {
    return;
  }

  switch (part->qer) {
    case PBL_FLASH_NOR_QER_NONE:
      break;
    case PBL_FLASH_NOR_QER_S1B6:
      prv_read_sr1(part, &sr[0]);
      sr[0] |= (1U << 6U);
      prv_cinstr_write(part->cmd.wrsr, sr, 1U);
      break;
    case PBL_FLASH_NOR_QER_S2B1v1:
    case PBL_FLASH_NOR_QER_S2B1v4:
    case PBL_FLASH_NOR_QER_S2B1v5:
      // Writing SR2 requires writing SR1 as well
      prv_read_sr1(part, &sr[0]);
      prv_read_sr2(part, &sr[1]);
      sr[1] |= (1U << 1U);
      prv_cinstr_write(part->cmd.wrsr, sr, 2U);
      break;
    case PBL_FLASH_NOR_QER_S2B1v6:
      prv_read_sr2(part, &sr[1]);
      sr[1] |= (1U << 1U);
      prv_cinstr_write(part->cmd.wrsr2, &sr[1], 1U);
      break;
    default:
      PBL_ASSERTN(false);
  }
}

static int prv_sec_reg_check(const struct pbl_flash_nor_part *part, uint32_t addr) {
  for (uint8_t i = 0U; i < part->sec_regs.count; ++i) {
    if (addr >= part->sec_regs.addrs[i] && addr < part->sec_regs.addrs[i] + part->sec_regs.size) {
      return 0;
    }
  }

  return -EINVAL;
}

// Serialises a 3 or 4 byte address plus optional data into a command payload.
static size_t prv_sec_reg_payload(const struct pbl_flash_nor_part *part, uint32_t addr,
                                  uint8_t *out) {
  size_t len = 0;

  if (part->geometry.size > ADDR_4BYTE_THRESHOLD) {
    out[len++] = (addr >> 24U) & 0xFFU;
  }
  out[len++] = (addr >> 16U) & 0xFFU;
  out[len++] = (addr >> 8U) & 0xFFU;
  out[len++] = addr & 0xFFU;

  return len;
}

static int prv_init(const struct pbl_flash_device *dev) {
  const struct pbl_flash_nrf5_qspi *cfg = prv_cfg(dev);
  const struct pbl_flash_nor_part *part = cfg->part;
  struct pbl_flash_nrf5_qspi_state *state = prv_state(dev);
  nrf_qspi_pins_t conf_pins;
  nrf_qspi_prot_conf_t conf_prot;
  nrf_qspi_phy_conf_t conf_phy;

  if (state->initialized) {
    return 0;
  }

  // QSPI clock is 32MHz, we have dividers from 1 to 16
  PBL_ASSERTN(cfg->clk_freq_hz <= 32000000UL && cfg->clk_freq_hz >= 200000UL);
  if (cfg->clk_freq_hz > 8000000UL) {
    PBL_LOG_WRN(
        "QSPI initialized at %lu Hz, which may cause data corruption if HF clock source switches "
        "(anomaly 244)",
        cfg->clk_freq_hz);
  }

  conf_pins.sck_pin = cfg->clk_gpio;
  conf_pins.csn_pin = cfg->cs_gpio;
  conf_pins.io0_pin = cfg->data_gpio[0];
  conf_pins.io1_pin = cfg->data_gpio[1];
  conf_pins.io2_pin = cfg->data_gpio[2];
  conf_pins.io3_pin = cfg->data_gpio[3];
  nrf_qspi_pins_set(NRF_QSPI, &conf_pins);

  switch (cfg->read_mode) {
    case READ_READ2O:
      conf_prot.readoc = NRF_QSPI_READOC_READ2O;
      break;
    case READ_READ2IO:
      conf_prot.readoc = NRF_QSPI_READOC_READ2IO;
      break;
    case READ_READ4O:
      conf_prot.readoc = NRF_QSPI_READOC_READ4O;
      break;
    case READ_READ4IO:
      conf_prot.readoc = NRF_QSPI_READOC_READ4IO;
      break;
    default:
      conf_prot.readoc = NRF_QSPI_READOC_FASTREAD;
      break;
  }

  switch (cfg->write_mode) {
    case WRITE_PP2O:
      conf_prot.writeoc = NRF_QSPI_WRITEOC_PP2O;
      break;
    case WRITE_PP4O:
      conf_prot.writeoc = NRF_QSPI_WRITEOC_PP4O;
      break;
    case WRITE_PP4IO:
      conf_prot.writeoc = NRF_QSPI_WRITEOC_PP4IO;
      break;
    default:
      conf_prot.writeoc = NRF_QSPI_WRITEOC_PP;
      break;
  }

  if (part->geometry.size > ADDR_4BYTE_THRESHOLD) {
    conf_prot.addrmode = NRF_QSPI_ADDRMODE_32BIT;
  } else {
    conf_prot.addrmode = NRF_QSPI_ADDRMODE_24BIT;
  }
  conf_prot.dpmconfig = false;
  nrf_qspi_ifconfig0_set(NRF_QSPI, &conf_prot);

  conf_phy.sck_delay = 5U;
  conf_phy.dpmen = false;
  conf_phy.spi_mode = NRF_QSPI_MODE_0;
  conf_phy.sck_freq = (32000000UL / cfg->clk_freq_hz) - 1U;
  nrf_qspi_ifconfig1_set(NRF_QSPI, &conf_phy);

  nrf_qspi_enable(NRF_QSPI);

  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
  nrf_qspi_task_trigger(NRF_QSPI, NRF_QSPI_TASK_ACTIVATE);
  while (!nrf_qspi_event_check(NRF_QSPI, NRF_QSPI_EVENT_READY)) {
  }
  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);

  // Reset the flash to stop any program or erase in progress from before reboot
  prv_cinstr(part->cmd.reset_enable);
  prv_cinstr(part->cmd.reset);
  psleep(part->reset_latency_ms);

  uint32_t id = 0;
  prv_cinstr_read(part->cmd.read_id, &id, 3U);
  if (id != part->id) {
    PBL_LOG_ERR("Flash is not %s (id: 0x%06lx)", part->name, id);
  }

  if (conf_prot.addrmode == NRF_QSPI_ADDRMODE_32BIT) {
    prv_cinstr(part->cmd.en4b);
  }

  prv_configure_qe(cfg);

  s_state = state;
  pbl_sem_init(&state->sem, 0, 1);
  NVIC_SetPriority(QSPI_IRQn, 5);
  NVIC_EnableIRQ(QSPI_IRQn);

  state->initialized = true;

  return 0;
}

// The peripheral needs word aligned buffers and lengths: unaligned head and
// tail bytes go through a bounce word.
static int prv_read_op(const struct pbl_flash_device *dev, uint32_t addr, void *buf, size_t len) {
  uint8_t __attribute__((aligned(4))) b_buf[4];
  size_t pre = MIN((4U - ((uintptr_t)buf % 4U)) % 4U, len);
  size_t suf = (len - pre) % 4U;
  size_t mid = len - pre - suf;

  if (pre != 0U) {
    prv_read(dev, b_buf, 4U, addr);
    memcpy(buf, b_buf, pre);
    addr += pre;
    buf = (uint8_t *)buf + pre;
  }

  if (mid != 0U) {
    prv_read(dev, buf, mid, addr);
    addr += mid;
    buf = (uint8_t *)buf + mid;
  }

  if (suf != 0U) {
    prv_read(dev, b_buf, 4U, addr);
    memcpy(buf, b_buf, suf);
  }

  return 0;
}

static void prv_write_page(const struct pbl_flash_device *dev, uint32_t addr, const uint8_t *buf,
                           size_t len) {
  const struct pbl_flash_nor_part *part = prv_cfg(dev)->part;
  uint8_t __attribute__((aligned(4))) b_buf[4];
  size_t pre = MIN((4U - ((uintptr_t)buf % 4U)) % 4U, len);
  size_t suf = (len - pre) % 4U;
  size_t mid = len - pre - suf;

  prv_cinstr(part->cmd.write_enable);

  if (pre != 0U) {
    memset(&b_buf[pre], 0xff, sizeof(b_buf) - pre);
    memcpy(b_buf, buf, pre);
    prv_write(dev, b_buf, 4U, addr);
    addr += pre;
    buf += pre;
  }

  if (mid != 0U) {
    prv_wait_idle(part);
    prv_write(dev, buf, mid, addr);
    addr += mid;
    buf += mid;
  }

  if (suf != 0U) {
    prv_wait_idle(part);
    memset(&b_buf[suf], 0xff, 4U - suf);
    memcpy(b_buf, buf, suf);
    prv_write(dev, b_buf, 4U, addr);
  }

  prv_wait_idle(part);
}

static int prv_write_op(const struct pbl_flash_device *dev, uint32_t addr, const void *buf,
                        size_t len) {
  const struct pbl_flash_nor_part *part = prv_cfg(dev)->part;
  const uint8_t *src = buf;

  while (len > 0) {
    size_t chunk = MIN(len, part->geometry.page_size - (addr % part->geometry.page_size));
    const uint8_t *data = src;

    // The peripheral can only read from RAM
    if (!nrfx_is_in_ram(src)) {
      chunk = MIN(chunk, sizeof(s_bounce_buf));
      memcpy(s_bounce_buf, src, chunk);
      data = s_bounce_buf;
    }

    prv_write_page(dev, addr, data, chunk);
    addr += chunk;
    src += chunk;
    len -= chunk;
  }

  return 0;
}

static int prv_erase_begin(const struct pbl_flash_device *dev, uint32_t addr, size_t size) {
  const struct pbl_flash_nor_part *part = prv_cfg(dev)->part;
  nrf_qspi_erase_len_t len;

  if (size == part->geometry.subsector_size) {
    len = NRF_QSPI_ERASE_LEN_4KB;
  } else if (size == part->geometry.sector_size) {
    len = NRF_QSPI_ERASE_LEN_64KB;
  } else {
    return -EINVAL;
  }

  prv_cinstr(part->cmd.write_enable);

  nrf_qspi_erase_ptr_set(NRF_QSPI, addr, len);
  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
  nrf_qspi_task_trigger(NRF_QSPI, NRF_QSPI_TASK_ERASESTART);
  while (!nrf_qspi_event_check(NRF_QSPI, NRF_QSPI_EVENT_READY)) {
  }
  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);

  // Released once the erase is seen to complete
  soc_nrf_sleep_full_block();

  return 0;
}

static int prv_erase_status(const struct pbl_flash_device *dev) {
  const struct pbl_flash_nor_part *part = prv_cfg(dev)->part;
  uint8_t sr2;

  if (prv_busy(part)) {
    return -EBUSY;
  }

  prv_read_sr2(part, &sr2);
  if ((sr2 & part->mask.sr2_erase_suspend) != 0U) {
    return -EAGAIN;
  }

  soc_nrf_sleep_full_release();

  return 0;
}

static int prv_erase_suspend(const struct pbl_flash_device *dev) {
  const struct pbl_flash_nor_part *part = prv_cfg(dev)->part;

  if (!prv_busy(part)) {
    return 1;
  }

  prv_cinstr(part->cmd.erase_suspend);
  if (part->suspend_to_read_latency_us) {
    delay_us(part->suspend_to_read_latency_us);
  }

  return 0;
}

static int prv_erase_resume(const struct pbl_flash_device *dev) {
  prv_cinstr(prv_cfg(dev)->part->cmd.erase_resume);
  return 0;
}

static void prv_power_down(const struct pbl_flash_device *dev) {
  const struct pbl_flash_nor_part *part = prv_cfg(dev)->part;

  prv_cinstr(part->cmd.enter_low_power);
  if (part->standby_to_low_power_latency_us) {
    delay_us(part->standby_to_low_power_latency_us);
  }

  nrf_qspi_int_disable(NRF_QSPI, NRF_QSPI_INT_READY_MASK);
  nrf_qspi_task_trigger(NRF_QSPI, NRF_QSPI_TASK_DEACTIVATE);
  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
  nrf_qspi_disable(NRF_QSPI);
}

static void prv_power_up(const struct pbl_flash_device *dev) {
  const struct pbl_flash_nor_part *part = prv_cfg(dev)->part;

  nrf_qspi_enable(NRF_QSPI);

  nrf_qspi_int_disable(NRF_QSPI, NRF_QSPI_INT_READY_MASK);
  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);
  nrf_qspi_task_trigger(NRF_QSPI, NRF_QSPI_TASK_ACTIVATE);
  while (!nrf_qspi_event_check(NRF_QSPI, NRF_QSPI_EVENT_READY)) {
  }
  nrf_qspi_event_clear(NRF_QSPI, NRF_QSPI_EVENT_READY);

  prv_cinstr(part->cmd.exit_low_power);
  if (part->low_power_to_standby_latency_us) {
    delay_us(part->low_power_to_standby_latency_us);
  }
}

static int prv_sec_reg_read(const struct pbl_flash_device *dev, uint32_t addr, uint8_t *val) {
  const struct pbl_flash_nor_part *part = prv_cfg(dev)->part;
  uint8_t out[6] = {0};
  uint8_t in[6];
  int ret;

  ret = prv_sec_reg_check(part, addr);
  if (ret != 0) {
    return ret;
  }

  // Address, one dummy byte, then the data byte
  size_t len = prv_sec_reg_payload(part, addr, out) + 2U;
  prv_cinstr_write_read(part->cmd.read_sec, out, in, len);
  *val = in[len - 1U];

  return 0;
}

static int prv_sec_reg_write(const struct pbl_flash_device *dev, uint32_t addr, uint8_t val) {
  const struct pbl_flash_nor_part *part = prv_cfg(dev)->part;
  uint8_t out[5];
  int ret;

  ret = prv_sec_reg_check(part, addr);
  if (ret != 0) {
    return ret;
  }

  size_t len = prv_sec_reg_payload(part, addr, out);
  out[len++] = val;

  prv_cinstr(part->cmd.write_enable);
  prv_cinstr_write(part->cmd.program_sec, out, len);
  prv_wait_idle(part);

  return 0;
}

static int prv_sec_reg_erase(const struct pbl_flash_device *dev, uint32_t addr) {
  const struct pbl_flash_nor_part *part = prv_cfg(dev)->part;
  uint8_t out[4];
  int ret;

  ret = prv_sec_reg_check(part, addr);
  if (ret != 0) {
    return ret;
  }

  size_t len = prv_sec_reg_payload(part, addr, out);

  prv_cinstr(part->cmd.write_enable);
  prv_cinstr_write(part->cmd.erase_sec, out, len);
  prv_wait_idle(part);

  return 0;
}

static int prv_sec_reg_is_locked(const struct pbl_flash_device *dev, uint32_t addr, bool *locked) {
  const struct pbl_flash_nor_part *part = prv_cfg(dev)->part;
  uint8_t sr2;
  int ret;

  ret = prv_sec_reg_check(part, addr);
  if (ret != 0) {
    return ret;
  }

  prv_read_sr2(part, &sr2);
  *locked = (sr2 & ((1U << SEC_ADDR_TO_IDX(addr)) << 3U)) != 0U;

  return 0;
}

#ifdef CONFIG_RECOVERY_FW
static int prv_sec_reg_lock(const struct pbl_flash_device *dev, uint32_t addr) {
  const struct pbl_flash_nor_part *part = prv_cfg(dev)->part;
  uint8_t sr[2];
  int ret;

  ret = prv_sec_reg_check(part, addr);
  if (ret != 0) {
    return ret;
  }

  prv_read_sr1(part, &sr[0]);
  prv_read_sr2(part, &sr[1]);
  sr[1] |= (1U << SEC_ADDR_TO_IDX(addr)) << 3U;
  prv_cinstr_write(part->cmd.wrsr, sr, 2U);

  return 0;
}
#endif

static const struct pbl_flash_ops s_ops = {
    .init = prv_init,
    .read = prv_read_op,
    .write = prv_write_op,
    .erase_begin = prv_erase_begin,
    .erase_status = prv_erase_status,
    .erase_suspend = prv_erase_suspend,
    .erase_resume = prv_erase_resume,
    .power_down = prv_power_down,
    .power_up = prv_power_up,
    .sec_reg_read = prv_sec_reg_read,
    .sec_reg_write = prv_sec_reg_write,
    .sec_reg_erase = prv_sec_reg_erase,
    .sec_reg_is_locked = prv_sec_reg_is_locked,
#ifdef CONFIG_RECOVERY_FW
    .sec_reg_lock = prv_sec_reg_lock,
#endif
};

#if defined(CONFIG_FLASH_NRF5_QSPI_READ_FASTREAD)
#define READ_MODE READ_FASTREAD
#elif defined(CONFIG_FLASH_NRF5_QSPI_READ_READ2O)
#define READ_MODE READ_READ2O
#elif defined(CONFIG_FLASH_NRF5_QSPI_READ_READ2IO)
#define READ_MODE READ_READ2IO
#elif defined(CONFIG_FLASH_NRF5_QSPI_READ_READ4O)
#define READ_MODE READ_READ4O
#else
#define READ_MODE READ_READ4IO
#endif

#if defined(CONFIG_FLASH_NRF5_QSPI_WRITE_PP)
#define WRITE_MODE WRITE_PP
#elif defined(CONFIG_FLASH_NRF5_QSPI_WRITE_PP2O)
#define WRITE_MODE WRITE_PP2O
#elif defined(CONFIG_FLASH_NRF5_QSPI_WRITE_PP4IO)
#define WRITE_MODE WRITE_PP4IO
#else
#define WRITE_MODE WRITE_PP4O
#endif

static struct pbl_flash_nrf5_qspi_state s_flash_state;
static const struct pbl_flash_nrf5_qspi s_flash = {
    .dev =
        {
            .state = &s_flash_state.flash,
            .ops = &s_ops,
            .geometry = &PBL_FLASH_NOR_PART.geometry,
            .sec_regs = &PBL_FLASH_NOR_PART.sec_regs,
        },
    .part = &PBL_FLASH_NOR_PART,
    .clk_freq_hz = CONFIG_FLASH_NRF5_QSPI_CLK_FREQ_HZ,
    .cs_gpio = CONFIG_FLASH_NRF5_QSPI_CSN_PIN,
    .clk_gpio = CONFIG_FLASH_NRF5_QSPI_SCK_PIN,
    .data_gpio =
        {
            CONFIG_FLASH_NRF5_QSPI_IO0_PIN,
            CONFIG_FLASH_NRF5_QSPI_IO1_PIN,
            CONFIG_FLASH_NRF5_QSPI_IO2_PIN,
            CONFIG_FLASH_NRF5_QSPI_IO3_PIN,
        },
    .read_mode = READ_MODE,
    .write_mode = WRITE_MODE,
};
const struct pbl_flash_device *const FLASH = &s_flash.dev;
