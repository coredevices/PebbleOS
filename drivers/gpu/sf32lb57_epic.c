/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/gpu/sf32lb57_epic.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/types.h>
#include <pbl/soc/sf32lb/idle.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>
#include <pbl/util/trig.h>

#include <hardware/sf32lb57_epic.h>

// How long a job is waited for in place before its interrupt is, and at most
#define EPIC_SPIN_US    200U
#define EPIC_TIMEOUT_MS 500U

// The accelerator reads the memories behind the MPIs through their system bus alias
#define EPIC_MPI_CBUS_START  0x10000000U
#define EPIC_MPI_CBUS_END    0x20000000U
#define EPIC_MPI_SBUS_OFFSET 0x50000000U

// Layers and canvas of a job with a transform span at most this many pixels
#define EPIC_TRANSFORM_SPAN_MAX 512

// The positions the transform registers hold, signed
#define EPIC_TRANSFORM_POS_MAX 8191

struct prv_format {
  uint8_t code;
  uint8_t bytes;
  bool alpha;
};

static const struct prv_format s_formats[PBL_GPU_FORMAT_COUNT] = {
  [PBL_GPU_FORMAT_RGB565] = {SF32LB57_EPIC_CFG_FORMAT_RGB565, 2U, false},
  [PBL_GPU_FORMAT_RGB888] = {SF32LB57_EPIC_CFG_FORMAT_RGB888, 3U, false},
  [PBL_GPU_FORMAT_ARGB8888] = {SF32LB57_EPIC_CFG_FORMAT_ARGB8888, 4U, true},
  [PBL_GPU_FORMAT_A8] = {SF32LB57_EPIC_CFG_FORMAT_A8, 1U, true},
};

static const struct pbl_gpu_sf32lb57_epic *prv_epic(const struct pbl_gpu *gpu) {
  return PBL_CONTAINER_OF(gpu, const struct pbl_gpu_sf32lb57_epic, gpu);
}

static uint32_t prv_read(const struct pbl_gpu_sf32lb57_epic *epic, uint32_t offset) {
  return pbl_readl(epic->base + offset);
}

static void prv_write(const struct pbl_gpu_sf32lb57_epic *epic, uint32_t offset, uint32_t value) {
  pbl_writel(value, epic->base + offset);
}

static void prv_modify(const struct pbl_gpu_sf32lb57_epic *epic, uint32_t offset, uint32_t clear,
                       uint32_t set) {
  pbl_modifyl(clear, set, epic->base + offset);
}

static uint32_t prv_bus_addr(const void *p) {
  uint32_t addr = (uint32_t)(uintptr_t)p;

  if (addr >= EPIC_MPI_CBUS_START && addr < EPIC_MPI_CBUS_END) {
    addr += EPIC_MPI_SBUS_OFFSET;
  }

  return addr;
}

static uint32_t prv_pos(uint32_t x, uint32_t y) {
  return PBL_FIELD_PREP(SF32LB57_EPIC_POS_X_MASK, x) | PBL_FIELD_PREP(SF32LB57_EPIC_POS_Y_MASK, y);
}

static uint8_t *prv_pixel(const struct pbl_gpu_buffer *buf, int32_t x, int32_t y) {
  return (uint8_t *)buf->data + (uint32_t)y * buf->pitch +
         (uint32_t)x * s_formats[buf->format].bytes;
}

static uint32_t prv_layer_cfg(const struct pbl_gpu_buffer *buf, uint32_t alpha) {
  const struct prv_format *format = &s_formats[buf->format];

  return PBL_FIELD_PREP(SF32LB57_EPIC_CFG_FORMAT_MASK, format->code) |
         (format->alpha ? SF32LB57_EPIC_CFG_ALPHA_BLEND : SF32LB57_EPIC_CFG_ALPHA_SEL) |
         PBL_FIELD_PREP(SF32LB57_EPIC_CFG_ALPHA_MASK, alpha) |
         PBL_FIELD_PREP(SF32LB57_EPIC_CFG_WIDTH_MASK, buf->pitch) | SF32LB57_EPIC_CFG_ACTIVE;
}

// A layer of L0 or L1 over the canvas, shown from first
static void prv_layer(const struct pbl_gpu_sf32lb57_epic *epic, uint32_t layer,
                      const struct pbl_gpu_buffer *buf, const void *first, uint32_t x, uint32_t y,
                      uint16_t width, uint16_t height, uint32_t alpha, uint32_t color) {
  prv_write(epic, layer + SF32LB57_EPIC_LX_TL_POS_OFFSET, prv_pos(x, y));
  prv_write(epic, layer + SF32LB57_EPIC_LX_BR_POS_OFFSET, prv_pos(x + width - 1U, y + height - 1U));
  prv_write(epic, layer + SF32LB57_EPIC_LX_SRC_OFFSET, prv_bus_addr(first));
  prv_write(epic, layer + SF32LB57_EPIC_LX_FILL_OFFSET,
            PBL_FIELD_PREP(SF32LB57_EPIC_FILL_RGB_MASK, color));
  prv_write(epic, layer + SF32LB57_EPIC_LX_CFG_OFFSET, prv_layer_cfg(buf, alpha));
}

// The canvas, written to dst from first, over the background color
static void prv_canvas(const struct pbl_gpu_sf32lb57_epic *epic, const struct pbl_gpu_buffer *dst,
                       const void *first, uint32_t x, uint32_t y, uint16_t width, uint16_t height,
                       uint32_t background) {
  const struct prv_format *format = &s_formats[dst->format];

  prv_write(epic, SF32LB57_EPIC_CANVAS_TL_POS_OFFSET, prv_pos(x, y));
  prv_write(epic, SF32LB57_EPIC_CANVAS_BR_POS_OFFSET, prv_pos(x + width - 1U, y + height - 1U));
  prv_write(epic, SF32LB57_EPIC_CANVAS_BG_OFFSET,
            PBL_FIELD_PREP(SF32LB57_EPIC_CANVAS_BG_RGB_MASK, background));
  prv_write(epic, SF32LB57_EPIC_AHB_CTRL_OFFSET,
            PBL_FIELD_PREP(SF32LB57_EPIC_AHB_CTRL_O_FORMAT_MASK, format->code));
  prv_write(epic, SF32LB57_EPIC_AHB_MEM_OFFSET, prv_bus_addr(first));
  prv_write(epic, SF32LB57_EPIC_AHB_STRIDE_OFFSET,
            PBL_FIELD_PREP(SF32LB57_EPIC_AHB_STRIDE_OFFSET_MASK,
                           dst->pitch - (uint32_t)width * format->bytes));
}

static bool prv_busy(const struct pbl_gpu_sf32lb57_epic *epic) {
  return prv_read(epic, SF32LB57_EPIC_STATUS_OFFSET) != 0U;
}

static void prv_layers_off(const struct pbl_gpu_sf32lb57_epic *epic) {
  prv_write(epic, SF32LB57_EPIC_L0_CFG_OFFSET, 0U);
  prv_write(epic, SF32LB57_EPIC_L1_CFG_OFFSET, 0U);
  prv_write(epic, SF32LB57_EPIC_VL_CFG_OFFSET, 0U);
  prv_modify(epic, SF32LB57_EPIC_VL_ROT_OFFSET, 0U, SF32LB57_EPIC_VL_ROT_CALC_CLR);
}

// Runs the job the registers hold
static int prv_run(const struct pbl_gpu_sf32lb57_epic *epic) {
  const uint32_t spin = (uint32_t)((uint64_t)pbl_cycles_per_sec() * EPIC_SPIN_US / 1000000U);
  uint32_t start;
  int ret = 0;

  prv_write(epic, SF32LB57_EPIC_EOF_IRQ_OFFSET, prv_read(epic, SF32LB57_EPIC_EOF_IRQ_OFFSET));
  prv_modify(epic, SF32LB57_EPIC_COMMAND_OFFSET, 0U, SF32LB57_EPIC_COMMAND_START);

  start = pbl_cycle_get_32();
  while (prv_busy(epic) && (pbl_cycle_get_32() - start) < spin) {
  }

  if (prv_busy(epic)) {
    pbl_sem_reset(&epic->state->done);
    prv_modify(epic, SF32LB57_EPIC_SETTING_OFFSET, 0U, SF32LB57_EPIC_SETTING_EOF_IRQ_MASK);

    if (prv_busy(epic) && pbl_sem_take(&epic->state->done, PBL_MSEC(EPIC_TIMEOUT_MS)) != 0 &&
        prv_busy(epic)) {
      (void)pbl_reset_reset(&epic->rst);
      ret = -ETIMEDOUT;
    }

    prv_modify(epic, SF32LB57_EPIC_SETTING_OFFSET, SF32LB57_EPIC_SETTING_EOF_IRQ_MASK, 0U);
  }

  prv_layers_off(epic);

  return ret;
}

// Flushes or invalidates the cache lines of a rectangle of a buffer: row by row if it is
// narrow, in one go otherwise
static void prv_cache(const struct pbl_gpu_buffer *buf, const struct pbl_gpu_rect *rect,
                      bool invalidate) {
  const size_t len = (size_t)rect->width * s_formats[buf->format].bytes;
  uint8_t *first = prv_pixel(buf, rect->x, rect->y);
  size_t rows = rect->height;
  size_t step = buf->pitch;
  size_t size = len;

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

// The accelerator reads and writes memory behind the cache: what the CPU wrote goes out before
// a job, and what the job wrote is read back after it. Of a source, only what the job reads of
// it, and nothing if it is in the MPI window the CPU runs from, which holds flash.
static int prv_begin(const struct pbl_gpu_sf32lb57_epic *epic, const struct pbl_gpu_buffer *dst,
                     const struct pbl_gpu_rect *rect, const struct pbl_gpu_buffer *src,
                     const struct pbl_gpu_rect *src_rect) {
  int ret;

  ret = pbl_clk_enable(&epic->clk);
  if (ret != 0) {
    return ret;
  }

  pbl_idle_state_block(SOC_SF32LB_IDLE_DEEPWFI);

  prv_cache(dst, rect, false);
  if (src != nullptr && prv_bus_addr(src->data) == (uint32_t)(uintptr_t)src->data) {
    prv_cache(src, src_rect, false);
  }

  return 0;
}

static void prv_end(const struct pbl_gpu_sf32lb57_epic *epic, const struct pbl_gpu_buffer *dst,
                    const struct pbl_gpu_rect *rect) {
  prv_cache(dst, rect, true);

  pbl_idle_state_release(SOC_SF32LB_IDLE_DEEPWFI);
  (void)pbl_clk_disable(&epic->clk);
}

static int prv_fill(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
                    const struct pbl_gpu_rect *rect, uint32_t argb) {
  const struct pbl_gpu_sf32lb57_epic *epic = prv_epic(gpu);
  const uint8_t *first = prv_pixel(dst, rect->x, rect->y);
  const uint32_t alpha = argb >> 24;
  int ret;

  ret = prv_begin(epic, dst, rect, nullptr, nullptr);
  if (ret != 0) {
    return ret;
  }

  // The color is the background, under what the rectangle holds at the alpha left for it
  if (alpha != PBL_GPU_ALPHA_OPAQUE) {
    prv_layer(epic, SF32LB57_EPIC_L1_CFG_OFFSET, dst, first, 0U, 0U, rect->width, rect->height,
              256U - alpha, 0U);
  }
  prv_canvas(epic, dst, first, 0U, 0U, rect->width, rect->height, argb);

  ret = prv_run(epic);

  prv_end(epic, dst, rect);

  return ret;
}

// The video layer of a transformed blit, in the coordinates of the target
struct prv_video {
  // Top left and size of the layer
  int32_t x, y;
  int32_t width, height;
  // Pivot and top left of the source, from the top left of the layer, and the size of what the
  // rotation works on, less one
  int32_t pivot_x, pivot_y;
  int32_t src_x, src_y;
  int32_t rot_max_col, rot_max_line;
  // Rotation: whole degrees, never a multiple of 90 unless 0, and magnitudes in Q12
  uint32_t deg;
  uint32_t sin, cos;
  // Source pixels per output pixel and first source position, 13.16
  uint32_t pitch_x, pitch_y;
  uint32_t init_x, init_y;
};

// Scales an axis of the layer from its top left, moving it so that the pivot stays in place
static bool prv_scale_axis(uint32_t pitch, int32_t pivot, int32_t len, int32_t *pos, int32_t *size,
                           uint32_t *init) {
  const int64_t scaled = ((int64_t)pivot * ((int64_t)1 << 32)) / pitch;
  const int64_t whole = scaled & ~(int64_t)0xFFFF;

  *init = (uint32_t)(((scaled - whole) * pitch) >> 16);
  *pos += pivot - (int32_t)(whole >> 16);

  if (((int64_t)len << 16) < *init || ((int64_t)len << 16) < pitch) {
    return false;
  }

  *size = (int32_t)(((((int64_t)len << 16) - *init) << 16) / pitch >> 16) + 1;

  return true;
}

// Works a transformed blit out. The layer rotates the source around the pivot, within itself,
// and then scales the result from its top left. False if nothing of the source is left.
static bool prv_video(const struct pbl_gpu_blit *blit, struct prv_video *video) {
  const struct pbl_gpu_transform *tf = blit->transform;
  const struct pbl_gpu_buffer *src = blit->src;
  int32_t angle = tf->angle % 3600;
  int32_t len_x = src->width - 1;
  int32_t len_y = src->height - 1;

  if (angle < 0) {
    angle += 3600;
  }

  *video = (struct prv_video){
    .x = blit->x,
    .y = blit->y,
    .width = src->width,
    .height = src->height,
    .pivot_x = tf->pivot_x,
    .pivot_y = tf->pivot_y,
    .pitch_x = (SF32LB57_EPIC_VL_SCALE_RATIO_NONE * PBL_GPU_SCALE_NONE) / tf->scale_x,
    .pitch_y = (SF32LB57_EPIC_VL_SCALE_RATIO_NONE * PBL_GPU_SCALE_NONE) / tf->scale_y,
  };

  if (video->pitch_x == 0U || video->pitch_y == 0U) {
    return false;
  }

  if (angle != 0) {
    const int32_t trig_angle = (angle * TRIG_MAX_ANGLE) / 3600;
    const int64_t sin = sin_lookup(trig_angle);
    const int64_t cos = cos_lookup(trig_angle);
    const int32_t xs[] = {-tf->pivot_x, src->width - tf->pivot_x};
    const int32_t ys[] = {-tf->pivot_y, src->height - tf->pivot_y};
    int32_t x0 = INT32_MAX;
    int32_t y0 = INT32_MAX;
    int32_t x1 = INT32_MIN;
    int32_t y1 = INT32_MIN;
    int32_t dx;
    int32_t dy;

    // What the rotated source covers, a pixel wider on every side
    for (size_t i = 0U; i < PBL_ARRAY_SIZE(xs); i++) {
      for (size_t j = 0U; j < PBL_ARRAY_SIZE(ys); j++) {
        const int32_t x = (int32_t)((cos * xs[i] - sin * ys[j]) / TRIG_MAX_RATIO) + tf->pivot_x;
        const int32_t y = (int32_t)((sin * xs[i] + cos * ys[j]) / TRIG_MAX_RATIO) + tf->pivot_y;

        x0 = PBL_MIN(x0, x);
        y0 = PBL_MIN(y0, y);
        x1 = PBL_MAX(x1, x);
        y1 = PBL_MAX(y1, y);
      }
    }
    x0 -= 1;
    y0 -= 1;
    x1 += 1;
    y1 += 1;

    // The layer is that area: the source moves within it by as much
    dx = -x0;
    dy = -y0;
    video->x -= dx;
    video->y -= dy;
    video->width = x1 - x0 + 1;
    video->height = y1 - y0 + 1;
    video->pivot_x += dx;
    video->pivot_y += dy;
    video->src_x = dx;
    video->src_y = dy;
    video->rot_max_col = PBL_MAX(video->width - 1, dx + src->width - 1) - PBL_MIN(0, dx);
    video->rot_max_line = PBL_MAX(video->height - 1, dy + src->height - 1) - PBL_MIN(0, dy);

    // The quadrant comes from the degrees, which the boundaries would leave undefined
    video->deg = (uint32_t)(angle / 10);
    if ((video->deg % 90U) == 0U) {
      video->deg += 1U;
    }
    video->sin = (uint32_t)((sin < 0 ? -sin : sin) >> 4);
    video->cos = (uint32_t)((cos < 0 ? -cos : cos) >> 4);

    len_x = video->width;
    len_y = video->height;
  }

  if (video->pitch_x != SF32LB57_EPIC_VL_SCALE_RATIO_NONE ||
      video->pitch_y != SF32LB57_EPIC_VL_SCALE_RATIO_NONE) {
    if (!prv_scale_axis(video->pitch_x, video->pivot_x, len_x, &video->x, &video->width,
                        &video->init_x) ||
        !prv_scale_axis(video->pitch_y, video->pivot_y, len_y, &video->y, &video->height,
                        &video->init_y)) {
      return false;
    }
  }

  return true;
}

static uint32_t prv_signed_pos(int32_t x, int32_t y) {
  return PBL_FIELD_PREP(SF32LB57_EPIC_VL_ROT_M_CFG_X_MASK, (uint32_t)x) |
         PBL_FIELD_PREP(SF32LB57_EPIC_VL_ROT_M_CFG_Y_MASK, (uint32_t)y);
}

static int prv_blit_transformed(const struct pbl_gpu_sf32lb57_epic *epic,
                                const struct pbl_gpu_buffer *dst, const struct pbl_gpu_rect *clip,
                                const struct pbl_gpu_blit *blit) {
  const struct pbl_gpu_buffer *src = blit->src;
  const struct prv_format *format = &s_formats[src->format];
  struct pbl_gpu_rect area;
  const uint8_t *first;
  struct prv_video video;
  uint32_t cfg;
  int32_t x0;
  int32_t y0;
  int32_t x1;
  int32_t y1;
  int ret;

  if (!prv_video(blit, &video)) {
    return 0;
  }

  // Only what the layer covers of the clip is drawn: the layer ends there, but starts where it
  // does
  area = (struct pbl_gpu_rect){
    .x = (int16_t)PBL_MAX(video.x, (int32_t)clip->x),
    .y = (int16_t)PBL_MAX(video.y, (int32_t)clip->y),
  };
  x1 = PBL_MIN(video.x + video.width, (int32_t)clip->x + clip->width);
  y1 = PBL_MIN(video.y + video.height, (int32_t)clip->y + clip->height);
  if (area.x >= x1 || area.y >= y1) {
    return 0;
  }
  area.width = (uint16_t)(x1 - area.x);
  area.height = (uint16_t)(y1 - area.y);
  first = prv_pixel(dst, area.x, area.y);

  // Positions are unsigned: everything moves so that the top left of it all is at 0
  x0 = video.x;
  y0 = video.y;
  if (x1 - x0 > EPIC_TRANSFORM_SPAN_MAX || y1 - y0 > EPIC_TRANSFORM_SPAN_MAX ||
      video.rot_max_col > EPIC_TRANSFORM_POS_MAX || video.rot_max_line > EPIC_TRANSFORM_POS_MAX ||
      PBL_MAX(video.pivot_x, -video.pivot_x) > EPIC_TRANSFORM_POS_MAX ||
      PBL_MAX(video.pivot_y, -video.pivot_y) > EPIC_TRANSFORM_POS_MAX ||
      PBL_MAX(video.src_x, -video.src_x) > EPIC_TRANSFORM_POS_MAX ||
      PBL_MAX(video.src_y, -video.src_y) > EPIC_TRANSFORM_POS_MAX) {
    return -ENOTSUP;
  }

  ret = prv_begin(epic, dst, &area, src,
                  &(struct pbl_gpu_rect){.width = src->width, .height = src->height});
  if (ret != 0) {
    return ret;
  }

  prv_layer(epic, SF32LB57_EPIC_L0_CFG_OFFSET, dst, first, (uint32_t)(area.x - x0),
            (uint32_t)(area.y - y0), area.width, area.height, PBL_GPU_ALPHA_OPAQUE, 0U);

  prv_write(epic, SF32LB57_EPIC_VL_TL_POS_OFFSET,
            prv_pos((uint32_t)(video.x - x0), (uint32_t)(video.y - y0)));
  prv_write(epic, SF32LB57_EPIC_VL_BR_POS_OFFSET,
            prv_pos((uint32_t)(x1 - 1 - x0), (uint32_t)(y1 - 1 - y0)));
  prv_write(epic, SF32LB57_EPIC_VL_SRC_OFFSET, prv_bus_addr(src->data));
  prv_write(epic, SF32LB57_EPIC_VL_EXTENTS_OFFSET,
            PBL_FIELD_PREP(SF32LB57_EPIC_VL_EXTENTS_MAX_COL_MASK, src->width - 1U) |
                PBL_FIELD_PREP(SF32LB57_EPIC_VL_EXTENTS_MAX_LINE_MASK, src->height - 1U));
  prv_write(epic, SF32LB57_EPIC_VL_FILL_OFFSET,
            PBL_FIELD_PREP(SF32LB57_EPIC_FILL_RGB_MASK, blit->color));

  prv_modify(epic, SF32LB57_EPIC_VL_ROT_OFFSET, 0U, SF32LB57_EPIC_VL_ROT_CALC_CLR);
  if (video.deg != 0U) {
    prv_modify(epic, SF32LB57_EPIC_VL_ROT_OFFSET, SF32LB57_EPIC_VL_ROT_ROT_DEG_MASK,
               SF32LB57_EPIC_VL_ROT_DEG_FORCE |
                   PBL_FIELD_PREP(SF32LB57_EPIC_VL_ROT_ROT_DEG_MASK, video.deg));
    prv_write(epic, SF32LB57_EPIC_VL_ROT_M_CFG1_OFFSET,
              SF32LB57_EPIC_VL_ROT_M_CFG1_M_MODE |
                  PBL_FIELD_PREP(SF32LB57_EPIC_VL_ROT_M_CFG1_M_ROT_MAX_COL_MASK,
                                 (uint32_t)video.rot_max_col) |
                  PBL_FIELD_PREP(SF32LB57_EPIC_VL_ROT_M_CFG1_M_ROT_MAX_LINE_MASK,
                                 (uint32_t)video.rot_max_line));
    prv_write(epic, SF32LB57_EPIC_VL_ROT_M_CFG2_OFFSET,
              prv_signed_pos(video.pivot_x, video.pivot_y));
    prv_write(epic, SF32LB57_EPIC_VL_ROT_M_CFG3_OFFSET, prv_signed_pos(video.src_x, video.src_y));
  } else {
    prv_modify(epic, SF32LB57_EPIC_VL_ROT_M_CFG1_OFFSET, SF32LB57_EPIC_VL_ROT_M_CFG1_M_MODE, 0U);
    prv_modify(epic, SF32LB57_EPIC_VL_ROT_OFFSET,
               SF32LB57_EPIC_VL_ROT_DEG_FORCE | SF32LB57_EPIC_VL_ROT_ROT_DEG_MASK, 0U);
  }

  // Over every other layer
  prv_write(epic, SF32LB57_EPIC_VL_MISC_CFG_OFFSET,
            PBL_FIELD_PREP(SF32LB57_EPIC_VL_MISC_CFG_COS_FORCE_VALUE_MASK, video.cos) |
                PBL_FIELD_PREP(SF32LB57_EPIC_VL_MISC_CFG_SIN_FORCE_VALUE_MASK, video.sin) |
                PBL_FIELD_PREP(SF32LB57_EPIC_VL_MISC_CFG_BLEND_DEPTH_MASK, 3U));
  prv_write(epic, SF32LB57_EPIC_VL_SCALE_INIT_CFG1_OFFSET, video.init_x);
  prv_write(epic, SF32LB57_EPIC_VL_SCALE_INIT_CFG2_OFFSET, video.init_y);
  prv_write(epic, SF32LB57_EPIC_VL_SCALE_RATIO_H_OFFSET, video.pitch_x);
  prv_write(epic, SF32LB57_EPIC_VL_SCALE_RATIO_V_OFFSET, video.pitch_y);

  // An opaque layer is its pixels alone
  cfg = PBL_FIELD_PREP(SF32LB57_EPIC_CFG_FORMAT_MASK, format->code) |
        PBL_FIELD_PREP(SF32LB57_EPIC_CFG_ALPHA_MASK, blit->alpha) |
        PBL_FIELD_PREP(SF32LB57_EPIC_CFG_WIDTH_MASK, src->pitch) | SF32LB57_EPIC_CFG_ACTIVE;
  if (blit->alpha != PBL_GPU_ALPHA_OPAQUE) {
    cfg |= format->alpha ? SF32LB57_EPIC_CFG_ALPHA_BLEND : SF32LB57_EPIC_CFG_ALPHA_SEL;
  }
  prv_write(epic, SF32LB57_EPIC_VL_CFG_OFFSET, cfg);

  prv_canvas(epic, dst, first, (uint32_t)(area.x - x0), (uint32_t)(area.y - y0), area.width,
             area.height, 0U);

  ret = prv_run(epic);

  prv_end(epic, dst, &area);

  return ret;
}

static int prv_blit(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
                    const struct pbl_gpu_rect *clip, const struct pbl_gpu_blit *blit) {
  const struct pbl_gpu_sf32lb57_epic *epic = prv_epic(gpu);
  const struct pbl_gpu_buffer *src = blit->src;
  const int32_t x0 = PBL_MAX((int32_t)clip->x, (int32_t)blit->x);
  const int32_t y0 = PBL_MAX((int32_t)clip->y, (int32_t)blit->y);
  const int32_t x1 = PBL_MIN((int32_t)clip->x + clip->width, (int32_t)blit->x + src->width);
  const int32_t y1 = PBL_MIN((int32_t)clip->y + clip->height, (int32_t)blit->y + src->height);
  struct pbl_gpu_rect rect;
  const uint8_t *first;
  int ret;

  if (blit->transform != nullptr) {
    return prv_blit_transformed(epic, dst, clip, blit);
  }

  if (x0 >= x1 || y0 >= y1) {
    return 0;
  }

  rect = (struct pbl_gpu_rect){
    .x = (int16_t)x0,
    .y = (int16_t)y0,
    .width = (uint16_t)(x1 - x0),
    .height = (uint16_t)(y1 - y0),
  };
  first = prv_pixel(dst, x0, y0);

  ret = prv_begin(epic, dst, &rect, src,
                  &(struct pbl_gpu_rect){
                    .x = (int16_t)(x0 - blit->x),
                    .y = (int16_t)(y0 - blit->y),
                    .width = rect.width,
                    .height = rect.height,
                  });
  if (ret != 0) {
    return ret;
  }

  // Only a source that lets the target through needs it under
  if (s_formats[src->format].alpha || blit->alpha != PBL_GPU_ALPHA_OPAQUE) {
    prv_layer(epic, SF32LB57_EPIC_L0_CFG_OFFSET, dst, first, 0U, 0U, rect.width, rect.height,
              PBL_GPU_ALPHA_OPAQUE, 0U);
  }
  prv_layer(epic, SF32LB57_EPIC_L1_CFG_OFFSET, src, prv_pixel(src, x0 - blit->x, y0 - blit->y), 0U,
            0U, rect.width, rect.height, blit->alpha, blit->color);
  prv_canvas(epic, dst, first, 0U, 0U, rect.width, rect.height, 0U);

  ret = prv_run(epic);

  prv_end(epic, dst, &rect);

  return ret;
}

void pbl_gpu_sf32lb57_epic_irq_handler(const struct pbl_gpu_sf32lb57_epic *epic) {
  prv_write(epic, SF32LB57_EPIC_EOF_IRQ_OFFSET, prv_read(epic, SF32LB57_EPIC_EOF_IRQ_OFFSET));
  prv_modify(epic, SF32LB57_EPIC_SETTING_OFFSET, SF32LB57_EPIC_SETTING_EOF_IRQ_MASK, 0U);
  pbl_sem_give(&epic->state->done);
}

static int prv_init(const struct pbl_gpu *gpu) {
  const struct pbl_gpu_sf32lb57_epic *epic = prv_epic(gpu);
  int ret;

  if (pbl_device_init(&epic->clk.ctrl->dev) != 0 || pbl_device_init(&epic->rst.ctrl->dev) != 0) {
    return -ENODEV;
  }

  ret = pbl_clk_enable(&epic->clk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_reset_reset(&epic->rst);
  (void)pbl_clk_disable(&epic->clk);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(epic->irq);

  return 0;
}

const struct pbl_gpu_ops pbl_gpu_sf32lb57_epic_ops = {
  .init = prv_init,
  .fill = prv_fill,
  .blit = prv_blit,
};

#include PBL_DT_INSTANCES(sifli_sf32lb57_epic)
