/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/gpu.h>
#include <pbl/kernel/types.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_gpu_class, "gpu");

static const uint8_t s_bytes_per_pixel[PBL_GPU_FORMAT_COUNT] = {
  [PBL_GPU_FORMAT_RGB565] = 2U,
  [PBL_GPU_FORMAT_RGB888] = 3U,
  [PBL_GPU_FORMAT_ARGB8888] = 4U,
  [PBL_GPU_FORMAT_A8] = 1U,
};

static bool prv_buffer_valid(const struct pbl_gpu_buffer *buf, uint32_t formats) {
  if (buf == nullptr || buf->data == nullptr || buf->format >= PBL_GPU_FORMAT_COUNT) {
    return false;
  }

  if ((formats & PBL_BIT(buf->format)) == 0U) {
    return false;
  }

  return buf->width != 0U && buf->height != 0U &&
         buf->pitch >= (uint32_t)buf->width * s_bytes_per_pixel[buf->format];
}

// Clips a rectangle to a buffer, all of it without one. False if nothing is left.
static bool prv_clip(const struct pbl_gpu_buffer *buf, const struct pbl_gpu_rect *rect,
                     struct pbl_gpu_rect *out) {
  int32_t x0 = 0;
  int32_t y0 = 0;
  int32_t x1 = buf->width;
  int32_t y1 = buf->height;

  if (rect != nullptr) {
    x0 = PBL_MAX(x0, (int32_t)rect->x);
    y0 = PBL_MAX(y0, (int32_t)rect->y);
    x1 = PBL_MIN(x1, (int32_t)rect->x + rect->width);
    y1 = PBL_MIN(y1, (int32_t)rect->y + rect->height);
  }

  if (x0 >= x1 || y0 >= y1) {
    return false;
  }

  *out = (struct pbl_gpu_rect){
    .x = (int16_t)x0,
    .y = (int16_t)y0,
    .width = (uint16_t)(x1 - x0),
    .height = (uint16_t)(y1 - y0),
  };

  return true;
}

int pbl_gpu_dev_init(const struct pbl_device *dev) {
  const struct pbl_gpu *gpu = PBL_CONTAINER_OF(dev, const struct pbl_gpu, dev);

  return gpu->ops->init(gpu);
}

int pbl_gpu_fill(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
                 const struct pbl_gpu_rect *rect, uint32_t argb) {
  struct pbl_gpu_rect clipped;
  int ret;

  if (!pbl_device_is_ready(&gpu->dev)) {
    return -ENODEV;
  }

  if (dst == nullptr || dst->format >= PBL_GPU_FORMAT_COUNT) {
    return -EINVAL;
  }

  if (!prv_buffer_valid(dst, gpu->caps.dst_formats)) {
    return -ENOTSUP;
  }

  if (!prv_clip(dst, rect, &clipped) || (argb >> 24) == 0U) {
    return 0;
  }

  (void)pbl_mutex_lock(&gpu->state->lock, PBL_FOREVER);
  ret = gpu->ops->fill(gpu, dst, &clipped, argb);
  pbl_mutex_unlock(&gpu->state->lock);

  return ret;
}

int pbl_gpu_blit(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
                 const struct pbl_gpu_rect *clip, const struct pbl_gpu_blit *blit) {
  const struct pbl_gpu_transform *tf;
  struct pbl_gpu_rect clipped;
  int ret;

  if (!pbl_device_is_ready(&gpu->dev)) {
    return -ENODEV;
  }

  if (dst == nullptr || blit == nullptr || blit->src == nullptr) {
    return -EINVAL;
  }

  if (!prv_buffer_valid(dst, gpu->caps.dst_formats) ||
      !prv_buffer_valid(blit->src, gpu->caps.src_formats)) {
    return -ENOTSUP;
  }

  tf = blit->transform;
  if (tf != nullptr) {
    if (tf->scale_x == 0U || tf->scale_y == 0U) {
      return -EINVAL;
    }

    if ((tf->angle % 3600) != 0 && (gpu->caps.flags & PBL_GPU_CAP_ROTATE) == 0U) {
      return -ENOTSUP;
    }

    if ((tf->scale_x != PBL_GPU_SCALE_NONE || tf->scale_y != PBL_GPU_SCALE_NONE) &&
        (gpu->caps.flags & PBL_GPU_CAP_SCALE) == 0U) {
      return -ENOTSUP;
    }
  }

  if (!prv_clip(dst, clip, &clipped) || blit->alpha == 0U) {
    return 0;
  }

  (void)pbl_mutex_lock(&gpu->state->lock, PBL_FOREVER);
  ret = gpu->ops->blit(gpu, dst, &clipped, blit);
  pbl_mutex_unlock(&gpu->state->lock);

  return ret;
}
