#include "drivers/qspi.h"

#include "board/board.h"
#include "drivers/dma.h"
#include "drivers/flash/flash_impl.h"
#include "drivers/flash/qspi_flash.h"
#include "drivers/flash/qspi_flash_definitions.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "drivers/qspi_definitions.h"
#include "flash_region/flash_region.h"
#include "kernel/util/delay.h"
#include "kernel/util/sleep.h"
#include "kernel/util/stop.h"
#include "mcu/cache.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/math.h"
#include "services/common/new_timer/new_timer.h"
#include "os/mutex.h"
#include "drivers/clocksource.h"

#define NRF5_COMPATIBLE
#include <mcu.h>
#include <nrfx.h>
#include <nrfx_qspi.h>

#include "FreeRTOS.h"
#include "semphr.h"

/* nRF5's QSPI controller is different enough from STM32's that we
 * reimplement qspi_flash.c, not stm32/qspi.c.  */

#define FLASH_RESET_WORD_VALUE (0xffffffff)

static uint8_t s_qspi_ram_buffer[32];

status_t flash_impl_set_nvram_erase_status(bool is_subsector, FlashAddress addr) {
  return S_SUCCESS;
}

status_t flash_impl_clear_nvram_erase_status(void) { return S_SUCCESS; }

status_t flash_impl_get_nvram_erase_status(bool *is_subsector, FlashAddress *addr) {
  // XXX
  return S_FALSE;
}

/*** QSPI sleep routines ***
 *
 * In the STM32 world, it is sufficient to just put the flash in DPD around
 * stop mode.  But on nRF52, it's actually not the flash that uses that much
 * power -- it's the flash *controller*!  Having the flash controller turned
 * on could cost as much as 800uA compared to the flash itself, which
 * probably is around 100 uA when not in DPD.
 *
 * We really want to shut it off ASAP, even if we're not in stop mode -- and
 * we definitely don't want to power it back up when we leave stop mode,
 * because powering it up is pretty slow (it requires turning on the HFXO in
 * order to be reliable).  So instead of using the PebbleOS "please enter
 * low power mode" hints, we set a new_timer of our own to power up on first
 * access, and then power down after 50ms of nobody talking to us.
 */

// Not too long that we're burning power, not too short that we're spending
// all our time turning the HFXO on and off.
#define FLASH_TIMEOUT_MS 50

// #define DEBUG_FLASH_SLEEP_TIME

static bool s_is_init = false;
static bool s_is_awake = false;
static int s_is_active = 0; // some QSPI functions are reentrant (like blank-check during erase), so this needs to not be a bool

static void prv_write_cmd_no_addr(QSPIPort *dev, uint8_t cmd);
static void _flash_handler(nrfx_qspi_evt_t event, void *ctx);

static TimerID s_sleep_timer;
static PebbleMutex *s_sleep_mutex;

static void prv_sleep_timeout(void *arg) {
  QSPIFlash *dev = (QSPIFlash *)arg;
  if (s_is_active || dev->state->coredump_mode) {
    // the new_timer will come and try again later
    return;
  }

#ifdef DEBUG_FLASH_SLEEP_TIME
#  if defined(DEBUG_FLASH_SLEEP_TIME) && FLASH_LOG_LEVEL >= LOG_LEVEL_DEBUG
#    error you goofball, you cannot use the flash as a backing medium for logs debugging the flash.  turn down the log level!
#  endif

  PBL_LOG(LOG_LEVEL_DEBUG, "flash is going back to sleep");
#endif

  mutex_lock(s_sleep_mutex);

  new_timer_stop(s_sleep_timer);

  prv_write_cmd_no_addr(dev->qspi, dev->state->part->instructions.enter_low_power);
  delay_us(dev->state->part->standby_to_low_power_latency_us);

  nrfx_qspi_deactivate();

  clocksource_hfxo_release();

  s_is_awake = false;

  mutex_unlock(s_sleep_mutex);
}

static void prv_use(QSPIFlash *dev) {
  if (!dev->state->coredump_mode) {
    mutex_lock(s_sleep_mutex);
  }

  PBL_ASSERTN(s_is_init);
  s_is_active++;

  if (s_is_awake) {
    if (!dev->state->coredump_mode) {
      new_timer_stop(s_sleep_timer);
      mutex_unlock(s_sleep_mutex);
    }
    return;
  }

#ifdef DEBUG_FLASH_SLEEP_TIME
  PBL_LOG(LOG_LEVEL_DEBUG, "flash is waking up");
#endif

  clocksource_hfxo_request();

  nrfx_err_t err;
  err = nrfx_qspi_activate(true /* wait */);
  PBL_ASSERTN(err == NRFX_SUCCESS);

  prv_write_cmd_no_addr(dev->qspi, dev->state->part->instructions.exit_low_power);
  delay_us(dev->state->part->low_power_to_standby_latency_us);

  s_is_awake = true;

  if (!dev->state->coredump_mode) {
    mutex_unlock(s_sleep_mutex);
  }
}

static void prv_release(QSPIFlash *dev) {
  if (!dev->state->coredump_mode) {
    mutex_lock(s_sleep_mutex);
  }

  PBL_ASSERTN(s_is_active);
  s_is_active--;
  // if not in coredump node, set new_timer to go back to sleep...
  if (!dev->state->coredump_mode && !s_is_active) {
    new_timer_start(s_sleep_timer, FLASH_TIMEOUT_MS, prv_sleep_timeout, (void *)dev, 0 /* flags */);
  }

  if (!dev->state->coredump_mode) {
    mutex_unlock(s_sleep_mutex);
  }
}

void qspi_flash_set_lower_power_mode(QSPIFlash *dev, bool active) {
  /* This is a no-op on nRF5, because do our own timer-based power
   * management to turn on and shut down not just the flash but the QSPI
   * controller also (which is way more expensive than the flash!).
   */
}

/*** Underlying small flash I/O helpers. ***/

static void prv_read_register(QSPIPort *dev, uint8_t instruction, uint8_t *data, uint32_t length) {
  nrf_qspi_cinstr_conf_t instr = NRFX_QSPI_DEFAULT_CINSTR(instruction, length + 1);
  instr.io2_level = true;
  instr.io3_level = true;
  nrfx_err_t err = nrfx_qspi_cinstr_xfer(&instr, NULL, data);
  PBL_ASSERTN(err == NRFX_SUCCESS);
}

static void prv_write_register(QSPIPort *dev, uint8_t instruction, const uint8_t *data,
                               uint32_t length) {
  nrf_qspi_cinstr_conf_t instr = NRFX_QSPI_DEFAULT_CINSTR(instruction, length + 1);
  instr.io2_level = true;
  instr.io3_level = true;
  instr.wren = true;
  nrfx_err_t err = nrfx_qspi_cinstr_xfer(&instr, data, NULL);
  PBL_ASSERTN(err == NRFX_SUCCESS);
}

static void prv_write_cmd_no_addr(QSPIPort *dev, uint8_t cmd) {
  nrf_qspi_cinstr_conf_t instr = NRFX_QSPI_DEFAULT_CINSTR(cmd, 1);
  instr.io2_level = true;
  instr.io3_level = true;
  nrfx_err_t err = nrfx_qspi_cinstr_xfer(&instr, NULL, NULL);
  PBL_ASSERTN(err == NRFX_SUCCESS);
}

static bool prv_poll_bit(QSPIPort *dev, uint8_t instruction, uint8_t bit_mask, bool should_be_set,
                         uint32_t timeout_us) {
  uint32_t loops = 0;
  uint8_t val;
  while (1) {
    prv_read_register(dev, instruction, &val, 1);
    if ((!!(val & bit_mask)) == should_be_set) break;
    if ((timeout_us != QSPI_NO_TIMEOUT) && (++loops > timeout_us)) {
      PBL_LOG(LOG_LEVEL_ERROR, "Timeout waiting for a bit!?!?");
      return false;
    }
    delay_us(1);
  }

  return true;
}

static void prv_write_enable(QSPIFlash *dev) {
  prv_write_cmd_no_addr(dev->qspi, dev->state->part->instructions.write_enable);
  // wait for writing to be enabled
  prv_poll_bit(dev->qspi, dev->state->part->instructions.rdsr1,
               dev->state->part->status_bit_masks.write_enable, true /* set */, QSPI_NO_TIMEOUT);
}

static void prv_wait_for_completion(QSPIFlash *dev) {
  if (!dev->state->coredump_mode) {
    xSemaphoreTake(dev->qspi->state->dma_semaphore, portMAX_DELAY);
  }
}

static bool prv_check_whoami(QSPIFlash *dev) {
  // The WHOAMI is 3 bytes
  const uint32_t whoami_length = 3;
  uint32_t read_whoami = 0;
  prv_read_register(dev->qspi, dev->state->part->instructions.qspi_id, (uint8_t *)&read_whoami,
                    whoami_length);

  if (read_whoami == dev->state->part->qspi_id_value) {
    PBL_LOG(LOG_LEVEL_INFO, "Flash is %s", dev->state->part->name);
    return true;
  } else {
    PBL_LOG(LOG_LEVEL_ERROR, "Flash isn't expected %s (whoami: 0x%" PRIx32 ")",
            dev->state->part->name, read_whoami);
    return false;
  }
}

bool qspi_flash_check_whoami(QSPIFlash *dev) {
  prv_use(dev);
  bool rv = prv_check_whoami(dev);
  prv_release(dev);
  return rv;
}

bool qspi_flash_is_in_coredump_mode(QSPIFlash *dev) { return dev->state->coredump_mode; }

static void _flash_handler(nrfx_qspi_evt_t event, void *ctx) {
  QSPIFlash *dev = (QSPIFlash *)ctx;
  BaseType_t woken = pdFALSE;

  PBL_ASSERTN(event == NRFX_QSPI_EVENT_DONE);

  xSemaphoreGiveFromISR(dev->qspi->state->dma_semaphore, &woken);
  portYIELD_FROM_ISR(woken);
}

/*** Flash init routines. ***/

static void prv_configure_qe(QSPIFlash *dev) {
  uint8_t sr[2];

  // Check first if read/write mode requires QE to be set
  if (!(dev->read_mode == QSPI_FLASH_READ_READ2IO || dev->read_mode == QSPI_FLASH_READ_READ4O ||
        dev->read_mode == QSPI_FLASH_READ_READ4IO || dev->write_mode == QSPI_FLASH_WRITE_PP4O ||
        dev->write_mode == QSPI_FLASH_WRITE_PP4IO)) {
    return;
  }

  // Check if QE is needed
  if (dev->state->part->qer_type == JESD216_DW15_QER_NONE) {
    return;
  }

  // Enable QE bit
  switch (dev->state->part->qer_type) {
    case JESD216_DW15_QER_S1B6:
      prv_read_register(dev->qspi, dev->state->part->instructions.rdsr1, sr, 1);
      sr[0] |= (1 << 6);
      prv_write_register(dev->qspi, dev->state->part->instructions.wrsr, sr, 1);
      break;
    case JESD216_DW15_QER_S2B1v1:
    case JESD216_DW15_QER_S2B1v4:
    case JESD216_DW15_QER_S2B1v5:
      // Writing SR2 requires writing SR1 as well
      prv_read_register(dev->qspi, dev->state->part->instructions.rdsr1, &sr[0], 1);
      prv_read_register(dev->qspi, dev->state->part->instructions.rdsr2, &sr[1], 1);
      sr[1] |= (1 << 1);
      prv_write_register(dev->qspi, dev->state->part->instructions.wrsr, sr, 2);
      break;
    case JESD216_DW15_QER_S2B1v6:
      // We can write SR2 without writing SR1
      prv_read_register(dev->qspi, dev->state->part->instructions.rdsr2, sr, 1);
      sr[0] |= (1 << 1);
      prv_write_register(dev->qspi, dev->state->part->instructions.wrsr2, sr, 1);
      break;
    default:
      PBL_ASSERT(false, "Unsupported QER type %d", dev->state->part->qer_type);
  }
}

void qspi_flash_init(QSPIFlash *dev, QSPIFlashPart *part, bool coredump_mode) {
  dev->state->part = part;
  dev->state->coredump_mode = coredump_mode;

  // Init the DMA semaphore, used for I/O ops
  if (!coredump_mode) {
    dev->qspi->state->dma_semaphore = xSemaphoreCreateBinary();
    s_sleep_timer = new_timer_create();
    s_sleep_mutex = mutex_create();
  }

  nrfx_qspi_config_t config = NRFX_QSPI_DEFAULT_CONFIG(
      dev->qspi->clk_gpio, dev->qspi->cs_gpio, dev->qspi->data_gpio[0], dev->qspi->data_gpio[1],
      dev->qspi->data_gpio[2], dev->qspi->data_gpio[3]);
  config.phy_if.sck_freq = NRF_QSPI_FREQ_DIV4;

  switch (dev->read_mode) {
    case QSPI_FLASH_READ_READ2O:
      config.prot_if.readoc = NRF_QSPI_READOC_READ2O;
      break;
    case QSPI_FLASH_READ_READ2IO:
      config.prot_if.readoc = NRF_QSPI_READOC_READ2IO;
      break;
    case QSPI_FLASH_READ_READ4O:
      config.prot_if.readoc = NRF_QSPI_READOC_READ4O;
      break;
    case QSPI_FLASH_READ_READ4IO:
      config.prot_if.readoc = NRF_QSPI_READOC_READ4IO;
      break;
    default:
      config.prot_if.readoc = NRF_QSPI_READOC_FASTREAD;
      break;
  }

  switch (dev->write_mode) {
    case QSPI_FLASH_WRITE_PP2O:
      config.prot_if.writeoc = NRF_QSPI_WRITEOC_PP2O;
      break;
    case QSPI_FLASH_WRITE_PP4O:
      config.prot_if.writeoc = NRF_QSPI_WRITEOC_PP4O;
      break;
    case QSPI_FLASH_WRITE_PP4IO:
      config.prot_if.writeoc = NRF_QSPI_WRITEOC_PP4IO;
      break;
    default:
      config.prot_if.writeoc = NRF_QSPI_WRITEOC_PP;
      break;
  }

  if (dev->state->part->size > 0x1000000) {
    config.prot_if.addrmode = NRF_QSPI_ADDRMODE_32BIT;
  } else {
    config.prot_if.addrmode = NRF_QSPI_ADDRMODE_24BIT;
  }

  nrfx_err_t err;
  if (s_is_init) {
    nrfx_qspi_uninit();
  }
  err = nrfx_qspi_init(&config, coredump_mode ? NULL : _flash_handler, (void *)dev);
  PBL_ASSERTN(err == NRFX_SUCCESS);
  s_is_init = true;

  if (dev->reset_gpio.gpio) {
    WTF;
  }

  // Reset the flash to stop any program's or erase in progress from before reboot
  prv_write_cmd_no_addr(dev->qspi, dev->state->part->instructions.reset_enable);
  prv_write_cmd_no_addr(dev->qspi, dev->state->part->instructions.reset);

  if (coredump_mode) {
    delay_us(dev->state->part->reset_latency_ms * 1000);
  } else {
    psleep(dev->state->part->reset_latency_ms);
  }

  if (!coredump_mode) {
    prv_check_whoami(dev);
  }

  if (config.prot_if.addrmode == NRF_QSPI_ADDRMODE_32BIT) {
    prv_write_cmd_no_addr(dev->qspi, dev->state->part->instructions.en4b);
  }

  prv_configure_qe(dev);

  s_is_awake = true;

  if (!coredump_mode) {
    // We go to sleep right away so that we're in a consistent state --
    // someone will wake us up when they need to.
    prv_write_cmd_no_addr(dev->qspi, dev->state->part->instructions.enter_low_power);
    delay_us(dev->state->part->standby_to_low_power_latency_us);
    nrfx_qspi_deactivate();
    s_is_awake = false;
  }
}

/*** Actual underlying flash implementation. ***/

status_t qspi_flash_is_erase_complete(QSPIFlash *dev) {
  uint8_t status_reg;
  uint8_t flag_status_reg;

  prv_use(dev);

  prv_read_register(dev->qspi, dev->state->part->instructions.rdsr1, &status_reg, 1);
  prv_read_register(dev->qspi, dev->state->part->instructions.rdsr2, &flag_status_reg, 1);

  prv_release(dev);

  if (status_reg & dev->state->part->status_bit_masks.busy) {
    return E_BUSY;
  } else if (flag_status_reg & dev->state->part->flag_status_bit_masks.erase_suspend) {
    return E_AGAIN;
  } else {
    return S_SUCCESS;
  }
}

status_t qspi_flash_erase_begin(QSPIFlash *dev, uint32_t addr, bool is_subsector) {
  prv_use(dev);

  prv_write_enable(dev);

  nrfx_err_t err =
      nrfx_qspi_erase(is_subsector ? NRF_QSPI_ERASE_LEN_4KB : NRF_QSPI_ERASE_LEN_64KB, addr);
  PBL_ASSERTN(err == NRFX_SUCCESS);

  prv_wait_for_completion(dev);

  // wait for busy to be set indicating the erase has started
  const uint32_t busy_timeout_us = 500;
  const bool result =
      prv_poll_bit(dev->qspi, dev->state->part->instructions.rdsr1,
                   dev->state->part->status_bit_masks.busy, true /* set */, busy_timeout_us);

  prv_release(dev);

  return result ? S_SUCCESS : E_ERROR;
}

status_t qspi_flash_erase_suspend(QSPIFlash *dev, uint32_t addr) {
  uint8_t status_reg;

  prv_use(dev);

  prv_read_register(dev->qspi, dev->state->part->instructions.rdsr1, &status_reg, 1);
  if (!(status_reg & dev->state->part->status_bit_masks.busy)) {
    // no erase in progress
    prv_release(dev);
    return S_NO_ACTION_REQUIRED;
  }

  prv_write_cmd_no_addr(dev->qspi, dev->state->part->instructions.erase_suspend);

  if (dev->state->part->suspend_to_read_latency_us) {
    delay_us(dev->state->part->suspend_to_read_latency_us);
  }

  prv_release(dev);

  return S_SUCCESS;
}

void qspi_flash_erase_resume(QSPIFlash *dev, uint32_t addr) {
  prv_use(dev);

  prv_write_cmd_no_addr(dev->qspi, dev->state->part->instructions.erase_resume);
  // wait for the erase_suspend bit to be cleared
  prv_poll_bit(dev->qspi, dev->state->part->instructions.rdsr2,
               dev->state->part->flag_status_bit_masks.erase_suspend, false /* !set */,
               QSPI_NO_TIMEOUT);

  prv_release(dev);
}

void qspi_flash_read_blocking(QSPIFlash *dev, uint32_t addr, void *buffer, uint32_t length) {
  uint8_t __attribute__((aligned(4))) b_buf[4];
  uint8_t buf_pre;
  uint8_t buf_suf;
  uint32_t buf_mid;
  nrfx_err_t err;

  prv_use(dev);

  buf_pre = (4U - (uint8_t)((uint32_t)buffer % 4U)) % 4U;
  if (buf_pre > length) {
    buf_pre = length;
  }

  buf_suf = (uint8_t)((length - buf_pre) % 4U);
  buf_mid = length - buf_pre - buf_suf;

  if (buf_pre != 0U) {
    err = nrfx_qspi_read(b_buf, 4U, addr);
    prv_wait_for_completion(dev);
    PBL_ASSERTN(err == NRFX_SUCCESS);

    memcpy(buffer, b_buf, buf_pre);
    addr += buf_pre;
    buffer = ((uint8_t *)buffer) + buf_pre;
  }

  if (buf_mid != 0U) {
    err = nrfx_qspi_read(buffer, buf_mid, addr);
    prv_wait_for_completion(dev);
    PBL_ASSERTN(err == NRFX_SUCCESS);

    addr += buf_mid;
    buffer = ((uint8_t *)buffer) + buf_mid;
  }

  if (buf_suf != 0U) {
    err = nrfx_qspi_read(b_buf, 4U, addr);
    prv_wait_for_completion(dev);
    PBL_ASSERTN(err == NRFX_SUCCESS);

    memcpy(buffer, b_buf, buf_suf);
  }

  prv_release(dev);
}

int qspi_flash_write_page_begin(QSPIFlash *dev, const void *buffer, uint32_t addr,
                                uint32_t length) {
  uint8_t __attribute__((aligned(4))) b_buf[4];
  uint8_t buf_pre;
  uint8_t buf_suf;
  uint32_t buf_mid;
  nrfx_err_t err;

  prv_use(dev);

  // we can write from start address up to the end of the page
  length = MIN(length, PAGE_SIZE_BYTES - (addr % PAGE_SIZE_BYTES));

  // bounce data to RAM if not in RAM
  if (!nrfx_is_in_ram(buffer)) {
    length = MIN(length, sizeof(s_qspi_ram_buffer));
    memcpy(s_qspi_ram_buffer, buffer, length);
    buffer = s_qspi_ram_buffer;
  }

  buf_pre = (4U - (uint8_t)((uint32_t)buffer % 4U)) % 4U;
  if (buf_pre > length) {
    buf_pre = length;
  }

  buf_suf = (uint8_t)((length - buf_pre) % 4U);
  buf_mid = length - buf_pre - buf_suf;

  prv_write_enable(dev);

  if (buf_pre != 0U) {
    memset(&b_buf[buf_pre], 0xff, sizeof(b_buf) - buf_pre);
    memcpy(b_buf, buffer, buf_pre);

    err = nrfx_qspi_write(b_buf, 4U, addr);
    prv_wait_for_completion(dev);
    PBL_ASSERTN(err == NRFX_SUCCESS);

    addr += buf_pre;
    buffer = ((uint8_t *)buffer) + buf_pre;
  }

  if (buf_mid != 0U) {
    while (nrfx_qspi_mem_busy_check() == NRFX_ERROR_BUSY) {
    }

    err = nrfx_qspi_write(buffer, buf_mid, addr);
    prv_wait_for_completion(dev);
    PBL_ASSERTN(err == NRFX_SUCCESS);

    addr += buf_mid;
    buffer = ((uint8_t *)buffer) + buf_mid;
  }

  if (buf_suf != 0U) {
    while (nrfx_qspi_mem_busy_check() == NRFX_ERROR_BUSY) {
    }

    memset(&b_buf[buf_suf], 0xff, 4U - buf_suf);
    memcpy(b_buf, buffer, buf_suf);

    err = nrfx_qspi_write(b_buf, 4U, addr);
    prv_wait_for_completion(dev);
    PBL_ASSERTN(err == NRFX_SUCCESS);
  }

  prv_release(dev);

  return length;
}

status_t qspi_flash_get_write_status(QSPIFlash *dev) {
  prv_use(dev);
  status_t rv = nrfx_qspi_mem_busy_check() == NRFX_SUCCESS ? S_SUCCESS : E_BUSY;
  prv_release(dev);
  return rv;
}


static bool prv_blank_check_poll(QSPIFlash *dev, uint32_t addr, bool is_subsector) {
  const uint32_t size_bytes = is_subsector ? SUBSECTOR_SIZE_BYTES : SECTOR_SIZE_BYTES;
  const uint32_t BUF_SIZE_BYTES = 128;
  const uint32_t BUF_SIZE_WORDS = BUF_SIZE_BYTES / sizeof(uint32_t);
  uint32_t buffer[BUF_SIZE_WORDS];
  for (uint32_t offset = 0; offset < size_bytes; offset += BUF_SIZE_BYTES) {
    flash_impl_read_sync(buffer, addr + offset, BUF_SIZE_BYTES);
    for (uint32_t i = 0; i < BUF_SIZE_WORDS; ++i) {
      if (buffer[i] != FLASH_RESET_WORD_VALUE) {
        return false;
      }
    }
  }
  return true;
}

status_t qspi_flash_blank_check(QSPIFlash *dev, uint32_t addr, bool is_subsector) {
  prv_use(dev);
  const bool result = prv_blank_check_poll(dev, addr, is_subsector);
  prv_release(dev);
  return result ? S_TRUE : S_FALSE;
}

void qspi_flash_ll_set_register_bits(QSPIFlash *dev, uint8_t read_instruction,
                                     uint8_t write_instruction, uint8_t value, uint8_t mask) {
  prv_use(dev);

  // make sure we're not trying to set any bits not within the mask
  PBL_ASSERTN((value & mask) == value);

  // first read the register
  uint8_t reg_value;
  prv_read_register(dev->qspi, read_instruction, &reg_value, 1);

  // set the desired bits
  reg_value = (reg_value & ~mask) | value;

  // enable writing and write the register value
  prv_write_cmd_no_addr(dev->qspi, dev->state->part->instructions.write_enable);
  qspi_indirect_write_no_addr(dev->qspi, write_instruction, &reg_value, 1);

  prv_release(dev);
}

status_t qspi_flash_write_protection_enable(QSPIFlash *dev) { return S_NO_ACTION_REQUIRED; }

status_t qspi_flash_lock_sector(QSPIFlash *dev, uint32_t addr) { return S_SUCCESS; }

status_t qspi_flash_unlock_all(QSPIFlash *dev) { return S_SUCCESS; }

status_t prv_qspi_security_register_check(QSPIFlash *dev, uint32_t addr) {
  bool addr_valid = false;

  if (dev->state->part->sec_registers.num_sec_regs == 0U) {
    return E_INVALID_OPERATION;
  }

  for (uint8_t i = 0U; i < dev->state->part->sec_registers.num_sec_regs; ++i) {
    if (addr >= dev->state->part->sec_registers.sec_regs[i] &&
        addr < dev->state->part->sec_registers.sec_regs[i] +
               dev->state->part->sec_registers.sec_reg_size) {
      addr_valid = true;
      break;
    }
  }

  if (!addr_valid) {
    return E_INVALID_ARGUMENT;
  }

  return S_SUCCESS;
}

status_t qspi_flash_read_security_register(QSPIFlash *dev, uint32_t addr, uint8_t *val) {
  status_t ret;
  nrfx_err_t err;
  nrf_qspi_cinstr_conf_t instr = NRFX_QSPI_DEFAULT_CINSTR(dev->state->part->instructions.read_sec, 0);
  uint8_t out[6];
  uint8_t in[6];

  prv_use(dev);

  ret = prv_qspi_security_register_check(dev, addr);
  if (ret != S_SUCCESS) {
    prv_release(dev);
    return ret;
  }

  instr.io2_level = true;
  instr.io3_level = true;

  if (dev->state->part->size > 0x1000000) {
    instr.length = 7;
    out[0] = (addr >> 24U);
    out[1] = (addr >> 16U) & 0xFFU;
    out[2] = (addr >> 8U) & 0xFFU;
    out[3] = addr & 0xFFU;
  } else {
    instr.length = 6;
    out[0] = (addr >> 16U) & 0xFFU;
    out[1] = (addr >> 8U) & 0xFFU;
    out[2] = addr & 0xFFU;
  }

  err = nrfx_qspi_cinstr_xfer(&instr, out, in);
  if (err != NRFX_SUCCESS) {
    prv_release(dev);
    return E_ERROR;
  }

  if (dev->state->part->size > 0x1000000) {
    *val = in[5];
  } else {
    *val = in[4];
  }

  prv_release(dev);

  return 0;
}

status_t qspi_flash_security_registers_are_locked(QSPIFlash *dev, bool *locked) {
  uint8_t sr2;

  prv_use(dev);

  prv_read_register(dev->qspi, dev->state->part->instructions.rdsr2, &sr2, 1);

  prv_release(dev);

  *locked = !!(sr2 & dev->state->part->flag_status_bit_masks.sec_lock);

  return 0;
}

status_t qspi_flash_erase_security_register(QSPIFlash *dev, uint32_t addr) {
  status_t ret;
  nrfx_err_t err;
  nrf_qspi_cinstr_conf_t instr = NRFX_QSPI_DEFAULT_CINSTR(dev->state->part->instructions.erase_sec, 0);
  uint8_t out[4];

  prv_use(dev);

  ret = prv_qspi_security_register_check(dev, addr);
  if (ret != S_SUCCESS) {
    prv_release(dev);
    return ret;
  }

  instr.io2_level = true;
  instr.io3_level = true;
  instr.wren = true;

  if (dev->state->part->size > 0x1000000) {
    instr.length = 5;
    out[0] = (addr >> 24U);
    out[1] = (addr >> 16U) & 0xFFU;
    out[2] = (addr >> 8U) & 0xFFU;
    out[3] = addr & 0xFFU;
  } else {
    instr.length = 4;
    out[0] = (addr >> 16U) & 0xFFU;
    out[1] = (addr >> 8U) & 0xFFU;
    out[2] = addr & 0xFFU;
  }

  err = nrfx_qspi_cinstr_xfer(&instr, out, NULL);
  if (err != NRFX_SUCCESS) {
    prv_release(dev);
    return E_ERROR;
  }

  while (nrfx_qspi_mem_busy_check() == NRFX_ERROR_BUSY) {
  }

  prv_release(dev);

  return 0;
}

status_t qspi_flash_write_security_register(QSPIFlash *dev, uint32_t addr, uint8_t val) {
  status_t ret;
  nrfx_err_t err;
  nrf_qspi_cinstr_conf_t instr = NRFX_QSPI_DEFAULT_CINSTR(dev->state->part->instructions.program_sec, 0);
  uint8_t out[5];

  prv_use(dev);

  ret = prv_qspi_security_register_check(dev, addr);
  if (ret != S_SUCCESS) {
    prv_release(dev);
    return ret;
  }

  instr.io2_level = true;
  instr.io3_level = true;
  instr.wren = true;

  if (dev->state->part->size > 0x1000000) {
    instr.length = 6;
    out[0] = (addr >> 24U);
    out[1] = (addr >> 16U) & 0xFFU;
    out[2] = (addr >> 8U) & 0xFFU;
    out[3] = addr & 0xFFU;
    out[4] = val;
  } else {
    instr.length = 5;
    out[0] = (addr >> 16U) & 0xFFU;
    out[1] = (addr >> 8U) & 0xFFU;
    out[2] = addr & 0xFFU;
    out[3] = val;
  }

  err = nrfx_qspi_cinstr_xfer(&instr, out, NULL);
  if (err != NRFX_SUCCESS) {
    prv_release(dev);
    return E_ERROR;
  }

  while (nrfx_qspi_mem_busy_check() == NRFX_ERROR_BUSY) {
  }

  prv_release(dev);

  return 0;
}

const FlashSecurityRegisters *qspi_flash_security_registers_info(QSPIFlash *dev) {
  return &dev->state->part->sec_registers;
}

#ifdef RECOVERY_FW
status_t qspi_flash_lock_security_registers(QSPIFlash *dev) {
  uint8_t sr[2];

  prv_use(dev);

  prv_read_register(dev->qspi, dev->state->part->instructions.rdsr1, &sr[0], 1);
  prv_read_register(dev->qspi, dev->state->part->instructions.rdsr2, &sr[1], 1);

  sr[1] |= dev->state->part->flag_status_bit_masks.sec_lock;

  prv_write_register(dev->qspi, dev->state->part->instructions.wrsr, sr, 2);

  prv_release(dev);

  return 0;
}
#endif // RECOVERY_FW

#if !RELEASE
#include "console/prompt.h"
#include "drivers/flash.h"
#include "kernel/pbl_malloc.h"
#include "system/profiler.h"
#include "util/size.h"

static bool prv_flash_read_verify(QSPIFlash *dev, int size, int offset) {
  bool success = true;

  profiler_start();
  // prv_read_mmap(dev, 0, buffer_mmap, size);
  profiler_stop();
  uint32_t mmap_time = profiler_get_total_duration(true);

  const int buf_size = 64;
  char buf[buf_size];
  prompt_send_response_fmt(buf, buf_size, "Size: %d UNIMPL: MMP: %" PRIu32, size, mmap_time);

  return success;
}

struct FlashReadTestValues {
  int size;
  int offset;
};

const struct FlashReadTestValues FLASH_READ_TEST_TABLE[] = {
    {.size = 1024, .offset = 0}, {.size = 1025, .offset = 0}, {.size = 1026, .offset = 0},
    {.size = 1027, .offset = 0}, {.size = 1024, .offset = 1}, {.size = 1025, .offset = 2},
    {.size = 1026, .offset = 3}, {.size = 4, .offset = 0},    {.size = 20, .offset = 0},
    {.size = 60, .offset = 0},   {.size = 127, .offset = 0},  {.size = 128, .offset = 0},
};

void command_flash_apicheck(const char *len_str) {
  QSPIFlash *dev = QSPI_FLASH;
  const int buf_size = 64;
  char buf[buf_size];
  int failures = 0;
  int passes = 0;

  profiler_init();

  prompt_send_response("Check whoami");
  if (!qspi_flash_check_whoami(dev)) {
    ++failures;
    prompt_send_response("ERROR: Who am I failed");
  } else {
    ++passes;
  }

  prompt_send_response("Enter low power mode");
  flash_impl_enter_low_power_mode();

  // WHOAMI should fail in low-power mode
  prompt_send_response("Check whoami, should fail in low power mode");
  if (qspi_flash_check_whoami(dev)) {
    ++failures;
    prompt_send_response("ERROR: Who am I failed");
  } else {
    ++passes;
  }

  prompt_send_response("Exit low power mode");
  flash_impl_exit_low_power_mode();

  prompt_send_response("Start flash_read_verify test");

  const int final_size = atoi(len_str);

  // If size is 0 run through a pre-defined table
  if (final_size == 0) {
    for (unsigned int i = 0; i < ARRAY_LENGTH(FLASH_READ_TEST_TABLE); ++i) {
      bool result = prv_flash_read_verify(dev, FLASH_READ_TEST_TABLE[i].size,
                                          FLASH_READ_TEST_TABLE[i].offset);
      if (!result) {
        ++failures;
      } else {
        ++passes;
      }
    }

  } else {
    if (prv_flash_read_verify(dev, final_size, 3)) {
      ++passes;
    } else {
      ++failures;
      prompt_send_response("ERROR: flash_read_verify failed");
    }
  }

  bool was_busy = false;

  // write a few bytes to the sector we're going to erase so it's not empty
  uint8_t dummy_data = 0x55;
  flash_write_bytes(&dummy_data, FLASH_REGION_FIRMWARE_SLOT_1_BEGIN, sizeof(dummy_data));
  profiler_start();
  status_t result = flash_impl_erase_sector_begin(FLASH_REGION_FIRMWARE_SLOT_1_BEGIN);
  flash_impl_get_erase_status();
  if (result == S_SUCCESS) {
    while (flash_impl_get_erase_status() == E_BUSY) {
      was_busy = true;
    }
  }
  profiler_stop();
  uint32_t duration = profiler_get_total_duration(true);
  prompt_send_response_fmt(buf, buf_size, "Erase took: %" PRIu32, duration);

  // Fash erases take at least ~100ms, if we're too short we probably didn't erase
  const uint32_t min_erase_time = 10000;
  if (result != S_SUCCESS) {
    ++failures;
    prompt_send_response_fmt(buf, buf_size, "FAILURE: erase did not report success %" PRIi32,
                             result);
  } else if (was_busy == false) {
    ++failures;
    prompt_send_response("FAILURE: Flash never became busy, but we should be busy for 300ms.");
    prompt_send_response("FAILURE: Flash probably never did an erase.");
  } else if (duration < min_erase_time) {
    ++failures;
    prompt_send_response("FAILURE: Flash erase completed way to quickly to have succeeded.");
  } else {
    ++passes;
  }

  // must call blank_check_poll by hand, otherwise we'll get the dma version
  profiler_start();
  bool is_blank =
      qspi_flash_blank_check(QSPI_FLASH, FLASH_REGION_FIRMWARE_SLOT_1_BEGIN, SUBSECTOR_SIZE_BYTES);
  profiler_stop();

  uint32_t blank = profiler_get_total_duration(true);
  prompt_send_response_fmt(buf, buf_size, "Sector blank check via read took: %" PRIu32, blank);
  if (is_blank != S_TRUE) {
    ++failures;
    prompt_send_response("FAILURE: sector not blank!?!");
  } else {
    ++passes;
  }

  profiler_start();
  is_blank = flash_impl_blank_check_subsector(FLASH_REGION_FIRMWARE_SLOT_1_BEGIN);
  profiler_stop();

  blank = profiler_get_total_duration(true);
  prompt_send_response_fmt(buf, buf_size, "Subsector blank check via read took: %" PRIu32, blank);
  if (is_blank != S_TRUE) {
    ++failures;
    prompt_send_response("FAILURE: sector not blank!?!");
  } else {
    ++passes;
  }

  if (failures == 0) {
    prompt_send_response_fmt(buf, buf_size, "SUCCESS: run %d tests and all passeed", passes);
  } else {
    prompt_send_response_fmt(buf, buf_size, "FAILED: run %d tests and %d failed", passes + failures,
                             failures);
  }
}

#endif

#if RECOVERY_FW
#include "console/prompt.h"
#include "drivers/flash.h"

#define SIGNAL_TEST_MAGIC_PATTERN (0xA5)
#define TEST_BUFFER_SIZE (1024)
static uint8_t s_test_buffer[TEST_BUFFER_SIZE];
static const uint32_t s_test_addr = FLASH_REGION_FIRMWARE_SLOT_1_END - SECTOR_SIZE_BYTES;
static bool s_signal_test_initialized;

static void prv_get_fast_read_params(QSPIFlash *dev, uint8_t *instruction, uint8_t *dummy_cycles,
                                     bool *is_ddr) {
  if (dev->state->fast_read_ddr_enabled) {
    *instruction = dev->state->part->instructions.fast_read_ddr;
    *dummy_cycles = dev->state->part->dummy_cycles.fast_read_ddr;
    *is_ddr = true;
  } else {
    *instruction = dev->state->part->instructions.fast_read;
    *dummy_cycles = dev->state->part->dummy_cycles.fast_read;
    *is_ddr = false;
  }
}

static void prv_set_fast_read_ddr_enabled(QSPIFlash *dev, bool enabled) {
  // If we're supposed to use DDR for fast read, make sure the part can support it
  PBL_ASSERTN(!enabled || dev->state->part->supports_fast_read_ddr);
  dev->state->fast_read_ddr_enabled = enabled;
}

void command_flash_signal_test_init(void) {
  // just test one sector, which is probably less than the size of the region

  // erase the sector
  flash_erase_sector_blocking(s_test_addr);

  // set the contents of the sector such that we will end up reading alternating 1s and 0s
  memset(s_test_buffer, SIGNAL_TEST_MAGIC_PATTERN, sizeof(s_test_buffer));
  flash_write_bytes(s_test_buffer, s_test_addr, sizeof(s_test_buffer));

  QSPIFlash *dev = QSPI_FLASH;
  // Ensure DDR is disabled for write check
  prv_set_fast_read_ddr_enabled(dev, false);
  uint8_t instruction;
  uint8_t dummy_cycles;
  bool is_ddr;
  prv_get_fast_read_params(dev, &instruction, &dummy_cycles, &is_ddr);
  PBL_ASSERTN(!is_ddr);

  qspi_indirect_read(dev->qspi, instruction, s_test_addr, dummy_cycles, s_test_buffer,
                     sizeof(s_test_buffer), is_ddr);

  prv_set_fast_read_ddr_enabled(dev, dev->default_fast_read_ddr_enabled);

  bool success = true;
  for (uint32_t i = 0; i < sizeof(s_test_buffer); ++i) {
    if (s_test_buffer[i] != SIGNAL_TEST_MAGIC_PATTERN) {
      success = false;
      break;
    }
  }

  if (success) {
    prompt_send_response("Done!");
    s_signal_test_initialized = true;
  } else {
    prompt_send_response("ERROR: Data read (SDR mode) did not match data written!");
  }
}

void command_flash_signal_test_run(void) {
  if (!s_signal_test_initialized) {
    prompt_send_response("ERROR: 'flash signal test init' must be run first!");
    return;
  }

  QSPIFlash *dev = QSPI_FLASH;

  // set to DDR
  prv_set_fast_read_ddr_enabled(dev, true);

  // issue the read
  uint8_t instruction;
  uint8_t dummy_cycles;
  bool is_ddr;
  prv_get_fast_read_params(dev, &instruction, &dummy_cycles, &is_ddr);
  PBL_ASSERTN(is_ddr);
  qspi_indirect_read(dev->qspi, instruction, s_test_addr, dummy_cycles, s_test_buffer,
                     sizeof(s_test_buffer), is_ddr);

  bool success = true;
  for (uint32_t i = 0; i < sizeof(s_test_buffer); ++i) {
    if (s_test_buffer[i] != SIGNAL_TEST_MAGIC_PATTERN) {
      success = false;
      break;
    }
  }

  // set back to default mode
  prv_set_fast_read_ddr_enabled(dev, dev->default_fast_read_ddr_enabled);

  if (success) {
    prompt_send_response("Ok");
  } else {
    prompt_send_response("ERROR: Read value didn't match!");
  }
}
#endif
