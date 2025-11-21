/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "board/board.h"
#include "drivers/flash/flash_impl.h"
#include "drivers/flash/qspi_flash.h"
#include "drivers/flash/qspi_flash_part_definitions.h"
#include "flash_region/flash_region.h"
#include "kernel/pbl_malloc.h"
#include "system/passert.h"
#include "system/status_codes.h"
#include "util/math.h"

#define SEC_ADDR_TO_IDX(addr) (((addr) >> 12U) - 1U)

static bool prv_blank_check_poll(uint32_t addr, bool is_subsector) {
  const uint32_t size_bytes = is_subsector ? SUBSECTOR_SIZE_BYTES : SECTOR_SIZE_BYTES;
  const uint32_t BUF_SIZE_BYTES = 128;
  const uint32_t BUF_SIZE_WORDS = BUF_SIZE_BYTES / sizeof(uint32_t);
  const uint32_t FLASH_RESET_WORD_VALUE = 0xFFFFFFFF;
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

static int prv_erase_nor(QSPIFlash *dev, uint32_t addr, uint32_t size) {
  FLASH_HandleTypeDef *hflash;
  uint32_t taddr, remain;
  int res;

  hflash = &dev->qspi->state->ctx.handle;

  if ((addr < hflash->base) || (addr > (hflash->base + hflash->size))) {
    return -1;
  }

  taddr = addr - hflash->base;
  remain = size;

  if ((taddr & (SUBSECTOR_SIZE_BYTES - 1)) != 0) {
    return -1;
  }

  if ((remain & (SUBSECTOR_SIZE_BYTES - 1)) != 0) {
    return -1;
  }

  while (remain > 0) {
    portENTER_CRITICAL();
    res = HAL_QSPIEX_SECT_ERASE(hflash, taddr);
    portEXIT_CRITICAL();
    if (res != 0) {
      res = -1;
      goto end;
    }

    remain -= SUBSECTOR_SIZE_BYTES;
    taddr += SUBSECTOR_SIZE_BYTES;
  }

end:
  SCB_InvalidateDCache_by_Addr((void *)addr, size);
  SCB_InvalidateICache_by_Addr((void *)addr, size);

  return res;
}

static int prv_write_nor(QSPIFlash *dev, uint32_t addr, uint8_t *buf, uint32_t size) {
  FLASH_HandleTypeDef *hflash;
  uint32_t taddr, start, remain, fill;
  uint8_t *tbuf;
  int res;
  uint8_t *local_buf = NULL;

  hflash = &dev->qspi->state->ctx.handle;

  if ((addr < hflash->base) || (addr > (hflash->base + hflash->size))) {
    return -1;
  }

  if (IS_SAME_FLASH_ADDR(buf, addr) || IS_SPI_NONDMA_RAM_ADDR(buf) ||
      (IS_DMA_ACCROSS_1M_BOUNDARY((uint32_t)buf, size))) {
    local_buf = kernel_malloc_check(size);
    memcpy(local_buf, buf, size);
    tbuf = local_buf;
  } else {
    tbuf = buf;
  }
  
  taddr = addr - hflash->base;
  remain = size;

  start = taddr & (PAGE_SIZE_BYTES - 1);
  // start address not page aligned
  if (start > 0) {
    fill = PAGE_SIZE_BYTES - start;
    if (fill > size) {
      fill = size;
    }

    portENTER_CRITICAL();
    res = HAL_QSPIEX_WRITE_PAGE(hflash, taddr, tbuf, fill);
    portEXIT_CRITICAL();
    if ((uint32_t)res != fill) {
      res = -1;
      goto end;
    }

    taddr += fill;
    tbuf += fill;
    remain -= fill;
  }

  while (remain > 0) {
    fill = remain > PAGE_SIZE_BYTES ? PAGE_SIZE_BYTES : remain;
    portENTER_CRITICAL();
    res = HAL_QSPIEX_WRITE_PAGE(hflash, taddr, tbuf, fill);
    portEXIT_CRITICAL();
    if ((uint32_t)res != fill) {
      res = -1;
      goto end;
    }

    taddr += fill;
    tbuf += fill;
    remain -= fill;
  }

  res = size;

end:
  SCB_InvalidateDCache_by_Addr((void *)addr, size);
  SCB_InvalidateICache_by_Addr((void *)addr, size);

  if (local_buf) {
    kernel_free(local_buf);
  }

  return res;
}

bool qspi_flash_check_whoami(QSPIFlash *dev) { 
  QSPI_FLASH_CTX_T *ctx = &dev->qspi->state->ctx;
  uint32_t id = ctx->dev_id;

  if (id == dev->state->part->qspi_id_value) {
    PBL_LOG(LOG_LEVEL_INFO, "Flash is %s", dev->state->part->name);
    return true;
  } else {
    PBL_LOG(LOG_LEVEL_ERROR, "Flash isn't expected %s (whoami: 0x%" PRIx32 ")",
            dev->state->part->name, id);
    return false;
  }
}

status_t qspi_flash_write_protection_enable(QSPIFlash *dev) { return S_NO_ACTION_REQUIRED; }

status_t qspi_flash_lock_sector(QSPIFlash *dev, uint32_t addr) { return S_SUCCESS; }

status_t qspi_flash_unlock_all(QSPIFlash *dev) { return S_SUCCESS; }

void qspi_flash_init(QSPIFlash *dev, QSPIFlashPart *part, bool coredump_mode) {
  HAL_StatusTypeDef res;

  if (dev->qspi->state->initialized) {
    if (coredump_mode) {
      dev->qspi->state->ctx.handle.dma = NULL;
    } else {
      dev->qspi->state->ctx.handle.dma = &dev->qspi->state->hdma;
    }

    return;
  }

  dev->state->part = part;
  dev->qspi->state->ctx.dual_mode = 1;

  res = HAL_FLASH_Init(&dev->qspi->state->ctx, (qspi_configure_t *)&dev->qspi->cfg,
                       &dev->qspi->state->hdma, (struct dma_config *)&dev->qspi->dma,
                       dev->qspi->clk_div);

  PBL_ASSERT(res == HAL_OK, "HAL_FLASH_Init failed");

  qspi_flash_check_whoami(dev);

  dev->qspi->state->initialized = true;
}

status_t qspi_flash_is_erase_complete(QSPIFlash *dev) {
  // A call to the HAL_QSPIEX_SECT_ERASE interface will always return success after the call
  return S_SUCCESS;
}

status_t qspi_flash_erase_begin(QSPIFlash *dev, uint32_t addr, bool is_subsector) {
  if (prv_erase_nor(dev, addr, is_subsector ? SUBSECTOR_SIZE_BYTES : SECTOR_SIZE_BYTES) != 0) {
    return E_ERROR;
  }

  return S_SUCCESS;
}

status_t qspi_flash_erase_suspend(QSPIFlash *dev, uint32_t addr) {
  // Everything will be blocked during the erase process, so nothing will happen if you call this
  // function.
  return S_SUCCESS;
}

void qspi_flash_erase_resume(QSPIFlash *dev, uint32_t addr) {
  // Everything will be blocked during the erase process, so nothing will happen if you call this
  // function.
}

void qspi_flash_read_blocking(QSPIFlash *dev, uint32_t addr, void *buffer, uint32_t length) {
  PBL_ASSERT(length > 0, "flash_impl_read_sync() called with 0 bytes to read");
  memcpy(buffer, (void *)(addr), length);
}

int qspi_flash_write_page_begin(QSPIFlash *dev, const void *buffer, uint32_t addr,
                                uint32_t length) {
  return prv_write_nor(dev, addr, (uint8_t *)buffer, length);
}

status_t qspi_flash_get_write_status(QSPIFlash *dev) {
  // It will be done in HAL_QSPIEX_WRITE_PAGE, so it must return success
  return S_SUCCESS;
}

void qspi_flash_set_lower_power_mode(QSPIFlash *dev, bool active) {}

status_t qspi_flash_blank_check(QSPIFlash *dev, uint32_t addr, bool is_subsector) {
  return prv_blank_check_poll(addr, is_subsector);
}

status_t flash_impl_set_nvram_erase_status(bool is_subsector, FlashAddress addr) {
  return S_SUCCESS;
}

status_t flash_impl_clear_nvram_erase_status(void) { return S_SUCCESS; }

status_t flash_impl_get_nvram_erase_status(bool *is_subsector, FlashAddress *addr) {
  return S_FALSE;
}

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
  FLASH_HandleTypeDef *hflash = &dev->qspi->state->ctx.handle;
  int res;

  res = prv_qspi_security_register_check(dev, addr);
  if (res != S_SUCCESS) {
    return res;
  }

  /* Security register read size should aligned with 4 bytes.  */
  uint8_t values[4] = {0};
  uint32_t offset = addr % 4;
  uint32_t base_addr = addr - offset;

  res = HAL_QSPI_READ_OTP(hflash, base_addr, values, 4);
  if (res != 4) {
    return E_ERROR;
  }

  *val = values[offset];

  return S_SUCCESS;
}

status_t qspi_flash_security_register_is_locked(QSPIFlash *dev, uint32_t addr, bool *locked) {
  FLASH_HandleTypeDef *hflash = &dev->qspi->state->ctx.handle;
  uint8_t opt_val = 0;
  int ret;

  /* OPT operation are synchronous, one match means all matched. */
  opt_val = HAL_QSPI_GET_OTP_LB(hflash, addr);
  if (opt_val == 0xff) {
    return E_ERROR;
  }

  /* Security registers address*/
  if ((opt_val & (1U << SEC_ADDR_TO_IDX(addr))) != 0U) {
    *locked = true;
  } else {
    *locked = false;
  }

  return S_SUCCESS;
}

status_t qspi_flash_erase_security_register(QSPIFlash *dev, uint32_t addr) {
  FLASH_HandleTypeDef *hflash = &dev->qspi->state->ctx.handle;
  int res;

  res = prv_qspi_security_register_check(dev, addr);
  if (res != S_SUCCESS) {
    return res;
  }

  res = HAL_QSPI_ERASE_OTP(hflash, addr);
  if (res != 0) {
    return E_ERROR;
  }

  return S_SUCCESS;
}

status_t qspi_flash_write_security_register(QSPIFlash *dev, uint32_t addr, uint8_t val) {
  FLASH_HandleTypeDef *hflash = &dev->qspi->state->ctx.handle;
  int res;

  res = prv_qspi_security_register_check(dev, addr);
  if (res != S_SUCCESS) {
    return res;
  }

  res = HAL_QSPI_WRITE_OTP(hflash, addr, &val, 1);
  if (res != 1) {
    return E_ERROR;
  }

  return S_SUCCESS;
}

const FlashSecurityRegisters *qspi_flash_security_registers_info(QSPIFlash *dev) {
  return &dev->state->part->sec_registers;
}

#ifdef RECOVERY_FW
status_t qspi_flash_lock_security_register(QSPIFlash *dev, uint32_t addr) {
  FLASH_HandleTypeDef *hflash = &dev->qspi->state->ctx.handle;
  int res;

  res = HAL_QSPI_LOCK_OTP(hflash, addr);
  if (res != 0) {
    return E_ERROR;
  }

  return S_SUCCESS;
}
#endif // RECOVERY_FW