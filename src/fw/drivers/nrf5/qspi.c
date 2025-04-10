#include "drivers/qspi.h"
#include "drivers/qspi_definitions.h"

#include "drivers/flash/qspi_flash.h"
#include "drivers/flash/qspi_flash_definitions.h"

#include "board/board.h"
#include "drivers/dma.h"
#include "drivers/flash/flash_impl.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "flash_region/flash_region.h"
#include "kernel/util/delay.h"
#include "kernel/util/sleep.h"
#include "kernel/util/stop.h"
#include "mcu/cache.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/math.h"

#define NRF5_COMPATIBLE
#include <mcu.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include <nrfx_qspi.h>

/* nRF5's QSPI controller is different enough from STM32's that we
 * reimplement qspi_flash.c, not stm32/qspi.c.  */

static void prv_use(QSPIPort *dev) {
  dev->state->use_count++;
}

static void prv_release(QSPIPort *dev) {
  PBL_ASSERTN(dev->state->use_count > 0);
}

status_t flash_impl_set_nvram_erase_status(bool is_subsector,
                                           FlashAddress addr) {
  return S_SUCCESS;
}

status_t flash_impl_clear_nvram_erase_status(void) {
  return S_SUCCESS;
}

status_t flash_impl_get_nvram_erase_status(bool *is_subsector,
                                           FlashAddress *addr) {
  // XXX
  return S_FALSE;
}

#define FLASH_RESET_WORD_VALUE (0xffffffff)

static void prv_read_register(QSPIPort *dev, uint8_t instruction, uint8_t *data, uint32_t length) {
  nrf_qspi_cinstr_conf_t instr = NRFX_QSPI_DEFAULT_CINSTR(instruction, length + 1);
  instr.io2_level = 1;
  instr.io3_level = 1;
  nrfx_err_t err = nrfx_qspi_cinstr_xfer(&instr, NULL, data);
  PBL_ASSERTN(err == NRFX_SUCCESS);
}

static void prv_write_cmd_no_addr(QSPIPort *dev, uint8_t cmd) {
  nrf_qspi_cinstr_conf_t instr = NRFX_QSPI_DEFAULT_CINSTR(cmd, 1);
  instr.io2_level = 1;
  instr.io3_level = 1;
  nrfx_err_t err = nrfx_qspi_cinstr_xfer(&instr, NULL, NULL);
  PBL_ASSERTN(err == NRFX_SUCCESS);
}

static bool prv_poll_bit(QSPIPort *dev, uint8_t instruction, uint8_t bit_mask, bool should_be_set,
                   uint32_t timeout_us) {
  uint32_t loops = 0;
  uint8_t val;
  while (1) {
    prv_read_register(dev, instruction, &val, 1);
    if ((!!(val & bit_mask)) == should_be_set)
      break;
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
    PBL_LOG(LOG_LEVEL_ERROR, "Flash isn't expected %s (whoami: 0x%"PRIx32")",
            dev->state->part->name, read_whoami);
    return false;
  }
}

bool qspi_flash_check_whoami(QSPIFlash *dev) {
  prv_use(dev->qspi);
  bool result = prv_check_whoami(dev);
  prv_release(dev->qspi);
  return result;
}

bool qspi_flash_is_in_coredump_mode(QSPIFlash *dev) {
  return dev->state->coredump_mode;
}

static void _flash_handler(nrfx_qspi_evt_t event, void *ctx) {
  QSPIFlash *dev = (QSPIFlash *)ctx;
  PBL_ASSERTN(event == NRFX_QSPI_EVENT_DONE);
  dev->qspi->state->waiting = 0;
  if (!dev->state->coredump_mode) {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(dev->qspi->state->dma_semaphore, &woken);
    portYIELD_FROM_ISR(woken);
  }
}

void qspi_flash_init(QSPIFlash *dev, QSPIFlashPart *part, bool coredump_mode) {
  static int was_init = 0;

  dev->state->part = part;
  dev->state->coredump_mode = coredump_mode;

  // Init the DMA semaphore, used for I/O ops
  if (!coredump_mode)
    dev->qspi->state->dma_semaphore = xSemaphoreCreateBinary();
  dev->qspi->state->waiting = 0;
  
  nrfx_qspi_config_t config = NRFX_QSPI_DEFAULT_CONFIG(
    dev->qspi->clk_gpio, dev->qspi->cs_gpio, dev->qspi->data_gpio[0],
    dev->qspi->data_gpio[1], dev->qspi->data_gpio[2], dev->qspi->data_gpio[3]);
  config.phy_if.sck_freq = NRF_QSPI_FREQ_32MDIV1;

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

  config.prot_if.addrmode = NRF_QSPI_ADDRMODE_24BIT;

  nrfx_err_t err;
  if (was_init) {
    nrfx_qspi_uninit();
  }
  err = nrfx_qspi_init(&config, coredump_mode ? NULL : _flash_handler, (void *)dev);
  PBL_ASSERTN(err == NRFX_SUCCESS);
  was_init = 1;
  
  prv_use(dev->qspi);

  if (dev->reset_gpio.gpio) {
    WTF;
  }

  // we do not enter QPI mode on nRF5
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

  prv_release(dev->qspi);
}

status_t qspi_flash_is_erase_complete(QSPIFlash *dev) {
  prv_use(dev->qspi);

  uint8_t status_reg;
  uint8_t flag_status_reg;
  prv_read_register(dev->qspi, dev->state->part->instructions.rdsr1, &status_reg, 1);
  prv_read_register(dev->qspi, dev->state->part->instructions.rdsr2, &flag_status_reg, 1);

  prv_release(dev->qspi);

  if (status_reg & dev->state->part->status_bit_masks.busy) {
    return E_BUSY;
  } else if (flag_status_reg & dev->state->part->flag_status_bit_masks.erase_suspend) {
    return E_AGAIN;
  } else {
    return S_SUCCESS;
  }
}

status_t qspi_flash_erase_begin(QSPIFlash *dev, uint32_t addr, bool is_subsector) {
  prv_use(dev->qspi);
  prv_write_enable(dev);

  dev->qspi->state->waiting = 1;
  
  nrfx_err_t err = nrfx_qspi_erase(is_subsector ? NRF_QSPI_ERASE_LEN_4KB : NRF_QSPI_ERASE_LEN_64KB, addr);
  PBL_ASSERTN(err == NRFX_SUCCESS);

  if (!dev->state->coredump_mode) {
    xSemaphoreTake(dev->qspi->state->dma_semaphore, portMAX_DELAY);
    PBL_ASSERTN(!dev->qspi->state->waiting);
  } else {
    dev->qspi->state->waiting = 0;
  }

  // wait for busy to be set indicating the erase has started
  const uint32_t busy_timeout_us = 500;
  const bool result = prv_poll_bit(dev->qspi, dev->state->part->instructions.rdsr1,
                                    dev->state->part->status_bit_masks.busy, true /* set */,
                                    busy_timeout_us);
  prv_release(dev->qspi);

  return result ? S_SUCCESS : E_ERROR;
}

status_t qspi_flash_erase_suspend(QSPIFlash *dev, uint32_t addr) {
  prv_use(dev->qspi);

  uint8_t status_reg;
  prv_read_register(dev->qspi, dev->state->part->instructions.rdsr1, &status_reg, 1);
  if (!(status_reg & dev->state->part->status_bit_masks.busy)) {
    // no erase in progress
    prv_release(dev->qspi);
    return S_NO_ACTION_REQUIRED;
  }

  prv_write_cmd_no_addr(dev->qspi, dev->state->part->instructions.erase_suspend);

  prv_release(dev->qspi);

  if (dev->state->part->suspend_to_read_latency_us) {
    delay_us(dev->state->part->suspend_to_read_latency_us);
  }

  return S_SUCCESS;
}

void qspi_flash_erase_resume(QSPIFlash *dev, uint32_t addr) {
  prv_use(dev->qspi);
  prv_write_cmd_no_addr(dev->qspi, dev->state->part->instructions.erase_resume);
  // wait for the erase_suspend bit to be cleared
  prv_poll_bit(dev->qspi, dev->state->part->instructions.rdsr2,
                dev->state->part->flag_status_bit_masks.erase_suspend, false /* !set */,
                QSPI_NO_TIMEOUT);
  prv_release(dev->qspi);
}

static void prv_qspi_flash_read_blocking(QSPIFlash *dev, uint32_t addr, void *buffer, uint32_t length) {
  PBL_ASSERTN(!dev->qspi->state->waiting);
  dev->qspi->state->waiting = 1;
  
  nrfx_err_t err = nrfx_qspi_read(buffer, length, addr);
  PBL_ASSERTN(err == NRFX_SUCCESS);

  // XXX: later: use mmap?
  if (!dev->state->coredump_mode) {
    xSemaphoreTake(dev->qspi->state->dma_semaphore, portMAX_DELAY);
    PBL_ASSERTN(!dev->qspi->state->waiting);
  } else {
    dev->qspi->state->waiting = 0;
  }
}

void qspi_flash_read_blocking(QSPIFlash *dev, uint32_t addr, void *buffer, uint32_t length) {
  prv_use(dev->qspi);

  uint32_t buf_p = (uint32_t) buffer;
  if (buf_p & 3) {
    /* argh ... */
    uint32_t align_len = 4 - (buf_p & 3);
    uint32_t tbuf = 0xAAAAAAAA;
    if (align_len > length) {
      align_len = length;
    }

    prv_qspi_flash_read_blocking(dev, addr, &tbuf, 4);
    memcpy(buffer, &tbuf, align_len);
    length -= align_len;
    addr += align_len;
    buffer = ((uint8_t *)buffer) + align_len;
  }
  uint32_t tail_len = length & 3;
  length -= tail_len;
  if (length) {
    prv_qspi_flash_read_blocking(dev, addr, buffer, length);
    addr += length;
    buffer = ((uint8_t *)buffer) + length;
  }
  if (tail_len) {
    /* argh... */
    uint32_t tbuf = 0xAAAAAAAA;
    prv_qspi_flash_read_blocking(dev, addr, &tbuf, 4);
    memcpy(buffer, &tbuf, tail_len);
  }

  prv_release(dev->qspi);
}

static void prv_write_page_begin(QSPIFlash *dev, const void *buffer, uint32_t addr,
                                uint32_t length) {
  PBL_ASSERTN(((uint32_t)buffer & 3) == 0);
  PBL_ASSERTN(((uint32_t)buffer & 0xF0000000) == 0x20000000);
  PBL_ASSERTN(length > 0);

  prv_write_enable(dev);

  PBL_ASSERTN(!dev->qspi->state->waiting);
  dev->qspi->state->waiting = 1;

  nrfx_err_t err = nrfx_qspi_write(buffer, length, addr); 
  PBL_ASSERTN(err == NRFX_SUCCESS);

  if (!dev->state->coredump_mode) {
    xSemaphoreTake(dev->qspi->state->dma_semaphore, portMAX_DELAY);
    PBL_ASSERTN(!dev->qspi->state->waiting);
  } else {
    dev->qspi->state->waiting = 0;
  }

  prv_poll_bit(dev->qspi, dev->state->part->instructions.rdsr1,
                dev->state->part->status_bit_masks.busy, false /* !set */, QSPI_NO_TIMEOUT);
}

static uint8_t s_qspi_ram_buffer[32];
int qspi_flash_write_page_begin(QSPIFlash *dev, const void *buffer, uint32_t addr,
                                uint32_t length) {
  const uint32_t offset_in_page = addr % PAGE_SIZE_BYTES;
  uint32_t bytes_in_page = MIN(PAGE_SIZE_BYTES - offset_in_page, length);
  
  if (((uintptr_t) buffer & 0xF0000000) != 0x20000000) {
    /* we cannot DMA from non-RAM, so bounce through a RAM buffer if we have
     * to; this is not performant but it is ok because any time we are
     * writing to QSPI flash from internal flash, it is during
     * initialization and should be pretty infrequent 
     */
    bytes_in_page = MIN(bytes_in_page, sizeof(s_qspi_ram_buffer));
    memcpy(s_qspi_ram_buffer, buffer, bytes_in_page);
    buffer = s_qspi_ram_buffer;
    /* s_qspi_ram_buffer does not get overwritten because nobody will call
     * us again until qspi_flash_get_write_status completes
     */
  }
  
  length = bytes_in_page;
  
  prv_use(dev->qspi);

  prv_write_enable(dev);

  uint32_t buf_p = (uint32_t) buffer;
  if (buf_p & 3) {
    /* argh ... */
    uint32_t align_len = 4 - (buf_p & 3);
    uint32_t tbuf = 0xFFFFFFFF;
    if (align_len > length) {
      align_len = length;
    }

    memcpy(&tbuf, buffer, align_len);
    prv_write_page_begin(dev, &tbuf, addr, 4);
    length -= align_len;
    addr += align_len;
    buffer = ((uint8_t *)buffer) + align_len;

    while (nrfx_qspi_mem_busy_check() != NRFX_SUCCESS)
      delay_us(10);
  }

  uint32_t tail_len = length & 3;
  length -= tail_len;
  if (length) {
    prv_write_page_begin(dev, buffer, addr, length);
    addr += length;
    buffer = ((uint8_t *)buffer) + length;
  }

  if (tail_len) {
    /* argh... */
    while (nrfx_qspi_mem_busy_check() != NRFX_SUCCESS)
      delay_us(10);

    uint32_t tbuf = 0xFFFFFFFF;
    memcpy(&tbuf, buffer, tail_len);
    prv_write_page_begin(dev, &tbuf, addr, 4);
  }

  prv_release(dev->qspi);

  return bytes_in_page;
}

status_t qspi_flash_get_write_status(QSPIFlash *dev) {
  prv_use(dev->qspi);
  uint8_t status_reg;
  prv_read_register(dev->qspi, dev->state->part->instructions.rdsr1, &status_reg, 1);
  prv_release(dev->qspi);
  return (status_reg & dev->state->part->status_bit_masks.busy) ? E_BUSY : S_SUCCESS;
}

void qspi_flash_set_lower_power_mode(QSPIFlash *dev, bool active) {
  prv_use(dev->qspi);
  uint8_t instruction;
  uint32_t delay;
  if (active) {
    instruction = dev->state->part->instructions.enter_low_power;
    delay = dev->state->part->standby_to_low_power_latency_us;
  } else {
    instruction = dev->state->part->instructions.exit_low_power;
    delay = dev->state->part->low_power_to_standby_latency_us;
  }
  prv_write_cmd_no_addr(dev->qspi, instruction);
  prv_release(dev->qspi);
  if (delay) {
    delay_us(delay);
  }
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
  prv_use(dev->qspi);
  const bool result = prv_blank_check_poll(dev, addr, is_subsector);
  prv_release(dev->qspi);
  return result ? S_TRUE : S_FALSE;
}

void qspi_flash_ll_set_register_bits(QSPIFlash *dev, uint8_t read_instruction,
                                     uint8_t write_instruction, uint8_t value, uint8_t mask) {
  // make sure we're not trying to set any bits not within the mask
  PBL_ASSERTN((value & mask) == value);

  prv_use(dev->qspi);

  // first read the register
  uint8_t reg_value;
  prv_read_register(dev->qspi, read_instruction, &reg_value, 1);

  // set the desired bits
  reg_value = (reg_value & ~mask) | value;

  // enable writing and write the register value
  prv_write_cmd_no_addr(dev->qspi, dev->state->part->instructions.write_enable);
  qspi_indirect_write_no_addr(dev->qspi, write_instruction, &reg_value, 1);

  prv_release(dev->qspi);
}

status_t qspi_flash_write_protection_enable(QSPIFlash *dev) {
  return S_NO_ACTION_REQUIRED;
}

status_t qspi_flash_lock_sector(QSPIFlash *dev, uint32_t addr) {
  return S_SUCCESS;
}

status_t qspi_flash_unlock_all(QSPIFlash *dev) {
  return S_SUCCESS;
}

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
  prompt_send_response_fmt(buf, buf_size, "Size: %d UNIMPL: MMP: %"PRIu32,
                           size, mmap_time);


  return success;
}

struct FlashReadTestValues {
  int size;
  int offset;
};

const struct FlashReadTestValues FLASH_READ_TEST_TABLE[] = {
  { .size = 1024, .offset = 0 },
  { .size = 1025, .offset = 0 },
  { .size = 1026, .offset = 0 },
  { .size = 1027, .offset = 0 },
  { .size = 1024, .offset = 1 },
  { .size = 1025, .offset = 2 },
  { .size = 1026, .offset = 3 },
  { .size = 4,    .offset = 0 },
  { .size = 20,   .offset = 0 },
  { .size = 60,   .offset = 0 },
  { .size = 127,  .offset = 0 },
  { .size = 128,  .offset = 0 },
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
  prv_use(dev->qspi);

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

  prv_release(dev->qspi);

  bool was_busy = false;

  // write a few bytes to the sector we're going to erase so it's not empty
  uint8_t dummy_data = 0x55;
  flash_write_bytes(&dummy_data, FLASH_REGION_FIRMWARE_SCRATCH_BEGIN, sizeof(dummy_data));
  profiler_start();
  status_t result = flash_impl_erase_sector_begin(FLASH_REGION_FIRMWARE_SCRATCH_BEGIN);
  flash_impl_get_erase_status();
  if (result == S_SUCCESS) {
    while (flash_impl_get_erase_status() == E_BUSY) {
      was_busy = true;
    }
  }
  profiler_stop();
  uint32_t duration = profiler_get_total_duration(true);
  prompt_send_response_fmt(buf, buf_size, "Erase took: %"PRIu32, duration);

  // Fash erases take at least ~100ms, if we're too short we probably didn't erase
  const uint32_t min_erase_time = 10000;
  if (result != S_SUCCESS) {
    ++failures;
     prompt_send_response_fmt(buf, buf_size,
                              "FAILURE: erase did not report success %"PRIi32, result);
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
  prv_use(dev->qspi);
  bool is_blank = qspi_flash_blank_check(QSPI_FLASH, FLASH_REGION_FIRMWARE_SCRATCH_BEGIN,
                                       SUBSECTOR_SIZE_BYTES);
  prv_release(dev->qspi);
  profiler_stop();

  uint32_t blank = profiler_get_total_duration(true);
  prompt_send_response_fmt(buf, buf_size, "Sector blank check via read took: %"PRIu32, blank);
  if (is_blank != S_TRUE) {
    ++failures;
    prompt_send_response("FAILURE: sector not blank!?!");
  } else {
    ++passes;
  }

  profiler_start();
  is_blank = flash_impl_blank_check_subsector(FLASH_REGION_FIRMWARE_SCRATCH_BEGIN);
  profiler_stop();

  blank = profiler_get_total_duration(true);
  prompt_send_response_fmt(buf, buf_size, "Subsector blank check via read took: %"PRIu32,
                           blank);
  if (is_blank != S_TRUE) {
    ++failures;
    prompt_send_response("FAILURE: sector not blank!?!");
  } else {
    ++passes;
  }

  if (failures == 0) {
    prompt_send_response_fmt(buf, buf_size, "SUCCESS: run %d tests and all passeed", passes);
  }
  else {
    prompt_send_response_fmt(buf, buf_size, "FAILED: run %d tests and %d failed",
                             passes + failures,
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
static const uint32_t s_test_addr = FLASH_REGION_FIRMWARE_SCRATCH_END - SECTOR_SIZE_BYTES;
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

  prv_use(dev->qspi);
  qspi_indirect_read(dev->qspi, instruction, s_test_addr, dummy_cycles, s_test_buffer,
                     sizeof(s_test_buffer), is_ddr);

  prv_set_fast_read_ddr_enabled(dev, dev->default_fast_read_ddr_enabled);
  prv_release(dev->qspi);

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
  prv_use(dev->qspi);

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
  prv_release(dev->qspi);

  if (success) {
    prompt_send_response("Ok");
  } else {
    prompt_send_response("ERROR: Read value didn't match!");
  }
}
#endif
