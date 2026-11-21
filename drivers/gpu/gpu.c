/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/gpu.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/types.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_gpu_class, "gpu");

// Longest job a synchronous call waits for
#define GPU_TIMEOUT_MS 500

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

// Every job holds the token from the start until pbl_gpu_complete(), the only one to give it back
// once a driver started the job
static int prv_start(const struct pbl_gpu *gpu, pbl_timeout_t timeout, pbl_gpu_done_t done,
                     void *arg) {
  if (pbl_sem_take(&gpu->state->free, timeout) != 0) {
    return -EBUSY;
  }

  gpu->state->done = done;
  gpu->state->arg = arg;

  return 0;
}

// A job with nothing to draw, or one the driver did not start
static int prv_end_now(const struct pbl_gpu *gpu, int ret) {
  if (ret == 0) {
    pbl_gpu_complete(gpu, 0);
    return 0;
  }

  pbl_sem_give(&gpu->state->free);

  return ret;
}

void pbl_gpu_complete(const struct pbl_gpu *gpu, int status) {
  struct pbl_gpu_state *st = gpu->state;
  pbl_gpu_done_t done = st->done;
  void *arg = st->arg;

  st->status = status;
  pbl_sem_give(&st->free);

  if (done != nullptr) {
    done(status, arg);
  }
}

int pbl_gpu_wait(const struct pbl_gpu *gpu, pbl_timeout_t timeout) {
  struct pbl_gpu_state *st = gpu->state;

  if (pbl_sem_take(&st->free, timeout) != 0) {
    if (gpu->ops->abort == nullptr) {
      return -ETIMEDOUT;
    }
    gpu->ops->abort(gpu);
    (void)pbl_sem_take(&st->free, PBL_FOREVER);
    st->status = -ETIMEDOUT;
  }
  pbl_sem_give(&st->free);

  return st->status;
}

static int prv_fill(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
                    const struct pbl_gpu_rect *rect, uint32_t argb, pbl_timeout_t timeout,
                    pbl_gpu_done_t done, void *arg) {
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

  ret = prv_start(gpu, timeout, done, arg);
  if (ret != 0) {
    return ret;
  }

  if (!prv_clip(dst, rect, &clipped) || (argb >> 24) == 0U) {
    return prv_end_now(gpu, 0);
  }

  ret = gpu->ops->fill(gpu, dst, &clipped, argb);
  if (ret != 0) {
    return prv_end_now(gpu, ret);
  }

  return 0;
}

static int prv_blit(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
                    const struct pbl_gpu_rect *clip, const struct pbl_gpu_blit *blit,
                    pbl_timeout_t timeout, pbl_gpu_done_t done, void *arg) {
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

  ret = prv_start(gpu, timeout, done, arg);
  if (ret != 0) {
    return ret;
  }

  if (!prv_clip(dst, clip, &clipped) || blit->alpha == 0U) {
    return prv_end_now(gpu, 0);
  }

  ret = gpu->ops->blit(gpu, dst, &clipped, blit);
  if (ret != 0) {
    return prv_end_now(gpu, ret);
  }

  return 0;
}

int pbl_gpu_fill_async(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
                       const struct pbl_gpu_rect *rect, uint32_t argb, pbl_gpu_done_t done,
                       void *arg) {
  return prv_fill(gpu, dst, rect, argb, PBL_NO_WAIT, done, arg);
}

int pbl_gpu_blit_async(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
                       const struct pbl_gpu_rect *clip, const struct pbl_gpu_blit *blit,
                       pbl_gpu_done_t done, void *arg) {
  return prv_blit(gpu, dst, clip, blit, PBL_NO_WAIT, done, arg);
}

// A synchronous call waits for its own job, and stops it if it hangs
struct prv_sync {
  struct pbl_sem done;
  int status;
};

static void prv_sync_done(int status, void *arg) {
  struct prv_sync *sync = arg;

  sync->status = status;
  pbl_sem_give(&sync->done);
}

static int prv_sync_wait(const struct pbl_gpu *gpu, struct prv_sync *sync, int ret) {
  if (ret == 0 && pbl_sem_take(&sync->done, PBL_MSEC(GPU_TIMEOUT_MS)) != 0) {
    if (gpu->ops->abort != nullptr) {
      gpu->ops->abort(gpu);
    }
    (void)pbl_sem_take(&sync->done, PBL_FOREVER);
    sync->status = -ETIMEDOUT;
  }
  pbl_sem_deinit(&sync->done);

  return ret != 0 ? ret : sync->status;
}

int pbl_gpu_fill(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
                 const struct pbl_gpu_rect *rect, uint32_t argb) {
  struct prv_sync sync;

  pbl_sem_init(&sync.done, 0, 1);

  return prv_sync_wait(gpu, &sync,
                       prv_fill(gpu, dst, rect, argb, PBL_FOREVER, prv_sync_done, &sync));
}

int pbl_gpu_blit(const struct pbl_gpu *gpu, const struct pbl_gpu_buffer *dst,
                 const struct pbl_gpu_rect *clip, const struct pbl_gpu_blit *blit) {
  struct prv_sync sync;

  pbl_sem_init(&sync.done, 0, 1);

  return prv_sync_wait(gpu, &sync,
                       prv_blit(gpu, dst, clip, blit, PBL_FOREVER, prv_sync_done, &sync));
}
