/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/flash/sf32lb52_mpi.h>

#include <pbl/drivers/flash/nor_part.h>

#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include <pbl/logging/logging.h>
#include "kernel/pbl_malloc.h"
#include "pbl/kernel/irq.h"
#include "pbl/mcu/cache.h"
#include "pbl/util/misc.h"
#include "system/passert.h"

#include <bf0_hal.h>

PBL_LOG_MODULE_DECLARE(driver_flash, CONFIG_DRIVER_FLASH_LOG_LEVEL);

// Bits 15-12 of a security register address are its one-based index.
#define SEC_ADDR_TO_IDX(addr) (((addr) >> 12U) - 1U)

#define MPI_INSTANCE_(n) FLASH##n
#define MPI_INSTANCE(n) MPI_INSTANCE_(n)
#define MPI_BASE_ADDR_(n) FLASH##n##_BASE_ADDR
#define MPI_BASE_ADDR(n) MPI_BASE_ADDR_(n)
#define DMA_CHANNEL_(n) DMA1_Channel##n
#define DMA_CHANNEL(n) DMA_CHANNEL_(n)
#define DMA_IRQN_(n) DMAC1_CH##n##_IRQn
#define DMA_IRQN(n) DMA_IRQN_(n)
#define DMA_REQUEST_(n) DMA_REQUEST_##n
#define DMA_REQUEST(n) DMA_REQUEST_(n)

struct pbl_flash_sf32lb52_mpi_state {
  struct pbl_flash_device_state flash;
  QSPI_FLASH_CTX_T ctx;
  DMA_HandleTypeDef hdma;
  qspi_configure_t cfg;
  struct dma_config dma;
  bool initialized;
};

struct pbl_flash_sf32lb52_mpi {
  struct pbl_flash_device dev;
  const struct pbl_flash_nor_part *part;
  uint16_t clk_div;
};

static inline const struct pbl_flash_sf32lb52_mpi *prv_cfg(const struct pbl_flash_device *dev) {
  return container_of(dev, const struct pbl_flash_sf32lb52_mpi, dev);
}

static inline struct pbl_flash_sf32lb52_mpi_state *prv_state(const struct pbl_flash_device *dev) {
  return container_of(dev->state, struct pbl_flash_sf32lb52_mpi_state, flash);
}

static inline FLASH_HandleTypeDef *prv_handle(const struct pbl_flash_device *dev) {
  return &prv_state(dev)->ctx.handle;
}

static int prv_init(const struct pbl_flash_device *dev) {
  const struct pbl_flash_sf32lb52_mpi *cfg = prv_cfg(dev);
  struct pbl_flash_sf32lb52_mpi_state *state = prv_state(dev);
  HAL_StatusTypeDef res;

  if (state->initialized) {
    // DMA completion relies on interrupts, which are unavailable in a coredump
    state->ctx.handle.dma = dev->state->coredump ? NULL : &state->hdma;
    return 0;
  }

  state->ctx.dual_mode = 1;
  res = HAL_FLASH_Init(&state->ctx, &state->cfg, &state->hdma, &state->dma, cfg->clk_div);
  PBL_ASSERT(res == HAL_OK, "HAL_FLASH_Init failed");

  if (state->ctx.dev_id != cfg->part->id) {
    PBL_LOG_ERR("Flash is not %s (id: 0x%" PRIx32 ")", cfg->part->name, state->ctx.dev_id);
  }

  state->initialized = true;

  return 0;
}

static int prv_read(const struct pbl_flash_device *dev, uint32_t addr, void *buf, size_t len) {
  memcpy(buf, (const void *)addr, len);
  return 0;
}

static int prv_write(const struct pbl_flash_device *dev, uint32_t addr, const void *buf,
                     size_t len) {
  FLASH_HandleTypeDef *hflash = prv_handle(dev);
  uint32_t page_size = prv_cfg(dev)->part->geometry.page_size;
  uint8_t *local_buf = NULL;
  const uint8_t *src = buf;
  int ret = 0;

  if (addr < hflash->base || addr > hflash->base + hflash->size) {
    return -EINVAL;
  }

  // The source must be reachable by the DMA and not live in the flash itself
  if (IS_SAME_FLASH_ADDR(src, addr) || IS_SPI_NONDMA_RAM_ADDR(src) ||
      IS_DMA_ACCROSS_1M_BOUNDARY((uint32_t)src, len)) {
    local_buf = kernel_malloc_check(len);
    memcpy(local_buf, src, len);
    src = local_buf;
  }

  // The DMA bypasses the D-cache
  uintptr_t flush_addr = (uintptr_t)src;
  size_t flush_size = len;
  dcache_align(&flush_addr, &flush_size);
  dcache_flush((const void *)flush_addr, flush_size);

  uint32_t offset = addr - hflash->base;
  size_t remain = len;

  while (remain > 0) {
    size_t chunk = page_size - (offset % page_size);
    if (chunk > remain) {
      chunk = remain;
    }

    pbl_irq_lock();
    int res = HAL_QSPIEX_WRITE_PAGE(hflash, offset, (uint8_t *)src, chunk);
    pbl_irq_unlock();
    if ((size_t)res != chunk) {
      ret = -EIO;
      break;
    }

    offset += chunk;
    src += chunk;
    remain -= chunk;
  }

  SCB_InvalidateDCache_by_Addr((void *)addr, len);
  SCB_InvalidateICache_by_Addr((void *)addr, len);

  if (local_buf != NULL) {
    kernel_free(local_buf);
  }

  return ret;
}

static int prv_erase_begin(const struct pbl_flash_device *dev, uint32_t addr, size_t size) {
  FLASH_HandleTypeDef *hflash = prv_handle(dev);
  const struct pbl_flash_nor_part *part = prv_cfg(dev)->part;
  int res;

  if (addr < hflash->base || addr > hflash->base + hflash->size) {
    return -EINVAL;
  }

  uint32_t offset = addr - hflash->base;

  pbl_irq_lock();
  if (size == part->geometry.sector_size) {
    res = HAL_QSPIEX_BLK64_ERASE(hflash, offset);
  } else if (size == part->geometry.subsector_size) {
    res = HAL_QSPIEX_SECT_ERASE(hflash, offset);
  } else {
    res = -1;
  }
  pbl_irq_unlock();

  SCB_InvalidateDCache_by_Addr((void *)addr, size);
  SCB_InvalidateICache_by_Addr((void *)addr, size);

  return (res == 0) ? 0 : -EIO;
}

static void prv_power_down(const struct pbl_flash_device *dev) {
  HAL_FLASH_NOP_CMD(prv_handle(dev));
}

void pbl_flash_sf32lb52_mpi_dpd_enter(const struct pbl_flash_device *dev) {
  FLASH_HandleTypeDef *hflash = prv_handle(dev);

  HAL_FLASH_NOP_CMD(hflash);
  HAL_FLASH_DEEP_PWRDOWN(hflash);
  HAL_Delay_us(prv_cfg(dev)->part->standby_to_low_power_latency_us);
}

void pbl_flash_sf32lb52_mpi_dpd_exit(const struct pbl_flash_device *dev) {
  HAL_FLASH_RELEASE_DPD(prv_handle(dev));
  HAL_Delay_us(prv_cfg(dev)->part->low_power_to_standby_latency_us);
}

static int prv_sec_reg_check(const struct pbl_flash_device *dev, uint32_t addr) {
  const struct pbl_flash_sec_regs *regs = dev->sec_regs;

  if (regs == NULL) {
    return -ENOTSUP;
  }

  for (uint8_t i = 0U; i < regs->count; ++i) {
    if (addr >= regs->addrs[i] && addr < regs->addrs[i] + regs->size) {
      return 0;
    }
  }

  return -EINVAL;
}

static int prv_sec_reg_read(const struct pbl_flash_device *dev, uint32_t addr, uint8_t *val) {
  int ret = prv_sec_reg_check(dev, addr);
  if (ret != 0) {
    return ret;
  }

  // Reads must be word sized and aligned
  uint8_t values[4] = {0};
  uint32_t offset = addr % 4;

  pbl_irq_lock();
  int res = HAL_QSPI_READ_OTP(prv_handle(dev), addr - offset, values, 4);
  pbl_irq_unlock();
  if (res != 4) {
    return -EIO;
  }

  *val = values[offset];

  return 0;
}

static int prv_sec_reg_write(const struct pbl_flash_device *dev, uint32_t addr, uint8_t val) {
  int ret = prv_sec_reg_check(dev, addr);
  if (ret != 0) {
    return ret;
  }

  uintptr_t flush_addr = (uintptr_t)&val;
  size_t flush_size = sizeof(val);
  dcache_align(&flush_addr, &flush_size);
  dcache_flush((const void *)flush_addr, flush_size);

  pbl_irq_lock();
  int res = HAL_QSPI_WRITE_OTP(prv_handle(dev), addr, &val, 1);
  pbl_irq_unlock();

  return (res == 1) ? 0 : -EIO;
}

static int prv_sec_reg_erase(const struct pbl_flash_device *dev, uint32_t addr) {
  int ret = prv_sec_reg_check(dev, addr);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_lock();
  int res = HAL_QSPI_ERASE_OTP(prv_handle(dev), addr);
  pbl_irq_unlock();

  return (res == 0) ? 0 : -EIO;
}

static int prv_sec_reg_is_locked(const struct pbl_flash_device *dev, uint32_t addr, bool *locked) {
  int ret = prv_sec_reg_check(dev, addr);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_lock();
  uint8_t lb = HAL_QSPI_GET_OTP_LB(prv_handle(dev));
  pbl_irq_unlock();
  if (lb == 0xff) {
    return -EIO;
  }

  *locked = (lb & (1U << SEC_ADDR_TO_IDX(addr))) != 0U;

  return 0;
}

#ifdef CONFIG_RECOVERY_FW
static int prv_sec_reg_lock(const struct pbl_flash_device *dev, uint32_t addr) {
  int ret = prv_sec_reg_check(dev, addr);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_lock();
  int res = HAL_QSPI_LOCK_OTP(prv_handle(dev), addr);
  pbl_irq_unlock();

  return (res == 0) ? 0 : -EIO;
}
#endif

static const struct pbl_flash_ops s_ops = {
    .init = prv_init,
    .read = prv_read,
    .write = prv_write,
    .erase_begin = prv_erase_begin,
    .power_down = prv_power_down,
    .sec_reg_read = prv_sec_reg_read,
    .sec_reg_write = prv_sec_reg_write,
    .sec_reg_erase = prv_sec_reg_erase,
    .sec_reg_is_locked = prv_sec_reg_is_locked,
#ifdef CONFIG_RECOVERY_FW
    .sec_reg_lock = prv_sec_reg_lock,
#endif
};

static struct pbl_flash_sf32lb52_mpi_state s_flash_state = {
    .cfg =
        {
            .Instance = MPI_INSTANCE(CONFIG_FLASH_SF32LB52_MPI_INSTANCE),
            .line = HAL_FLASH_QMODE,
            .base = MPI_BASE_ADDR(CONFIG_FLASH_SF32LB52_MPI_INSTANCE),
            .msize = CONFIG_FLASH_SF32LB52_MPI_MSIZE,
            .SpiMode = SPI_MODE_NOR,
        },
    .dma =
        {
            .Instance = DMA_CHANNEL(CONFIG_FLASH_SF32LB52_MPI_DMA_CHANNEL),
            .dma_irq = DMA_IRQN(CONFIG_FLASH_SF32LB52_MPI_DMA_CHANNEL),
            .request = DMA_REQUEST(CONFIG_FLASH_SF32LB52_MPI_DMA_REQUEST),
        },
};
static const struct pbl_flash_sf32lb52_mpi s_flash = {
    .dev =
        {
            .state = &s_flash_state.flash,
            .ops = &s_ops,
            .base = MPI_BASE_ADDR(CONFIG_FLASH_SF32LB52_MPI_INSTANCE),
            .geometry = &PBL_FLASH_NOR_PART.geometry,
            .sec_regs = &PBL_FLASH_NOR_PART.sec_regs,
        },
    .part = &PBL_FLASH_NOR_PART,
    .clk_div = CONFIG_FLASH_SF32LB52_MPI_CLK_DIV,
};
const struct pbl_flash_device *const FLASH = &s_flash.dev;
