/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/gpu/stm32_dma2d.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/types.h>
#include <pbl/soc/st/idle.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/stm32_dma2d.h>

// An aborted job stops at the end of a burst
#define DMA2D_ABORT_TIMEOUT_US 1000U

#define DMA2D_PL_MAX 16383U

// Past twice the size of the data cache, cleaning all of it costs less than line by line
#define DMA2D_CACHE_ALL_BYTES (32U * 1024U)

#define DMA2D_CR_IRQS (DMA2D_CR_TEIE | DMA2D_CR_TCIE | DMA2D_CR_CEIE)

struct prv_format {
  uint8_t bytes;
  uint8_t cm;
};

static const struct prv_format s_formats[PBL_GPU_FORMAT_COUNT] = {
  [PBL_GPU_FORMAT_RGB565] = {.bytes = 2U, .cm = DMA2D_CM_RGB565},
  [PBL_GPU_FORMAT_RGB888] = {.bytes = 3U, .cm = DMA2D_CM_RGB888},
  [PBL_GPU_FORMAT_ARGB8888] = {.bytes = 4U, .cm = DMA2D_CM_ARGB8888},
  [PBL_GPU_FORMAT_A8] = {.bytes = 1U, .cm = DMA2D_CM_A8},
};

static const struct pbl_gpu_stm32_dma2d *prv_dma2d(const struct pbl_gpu *gpu) {
  return PBL_CONTAINER_OF(gpu, const struct pbl_gpu_stm32_dma2d, gpu);
}

static inline uint32_t prv_read(const struct pbl_gpu_stm32_dma2d *dma2d, uint32_t offset) {
  return pbl_readl(dma2d->base + offset);
}

static inline void prv_write(const struct pbl_gpu_stm32_dma2d *dma2d, uint32_t offset,
                             uint32_t value) {
  pbl_writel(value, dma2d->base + offset);
}

static uint8_t *prv_pixel(const struct pbl_gpu_buffer *buf, int32_t x, int32_t y) {
  return (uint8_t *)buf->data + (size_t)y * buf->pitch + (size_t)x * s_formats[buf->format].bytes;
}

// Pixels wider than a byte are read and written aligned to their size
static bool prv_aligned(const struct pbl_gpu_buffer *buf) {
  uint32_t bytes = s_formats[buf->format].bytes;

  return bytes == 3U || bytes == 1U ||
         ((uintptr_t)buf->data % bytes == 0U && buf->pitch % bytes == 0U);
}

// Flushes or invalidates the cache lines of a rectangle of a buffer: row by row if it is
// narrow, in one go otherwise, and all of the cache if it is large
static void prv_cache(const struct pbl_gpu_buffer *buf, const struct pbl_gpu_rect *rect,
                      bool invalidate) {
  const size_t len = (size_t)rect->width * s_formats[buf->format].bytes;
  uint8_t *first = prv_pixel(buf, rect->x, rect->y);
  size_t rows = rect->height;
  size_t step = buf->pitch;
  size_t size = len;

  if ((rows - 1U) * buf->pitch + len > DMA2D_CACHE_ALL_BYTES) {
    if (invalidate) {
      pbl_dcache_flush_invalidate_all();
    } else {
      pbl_dcache_flush_all();
    }
    return;
  }

  if (len + 2U * CONFIG_DCACHE_LINE_SIZE >= buf->pitch) {
    size = (rows - 1U) * buf->pitch + len;
    rows = 1U;
  }

  for (size_t row = 0U; row < rows; row++) {
    if (invalidate) {
      pbl_dcache_invalidate_range(first + row * step, size);
    } else {
      pbl_dcache_flush_range(first + row * step, size);
    }
  }
}

// The output, a rectangle of the target
static void prv_output(const struct pbl_gpu_stm32_dma2d *dma2d, const struct pbl_gpu_buffer *dst,
                       const struct pbl_gpu_rect *rect) {
  prv_write(dma2d, STM32_DMA2D_OMAR_OFFSET, (uint32_t)(uintptr_t)prv_pixel(dst, rect->x, rect->y));
  prv_write(dma2d, STM32_DMA2D_OOR_OFFSET,
            dst->pitch - (uint32_t)rect->width * s_formats[dst->format].bytes);
  prv_write(dma2d, STM32_DMA2D_OPFCCR_OFFSET,
            PBL_FIELD_PREP(DMA2D_PFCCR_CM_MASK, s_formats[dst->format].cm));
  prv_write(dma2d, STM32_DMA2D_NLR_OFFSET,
            PBL_FIELD_PREP(DMA2D_NLR_PL_MASK, rect->width) |
                PBL_FIELD_PREP(DMA2D_NLR_NL_MASK, rect->height));
}

// The target as the background a job blends over
static void prv_background(const struct pbl_gpu_stm32_dma2d *dma2d,
                           const struct pbl_gpu_buffer *dst, const struct pbl_gpu_rect *rect) {
  prv_write(dma2d, STM32_DMA2D_BGMAR_OFFSET, (uint32_t)(uintptr_t)prv_pixel(dst, rect->x, rect->y));
  prv_write(dma2d, STM32_DMA2D_BGOR_OFFSET,
            dst->pitch - (uint32_t)rect->width * s_formats[dst->format].bytes);
  prv_write(dma2d, STM32_DMA2D_BGPFCCR_OFFSET,
            PBL_FIELD_PREP(DMA2D_PFCCR_CM_MASK, s_formats[dst->format].cm) |
                PBL_FIELD_PREP(DMA2D_PFCCR_AM_MASK, DMA2D_PFCCR_AM_PIXEL));
}

// Starts the job the registers hold: its interrupt ends it
static void prv_start(const struct pbl_gpu_stm32_dma2d *dma2d, uint32_t mode) {
  dma2d->state->active = true;
  prv_write(dma2d, STM32_DMA2D_IFCR_OFFSET, DMA2D_ISR_ALL);
  prv_write(
      dma2d, STM32_DMA2D_CR_OFFSET,
      PBL_FIELD_PREP(DMA2D_CR_MODE_MASK, mode) | DMA2D_CR_LOM | DMA2D_CR_IRQS | DMA2D_CR_START);
}

// The accelerator reads and writes memory behind the cache: what the CPU wrote goes out before
// a job, and what the job wrote is read back after it
static int prv_begin(const struct pbl_gpu_stm32_dma2d *dma2d, const struct pbl_gpu_buffer *dst,
                     const struct pbl_gpu_rect *rect, const struct pbl_gpu_buffer *src,
                     const struct pbl_gpu_rect *src_rect) {
  struct pbl_gpu_stm32_dma2d_state *st = dma2d->state;
  int ret;

  if (!prv_aligned(dst) || (src != nullptr && !prv_aligned(src)) || rect->width > DMA2D_PL_MAX) {
    return -ENOTSUP;
  }

  prv_cache(dst, rect, false);
  if (src != nullptr) {
    prv_cache(src, src_rect, false);
  }

  // The job may be waited for in idle, where Stop would stop it
  pbl_idle_state_block(SOC_STM32_IDLE_STOP);

  ret = pbl_clk_enable(&dma2d->clk);
  if (ret != 0) {
    pbl_idle_state_release(SOC_STM32_IDLE_STOP);
    return ret;
  }

  st->dst = *dst;
  st->rect = *rect;

  return 0;
}

// Ends the job running, once, from its interrupt or from an abort
static void prv_end(const struct pbl_gpu_stm32_dma2d *dma2d, int status) {
  struct pbl_gpu_stm32_dma2d_state *st = dma2d->state;

  pbl_irq_lock();
  if (!st->active) {
    pbl_irq_unlock();
    return;
  }
  st->active = false;
  pbl_irq_unlock();

  pbl_modifyl(DMA2D_CR_START | DMA2D_CR_IRQS, 0U, dma2d->base + STM32_DMA2D_CR_OFFSET);
  prv_write(dma2d, STM32_DMA2D_IFCR_OFFSET, DMA2D_ISR_ALL);
  (void)pbl_clk_disable(&dma2d->clk);
  pbl_idle_state_release(SOC_STM32_IDLE_STOP);
  prv_cache(&st->dst, &st->rect, true);

  pbl_gpu_complete(&dma2d->gpu, status);
}

// A color in the format of the target, as the register-to-memory mode writes it
static uint32_t prv_color(enum pbl_gpu_format format, uint32_t argb) {
  switch (format) {
    case PBL_GPU_FORMAT_RGB565:
      return ((argb >> 8) & 0xF800U) | ((argb >> 5) & 0x07E0U) | ((argb >> 3) & 0x001FU);
    case PBL_GPU_FORMAT_RGB888:
      return argb & 0xFFFFFFU;
    default:
      return argb;
  }
}

static int prv_fill(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
                    const struct pbl_gpu_rect *rect, uint32_t argb) {
  const struct pbl_gpu_stm32_dma2d *dma2d = prv_dma2d(gpu);
  const uint32_t alpha = argb >> 24;
  uint32_t mode;
  int ret;

  ret = prv_begin(dma2d, dst, rect, nullptr, nullptr);
  if (ret != 0) {
    return ret;
  }

  prv_output(dma2d, dst, rect);

  if (alpha == PBL_GPU_ALPHA_OPAQUE) {
    prv_write(dma2d, STM32_DMA2D_OCOLR_OFFSET, prv_color(dst->format, argb));
    mode = DMA2D_CR_MODE_R2M;
  } else {
    // The color is a fixed foreground, blended over what the rectangle holds
    prv_background(dma2d, dst, rect);
    prv_write(dma2d, STM32_DMA2D_FGCOLR_OFFSET, argb & 0xFFFFFFU);
    prv_write(dma2d, STM32_DMA2D_FGPFCCR_OFFSET,
              PBL_FIELD_PREP(DMA2D_PFCCR_CM_MASK, DMA2D_CM_ARGB8888) |
                  PBL_FIELD_PREP(DMA2D_PFCCR_AM_MASK, DMA2D_PFCCR_AM_REPLACE) |
                  PBL_FIELD_PREP(DMA2D_PFCCR_ALPHA_MASK, alpha));
    mode = DMA2D_CR_MODE_BLEND_FG;
  }

  prv_start(dma2d, mode);

  return 0;
}

static int prv_blit(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
                    const struct pbl_gpu_rect *clip, const struct pbl_gpu_blit *blit) {
  const struct pbl_gpu_stm32_dma2d *dma2d = prv_dma2d(gpu);
  const struct pbl_gpu_buffer *src = blit->src;
  const int32_t x0 = PBL_MAX((int32_t)clip->x, (int32_t)blit->x);
  const int32_t y0 = PBL_MAX((int32_t)clip->y, (int32_t)blit->y);
  const int32_t x1 = PBL_MIN((int32_t)clip->x + clip->width, (int32_t)blit->x + src->width);
  const int32_t y1 = PBL_MIN((int32_t)clip->y + clip->height, (int32_t)blit->y + src->height);
  const bool blend = src->format == PBL_GPU_FORMAT_ARGB8888 || src->format == PBL_GPU_FORMAT_A8 ||
                     blit->alpha != PBL_GPU_ALPHA_OPAQUE;
  struct pbl_gpu_rect rect;
  struct pbl_gpu_rect src_rect;
  uint32_t mode;
  int ret;

  if (blit->transform != nullptr) {
    return -ENOTSUP;
  }

  if (x0 >= x1 || y0 >= y1) {
    pbl_gpu_complete(gpu, 0);
    return 0;
  }

  rect = (struct pbl_gpu_rect){
    .x = (int16_t)x0,
    .y = (int16_t)y0,
    .width = (uint16_t)(x1 - x0),
    .height = (uint16_t)(y1 - y0),
  };
  src_rect = (struct pbl_gpu_rect){
    .x = (int16_t)(x0 - blit->x),
    .y = (int16_t)(y0 - blit->y),
    .width = rect.width,
    .height = rect.height,
  };

  ret = prv_begin(dma2d, dst, &rect, src, &src_rect);
  if (ret != 0) {
    return ret;
  }

  prv_output(dma2d, dst, &rect);
  prv_write(dma2d, STM32_DMA2D_FGMAR_OFFSET,
            (uint32_t)(uintptr_t)prv_pixel(src, src_rect.x, src_rect.y));
  prv_write(dma2d, STM32_DMA2D_FGOR_OFFSET,
            src->pitch - (uint32_t)rect.width * s_formats[src->format].bytes);
  prv_write(dma2d, STM32_DMA2D_FGCOLR_OFFSET, blit->color & 0xFFFFFFU);
  prv_write(dma2d, STM32_DMA2D_FGPFCCR_OFFSET,
            PBL_FIELD_PREP(DMA2D_PFCCR_CM_MASK, s_formats[src->format].cm) |
                PBL_FIELD_PREP(DMA2D_PFCCR_AM_MASK, blit->alpha == PBL_GPU_ALPHA_OPAQUE
                                                        ? DMA2D_PFCCR_AM_PIXEL
                                                        : DMA2D_PFCCR_AM_MULTIPLY) |
                PBL_FIELD_PREP(DMA2D_PFCCR_ALPHA_MASK, blit->alpha));

  // Only a source that lets the target through needs it under
  if (blend) {
    prv_background(dma2d, dst, &rect);
    mode = DMA2D_CR_MODE_M2M_BLEND;
  } else if (src->format == dst->format) {
    mode = DMA2D_CR_MODE_M2M;
  } else {
    mode = DMA2D_CR_MODE_M2M_PFC;
  }

  prv_start(dma2d, mode);

  return 0;
}

void pbl_gpu_stm32_dma2d_irq_handler(const struct pbl_gpu_stm32_dma2d *dma2d) {
  uint32_t isr = prv_read(dma2d, STM32_DMA2D_ISR_OFFSET);

  prv_end(dma2d, (isr & (DMA2D_ISR_TEIF | DMA2D_ISR_CEIF)) != 0U ? -EIO : 0);
}

static void prv_abort(const struct pbl_gpu *gpu) {
  const struct pbl_gpu_stm32_dma2d *dma2d = prv_dma2d(gpu);

  pbl_modifyl(DMA2D_CR_START, DMA2D_CR_ABORT, dma2d->base + STM32_DMA2D_CR_OFFSET);
  (void)PBL_POLL_BUSY_US((prv_read(dma2d, STM32_DMA2D_CR_OFFSET) & DMA2D_CR_START) == 0U, 1U,
                         DMA2D_ABORT_TIMEOUT_US);
  prv_end(dma2d, -ETIMEDOUT);
}

static int prv_init(const struct pbl_gpu *gpu) {
  const struct pbl_gpu_stm32_dma2d *dma2d = prv_dma2d(gpu);
  int ret;

  ret = pbl_device_init(&dma2d->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(dma2d->irq);

  return 0;
}

const struct pbl_gpu_ops pbl_gpu_stm32_dma2d_ops = {
  .init = prv_init,
  .fill = prv_fill,
  .blit = prv_blit,
  .abort = prv_abort,
};

#include PBL_DT_INSTANCES(st_stm32_dma2d)
