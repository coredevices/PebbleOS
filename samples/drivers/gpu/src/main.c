/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/display.h>
#include <pbl/drivers/gpu.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/time.h>
#include <pbl/util/size.h>

#define SPRITE 96U

#define RGB565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

/* Where a point of the opaque left half ends up, and one outside of the transformed sprite. */
struct transform {
  const char *name;
  int16_t angle;
  uint16_t scale;
  int16_t x, y;
  int16_t in_x, in_y;
  int16_t out_x, out_y;
};

static const struct transform s_transforms[] = {
  /* A quarter turn clockwise: the left half goes up. */
  {"rotated", 900, PBL_GPU_SCALE_NONE, 270, 80, 0, -30, 0, 60},
  /* Twice the size: the left edge is twice as far. */
  {"scaled", 0, 2U * PBL_GPU_SCALE_NONE, 60, 250, -80, 0, 0, 110},
  /* Both: the left half goes up, twice as far. */
  {"rotated and scaled", 900, 2U * PBL_GPU_SCALE_NONE, 250, 250, 0, -80, 0, 115},
};

static uint32_t s_start;

static void prv_start(void) {
  s_start = pbl_cycle_get_32();
}

static unsigned int prv_elapsed_us(void) {
  return (unsigned int)((uint64_t)(pbl_cycle_get_32() - s_start) * 1000000U / pbl_cycles_per_sec());
}

static uint16_t prv_pixel(const struct pbl_gpu_buffer *fb, uint16_t x, uint16_t y) {
  return *(const uint16_t *)(const void *)((const uint8_t *)fb->data + y * fb->pitch + x * 2U);
}

static const char *prv_ok(bool ok) {
  return ok ? "ok" : "wrong";
}

int main(void) {
  const struct pbl_display_props *props = pbl_display_get_props(DISPLAY);
  const struct pbl_gpu_caps *caps = pbl_gpu_get_caps(GPU);
  struct pbl_gpu_rect bar = {.x = 0, .y = 0, .width = props->width, .height = 60};
  struct pbl_gpu_buffer sprite = {
    .pitch = SPRITE * sizeof(uint32_t),
    .width = SPRITE,
    .height = SPRITE,
    .format = PBL_GPU_FORMAT_ARGB8888,
  };
  struct pbl_gpu_blit blit = {.src = &sprite, .x = 20, .y = 80, .alpha = PBL_GPU_ALPHA_OPAQUE};
  struct pbl_display_buffer frame;
  struct pbl_gpu_buffer fb;
  unsigned int us;
  uint32_t *px;
  int ret;

  if (props->format != PBL_DISPLAY_FORMAT_RGB565 ||
      (caps->dst_formats & PBL_BIT(PBL_GPU_FORMAT_RGB565)) == 0U) {
    fprintf(stderr, "gpu: no RGB565 frame to draw into\n");
    return -ENOTSUP;
  }

  ret = pbl_display_get_framebuffer(DISPLAY, &frame);
  if (ret != 0) {
    fprintf(stderr, "gpu: no frame (%d)\n", ret);
    return ret;
  }

  /* The GPU draws into buffers the caller owns: here the frame the display sends. */
  fb = (struct pbl_gpu_buffer){
    .data = frame.data,
    .pitch = frame.pitch,
    .width = props->width,
    .height = props->height,
    .format = PBL_GPU_FORMAT_RGB565,
  };
  printf("gpu: %dx%d frame\n", fb.width, fb.height);

  /* An opaque color replaces what the rectangle holds: no rectangle is the whole buffer. */
  prv_start();
  ret = pbl_gpu_fill(GPU, &fb, nullptr, 0xFF202040);
  us = prv_elapsed_us();
  if (ret != 0) {
    fprintf(stderr, "gpu: fill failed (%d)\n", ret);
    return ret;
  }
  printf("gpu: opaque fill: %s, %u us\n",
         prv_ok(prv_pixel(&fb, 5, 5) == RGB565(0x20, 0x20, 0x40) &&
                prv_pixel(&fb, fb.width - 1U, fb.height - 1U) == RGB565(0x20, 0x20, 0x40)),
         us);

  /* Any other alpha blends over it: half white over the background. */
  prv_start();
  ret = pbl_gpu_fill(GPU, &fb, &bar, 0x80FFFFFF);
  us = prv_elapsed_us();
  if (ret != 0) {
    fprintf(stderr, "gpu: blended fill failed (%d)\n", ret);
    return ret;
  }
  printf("gpu: blended fill: %s, %u us\n",
         prv_ok(prv_pixel(&fb, 5, 5) != RGB565(0x20, 0x20, 0x40) &&
                prv_pixel(&fb, 5, 5) != RGB565(0xFF, 0xFF, 0xFF) &&
                prv_pixel(&fb, 5, 70) == RGB565(0x20, 0x20, 0x40)),
         us);

  /* A source with its own alpha: opaque red on the left, fading out to the right. */
  sprite.data = pbl_malloc(sprite.pitch * SPRITE);
  if (sprite.data == nullptr) {
    fprintf(stderr, "gpu: no memory for the sprite\n");
    return -ENOMEM;
  }
  px = sprite.data;
  for (uint32_t y = 0U; y < SPRITE; y++) {
    for (uint32_t x = 0U; x < SPRITE; x++) {
      uint32_t alpha = x < SPRITE / 2U ? 0xFFU : 0xFFU - (x - SPRITE / 2U) * 0xFFU / (SPRITE / 2U);

      px[y * SPRITE + x] = (alpha << 24) | 0xFF0000U;
    }
  }

  prv_start();
  ret = pbl_gpu_blit(GPU, &fb, nullptr, &blit);
  us = prv_elapsed_us();
  if (ret != 0) {
    fprintf(stderr, "gpu: blit failed (%d)\n", ret);
    goto out;
  }
  printf("gpu: blit: %s, %u us\n",
         prv_ok(prv_pixel(&fb, 21, 81) == RGB565(0xFF, 0, 0) &&
                prv_pixel(&fb, 20 + SPRITE + 1, 81) == RGB565(0x20, 0x20, 0x40)),
         us);

  /* The alpha of the blit is on top of the source's. */
  blit.x = 150;
  blit.alpha = 128;
  prv_start();
  ret = pbl_gpu_blit(GPU, &fb, nullptr, &blit);
  us = prv_elapsed_us();
  if (ret != 0) {
    fprintf(stderr, "gpu: blended blit failed (%d)\n", ret);
    goto out;
  }
  printf("gpu: blended blit: %s, %u us\n",
         prv_ok(prv_pixel(&fb, 151, 81) != RGB565(0xFF, 0, 0) &&
                prv_pixel(&fb, 151, 81) != RGB565(0x20, 0x20, 0x40)),
         us);

  /*
   * A transform turns and scales the source around a pivot, which stays where it was. What a GPU
   * cannot do it refuses before drawing, for the caller to draw it otherwise.
   */
  blit.alpha = PBL_GPU_ALPHA_OPAQUE;
  for (size_t i = 0U; i < PBL_ARRAY_SIZE(s_transforms); i++) {
    const struct transform *t = &s_transforms[i];
    struct pbl_gpu_transform transform = {
      .angle = t->angle,
      .scale_x = t->scale,
      .scale_y = t->scale,
      .pivot_x = SPRITE / 2U,
      .pivot_y = SPRITE / 2U,
    };
    /* The pivot, a point of the opaque half and one the source never reaches. */
    const uint16_t px = t->x + SPRITE / 2U;
    const uint16_t py = t->y + SPRITE / 2U;

    blit.x = t->x;
    blit.y = t->y;
    blit.transform = &transform;
    prv_start();
    ret = pbl_gpu_blit(GPU, &fb, nullptr, &blit);
    us = prv_elapsed_us();
    if (ret == -ENOTSUP) {
      printf("gpu: %s blit: not supported\n", t->name);
    } else if (ret != 0) {
      fprintf(stderr, "gpu: %s blit failed (%d)\n", t->name, ret);
      goto out;
    } else {
      printf("gpu: %s blit: %s, %u us\n", t->name,
             prv_ok(prv_pixel(&fb, px - 2U, py) == RGB565(0xFF, 0, 0) &&
                    prv_pixel(&fb, px + t->in_x, py + t->in_y) == RGB565(0xFF, 0, 0) &&
                    prv_pixel(&fb, px + t->out_x, py + t->out_y) == RGB565(0x20, 0x20, 0x40)),
             us);
    }
  }

  ret = pbl_display_update_sync(DISPLAY, &frame, 0U, props->height - 1U);
  if (ret != 0) {
    fprintf(stderr, "gpu: display update failed (%d)\n", ret);
    goto out;
  }

  printf("Sample done\n");

out:
  pbl_free(sprite.data);

  return ret;
}
