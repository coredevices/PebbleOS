/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>
#include <pbl/util/bits.h>

/**
 * @defgroup drivers_gpu GPU
 * @ingroup drivers
 * @brief 2D graphics accelerator class.
 *
 * A GPU is a struct pbl_gpu device that draws into pixel buffers owned by the caller: it fills
 * rectangles and blits one buffer onto another, converting between formats, blending and, where
 * the hardware can, rotating and scaling. What a GPU can do is in its @ref pbl_gpu_caps. A job
 * it cannot take is refused with @c -ENOTSUP before anything is drawn, for the caller to draw it
 * in software, so the class can be used without knowing the hardware behind it.
 *
 * @code{.c}
 * struct pbl_gpu_buffer fb = {
 *   .data = pixels, .pitch = 390 * 2, .width = 390, .height = 450,
 *   .format = PBL_GPU_FORMAT_RGB565,
 * };
 * struct pbl_gpu_rect bar = {.x = 0, .y = 400, .width = 390, .height = 50};
 *
 * // Half-transparent red over what the bar holds
 * ret = pbl_gpu_fill(GPU, &fb, &bar, 0x80FF0000);
 *
 * struct pbl_gpu_blit blit = {.src = &icon, .x = 20, .y = 20, .alpha = 255};
 *
 * ret = pbl_gpu_blit(GPU, &fb, nullptr, &blit);
 * @endcode
 *
 * Calls are made from task context, one job at a time, and return once the job is done, with the
 * target ready to be read by the CPU: buffers may be cached. They return 0 or a negative errno:
 * @c -ENOTSUP for a job the GPU cannot take, @c -EINVAL for an invalid one, @c -ENODEV if the
 * GPU is not ready and @c -EIO or @c -ETIMEDOUT if the hardware failed.
 * @{
 */

/** @brief Pixel formats. Pixels wider than a byte are little-endian. */
enum pbl_gpu_format : uint8_t {
  /** 16 bits per pixel: 5 of red, 6 of green and 5 of blue, from the most significant bit. */
  PBL_GPU_FORMAT_RGB565,
  /** 24 bits per pixel: 8 of red, green and blue, from the most significant bit. */
  PBL_GPU_FORMAT_RGB888,
  /** 32 bits per pixel: 8 of alpha, red, green and blue, from the most significant bit. */
  PBL_GPU_FORMAT_ARGB8888,
  /** 8 bits per pixel, alpha only: the color comes from the blit. */
  PBL_GPU_FORMAT_A8,
  /** Number of formats. */
  PBL_GPU_FORMAT_COUNT,
};

/** @brief Opaque alpha. */
#define PBL_GPU_ALPHA_OPAQUE 255U

/** @brief Scale of 1:1. */
#define PBL_GPU_SCALE_NONE 256U

/** @brief Capability flags. */
enum : uint32_t {
  /** Blits rotate by any angle. */
  PBL_GPU_CAP_ROTATE = PBL_BIT(0),
  /** Blits scale. */
  PBL_GPU_CAP_SCALE = PBL_BIT(1),
};

/** @brief What a GPU can do. */
struct pbl_gpu_caps {
  /** Formats it draws into, a bit per @ref pbl_gpu_format. */
  uint32_t dst_formats;
  /** Formats it blits from, a bit per @ref pbl_gpu_format. */
  uint32_t src_formats;
  /** Capability flags. */
  uint32_t flags;
};

/** @brief A pixel buffer. */
struct pbl_gpu_buffer {
  /** First pixel. */
  void *data;
  /** Bytes from one row to the next. */
  uint32_t pitch;
  /** Width, in pixels. */
  uint16_t width;
  /** Height, in pixels. */
  uint16_t height;
  /** Format. */
  enum pbl_gpu_format format;
};

/** @brief A rectangle of a buffer. */
struct pbl_gpu_rect {
  /** Left column. */
  int16_t x;
  /** Top row. */
  int16_t y;
  /** Width, in pixels. */
  uint16_t width;
  /** Height, in pixels. */
  uint16_t height;
};

/** @brief Rotation and scaling of a blit around a pivot, which stays where it is. */
struct pbl_gpu_transform {
  /** Clockwise rotation, in tenths of a degree. */
  int16_t angle;
  /** Horizontal scale, @ref PBL_GPU_SCALE_NONE for 1:1. */
  uint16_t scale_x;
  /** Vertical scale, @ref PBL_GPU_SCALE_NONE for 1:1. */
  uint16_t scale_y;
  /** Pivot column, from the first pixel of the source. */
  int16_t pivot_x;
  /** Pivot row, from the first pixel of the source. */
  int16_t pivot_y;
};

/** @brief A blit: a source drawn over what the target holds. */
struct pbl_gpu_blit {
  /** Source. */
  const struct pbl_gpu_buffer *src;
  /** Column of the target the first pixel of the source goes to, before any transform. */
  int16_t x;
  /** Row of the target the first pixel of the source goes to, before any transform. */
  int16_t y;
  /** Alpha of the whole source, on top of its own: @ref PBL_GPU_ALPHA_OPAQUE to leave it. */
  uint8_t alpha;
  /** Color of a @ref PBL_GPU_FORMAT_A8 source, as 0x00RRGGBB. */
  uint32_t color;
  /** Transform, or NULL. */
  const struct pbl_gpu_transform *transform;
};

/** @brief Class of the GPUs. */
extern const struct pbl_device_class pbl_gpu_class;

struct pbl_gpu;

/** @cond INTERNAL_HIDDEN */
struct pbl_gpu_state {
  struct pbl_mutex lock;
};
/** @endcond */

/**
 * @brief GPU driver operations.
 *
 * The class layer checks the job against the capabilities, clips it to the target and runs one
 * at a time; drivers draw, and look after the caches of the buffers.
 */
struct pbl_gpu_ops {
  /** Set the GPU up. Returns 0 or a negative errno. */
  int (*init)(const struct pbl_gpu *gpu);
  /**
   * Fill @p rect, within @p dst and not empty, with @p argb, blended if not opaque. Returns 0 or
   * a negative errno.
   */
  int (*fill)(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
              const struct pbl_gpu_rect *rect, uint32_t argb);
  /**
   * Draw @p blit over @p dst, touching only @p clip, within @p dst and not empty. Returns 0 or a
   * negative errno.
   */
  int (*blit)(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
              const struct pbl_gpu_rect *clip, const struct pbl_gpu_blit *blit);
};

/** @brief A GPU. */
struct pbl_gpu {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_gpu_ops *ops;
  /** Runtime state. */
  struct pbl_gpu_state *state;
  /** Capabilities. */
  struct pbl_gpu_caps caps;
};

/** @brief The GPU, defined by the board. */
extern const struct pbl_gpu *const GPU;

/**
 * @brief Define the class state of GPU @p sym, for the @c PBL_*_DEFINE() macro of a driver.
 *
 * @param sym Symbol of the GPU instance.
 */
#define PBL_GPU_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);   \
  static struct pbl_gpu_state sym##_gpu_state = {.lock = PBL_MUTEX_INITIALIZER}

/**
 * @brief Initializer for the struct pbl_gpu of GPU @p sym.
 *
 * @param sym Symbol of the GPU instance, with its state defined by PBL_GPU_STATE_DEFINE().
 * @param _name Name.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 * @param ... Capabilities, designated initializers of struct pbl_gpu_caps.
 */
#define PBL_GPU_INIT(sym, _name, _parent, _ops, ...)                                     \
  {                                                                                      \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_gpu_dev_init, _parent, &pbl_gpu_class), \
    .ops = (_ops), .state = &sym##_gpu_state, .caps = {                                  \
      __VA_ARGS__                                                                        \
    }                                                                                    \
  }

/**
 * @brief Device init of every GPU: calls the driver's init.
 *
 * @param dev GPU device.
 * @return 0 or a negative errno.
 */
int pbl_gpu_dev_init(const struct pbl_device *dev);

/**
 * @brief Get what a GPU can do.
 *
 * @param gpu GPU.
 * @return Capabilities.
 */
static inline const struct pbl_gpu_caps *pbl_gpu_get_caps(const struct pbl_gpu *gpu) {
  return &gpu->caps;
}

/**
 * @brief Fill a rectangle with a color.
 *
 * @param gpu GPU.
 * @param dst Target.
 * @param rect Rectangle, clipped to the target, or NULL for all of it.
 * @param argb Color, as 0xAARRGGBB: blended over what the rectangle holds unless its alpha is
 *             @ref PBL_GPU_ALPHA_OPAQUE.
 * @return 0 or a negative errno.
 */
int pbl_gpu_fill(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
                 const struct pbl_gpu_rect *rect, uint32_t argb);

/**
 * @brief Draw a buffer over another.
 *
 * @param gpu GPU.
 * @param dst Target.
 * @param clip Rectangle of the target that may change, clipped to it, or NULL for all of it.
 * @param blit Source, where it goes and how.
 * @return 0 or a negative errno.
 */
int pbl_gpu_blit(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
                 const struct pbl_gpu_rect *clip, const struct pbl_gpu_blit *blit);

/** @} */
