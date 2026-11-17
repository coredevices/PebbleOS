/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl_lvgl_priv.h"

#include <errno.h>

#include <pbl/drivers/gpu.h>

#include <lvgl_private.h>
#include <pbl_lvgl.h>

#define DRAW_UNIT_ID 64
#define SCORE        70

struct pbl_lvgl__draw_gpu_unit {
  lv_draw_unit_t base;
  lv_draw_task_t *task;
};

static bool prv_format(lv_color_format_t cf, enum pbl_gpu_format *format) {
  switch (cf) {
    case LV_COLOR_FORMAT_RGB565:
      *format = PBL_GPU_FORMAT_RGB565;
      return true;
    case LV_COLOR_FORMAT_RGB888:
      *format = PBL_GPU_FORMAT_RGB888;
      return true;
    case LV_COLOR_FORMAT_ARGB8888:
      *format = PBL_GPU_FORMAT_ARGB8888;
      return true;
    case LV_COLOR_FORMAT_A8:
      *format = PBL_GPU_FORMAT_A8;
      return true;
    default:
      return false;
  }
}

static bool prv_format_supported(lv_color_format_t cf, uint32_t formats) {
  enum pbl_gpu_format format;

  return prv_format(cf, &format) && (formats & PBL_BIT(format)) != 0U;
}

static bool prv_buffer(const lv_image_header_t *header, const void *data,
                       struct pbl_gpu_buffer *buf) {
  if (data == nullptr || !prv_format(header->cf, &buf->format)) {
    return false;
  }

  buf->data = (void *)(uintptr_t)data;
  buf->width = (uint16_t)header->w;
  buf->height = (uint16_t)header->h;
  buf->pitch = header->stride != 0U ? header->stride
                                    : (uint32_t)header->w * lv_color_format_get_size(header->cf);

  return true;
}

// The target of a task and the part of it the task may change, false if there is none
static bool prv_target(const lv_draw_task_t *t, const lv_area_t *area, struct pbl_gpu_buffer *dst,
                       struct pbl_gpu_rect *clip) {
  const lv_layer_t *layer = t->target_layer;
  lv_area_t clipped;

  if (!lv_area_intersect(&clipped, area, &t->clip_area) ||
      !prv_buffer(&layer->draw_buf->header, layer->draw_buf->data, dst)) {
    return false;
  }

  lv_area_move(&clipped, -layer->buf_area.x1, -layer->buf_area.y1);
  *clip = (struct pbl_gpu_rect){
    .x = (int16_t)clipped.x1,
    .y = (int16_t)clipped.y1,
    .width = (uint16_t)lv_area_get_width(&clipped),
    .height = (uint16_t)lv_area_get_height(&clipped),
  };

  return true;
}

static bool prv_large_enough(const lv_draw_task_t *t) {
  lv_area_t clipped;

  return lv_area_intersect(&clipped, &t->area, &t->clip_area) &&
         lv_area_get_size(&clipped) >= CONFIG_LVGL_DRAW_GPU_MIN_PIXELS;
}

// A fill is a rectangle for the GPU unless what is drawn of it reaches its rounded corners
static bool prv_fill_supported(const lv_draw_task_t *t) {
  const lv_draw_fill_dsc_t *dsc = t->draw_dsc;
  int32_t radius = dsc->radius;
  lv_area_t clipped;

  if (dsc->grad.dir != LV_GRAD_DIR_NONE) {
    return false;
  }

  if (radius == 0) {
    return true;
  }

  radius = LV_MIN3(radius, lv_area_get_width(&t->area) / 2, lv_area_get_height(&t->area) / 2);

  return lv_area_intersect(&clipped, &t->area, &t->clip_area) &&
         ((clipped.x1 >= t->area.x1 + radius && clipped.x2 <= t->area.x2 - radius) ||
          (clipped.y1 >= t->area.y1 + radius && clipped.y2 <= t->area.y2 - radius));
}

static bool prv_image_supported(const lv_draw_image_dsc_t *dsc, lv_color_format_t cf,
                                const struct pbl_gpu_caps *caps) {
  bool rotated = dsc->rotation != 0;
  bool scaled = dsc->scale_x != LV_SCALE_NONE || dsc->scale_y != LV_SCALE_NONE;

  if (!prv_format_supported(cf, caps->src_formats)) {
    return false;
  }

  if (dsc->blend_mode != LV_BLEND_MODE_NORMAL || dsc->skew_x != 0 || dsc->skew_y != 0 ||
      dsc->tile != 0U || dsc->clip_radius != 0 || dsc->bitmap_mask_src != nullptr) {
    return false;
  }

  // An alpha-only image takes its color from the recolor, any other is not recolored
  if (cf != LV_COLOR_FORMAT_A8 && dsc->recolor_opa > LV_OPA_MIN) {
    return false;
  }

  if (dsc->scale_x <= 0 || dsc->scale_y <= 0 || dsc->scale_x > UINT16_MAX ||
      dsc->scale_y > UINT16_MAX) {
    return false;
  }

  return (!rotated || (caps->flags & PBL_GPU_CAP_ROTATE) != 0U) &&
         (!scaled || (caps->flags & PBL_GPU_CAP_SCALE) != 0U);
}

// Text set straight: the callback below draws no rotated or outlined glyphs
static bool prv_label_supported(const lv_draw_label_dsc_t *dsc, const struct pbl_gpu_caps *caps) {
  return (caps->src_formats & PBL_BIT(PBL_GPU_FORMAT_A8)) != 0U && dsc->rotation == 0 &&
         dsc->outline_stroke_width == 0;
}

static int32_t prv_evaluate(lv_draw_unit_t *unit, lv_draw_task_t *t) {
  const struct pbl_gpu_caps *caps = pbl_gpu_get_caps(GPU);
  const lv_draw_dsc_base_t *base = t->draw_dsc;
  bool supported = false;

  if (!prv_format_supported(base->layer->color_format, caps->dst_formats) || !prv_large_enough(t)) {
    return 0;
  }

  switch (t->type) {
    case LV_DRAW_TASK_TYPE_FILL:
      supported = prv_fill_supported(t);
      break;
    case LV_DRAW_TASK_TYPE_IMAGE: {
      const lv_draw_image_dsc_t *dsc = t->draw_dsc;
      const lv_image_dsc_t *img = dsc->src;

      // Pixels the GPU reads where they are: a decoded image would have to be cached first
      supported = lv_image_src_get_type(dsc->src) == LV_IMAGE_SRC_VARIABLE &&
                  (img->header.flags & LV_IMAGE_FLAGS_COMPRESSED) == 0U &&
                  prv_image_supported(dsc, img->header.cf, caps);
      break;
    }
    case LV_DRAW_TASK_TYPE_LAYER: {
      const lv_draw_image_dsc_t *dsc = t->draw_dsc;
      const lv_layer_t *src = dsc->src;

      supported = prv_image_supported(dsc, src->color_format, caps);
      break;
    }
    case LV_DRAW_TASK_TYPE_LABEL:
      supported = prv_label_supported(t->draw_dsc, caps);
      break;
    default:
      break;
  }

  if (!supported) {
    return 0;
  }

  if (t->preference_score > SCORE) {
    t->preference_score = SCORE;
    t->preferred_draw_unit_id = DRAW_UNIT_ID;
  }

  return 1;
}

static int prv_fill(lv_draw_task_t *t) {
  const lv_draw_fill_dsc_t *dsc = t->draw_dsc;
  struct pbl_gpu_buffer dst;
  struct pbl_gpu_rect rect;

  if (dsc->opa <= LV_OPA_MIN || !prv_target(t, &t->area, &dst, &rect)) {
    return 0;
  }

  return pbl_gpu_fill(GPU, &dst, &rect,
                      ((uint32_t)dsc->opa << 24) | (lv_color_to_u32(dsc->color) & 0xFFFFFFU));
}

static int prv_image(lv_draw_task_t *t, const lv_image_header_t *header, const void *data) {
  const lv_draw_image_dsc_t *dsc = t->draw_dsc;
  const lv_layer_t *layer = t->target_layer;
  struct pbl_gpu_transform transform = {
    .angle = (int16_t)dsc->rotation,
    .scale_x = (uint16_t)dsc->scale_x,
    .scale_y = (uint16_t)dsc->scale_y,
    .pivot_x = (int16_t)dsc->pivot.x,
    .pivot_y = (int16_t)dsc->pivot.y,
  };
  bool transformed =
      dsc->rotation != 0 || dsc->scale_x != LV_SCALE_NONE || dsc->scale_y != LV_SCALE_NONE;
  struct pbl_gpu_buffer src;
  struct pbl_gpu_blit blit = {
    .src = &src,
    .x = (int16_t)(t->area.x1 - layer->buf_area.x1),
    .y = (int16_t)(t->area.y1 - layer->buf_area.y1),
    .alpha = dsc->opa,
    .color = lv_color_to_u32(dsc->recolor) & 0xFFFFFFU,
    .transform = transformed ? &transform : nullptr,
  };
  lv_area_t area = t->area;
  struct pbl_gpu_buffer dst;
  struct pbl_gpu_rect clip;

  if (dsc->opa <= LV_OPA_MIN) {
    return 0;
  }

  if (!prv_buffer(header, data, &src)) {
    return -ENOTSUP;
  }

  if (transformed) {
    lv_image_buf_get_transformed_area(&area, lv_area_get_width(&t->area),
                                      lv_area_get_height(&t->area), dsc->rotation, dsc->scale_x,
                                      dsc->scale_y, &dsc->pivot);
    lv_area_move(&area, t->area.x1, t->area.y1);
  }

  if (!prv_target(t, &area, &dst, &clip)) {
    return 0;
  }

  return pbl_gpu_blit(GPU, &dst, &clip, &blit);
}

// A glyph as the GPU takes it: its coverage, in the color of the text
static int prv_glyph(lv_draw_task_t *t, const lv_draw_glyph_dsc_t *glyph,
                     const lv_draw_buf_t *coverage) {
  const lv_layer_t *layer = t->target_layer;
  struct pbl_gpu_buffer src;
  struct pbl_gpu_blit blit = {
    .src = &src,
    .x = (int16_t)(glyph->letter_coords->x1 - layer->buf_area.x1),
    .y = (int16_t)(glyph->letter_coords->y1 - layer->buf_area.y1),
    .alpha = glyph->opa,
    .color = lv_color_to_u32(glyph->color) & 0xFFFFFFU,
  };
  struct pbl_gpu_buffer dst;
  struct pbl_gpu_rect clip;

  if ((int32_t)lv_area_get_size(glyph->letter_coords) < CONFIG_LVGL_DRAW_GPU_GLYPH_MIN_PIXELS ||
      coverage->header.cf != LV_COLOR_FORMAT_A8) {
    return -ENOTSUP;
  }

  if (!prv_target(t, glyph->letter_coords, &dst, &clip)) {
    return 0;
  }

  // The buffer is as wide as its rows are long: the glyph is what the blit shows of it
  src = (struct pbl_gpu_buffer){
    .data = coverage->data,
    .pitch = coverage->header.stride,
    .width = (uint16_t)lv_area_get_width(glyph->letter_coords),
    .height = (uint16_t)lv_area_get_height(glyph->letter_coords),
    .format = PBL_GPU_FORMAT_A8,
  };

  return pbl_gpu_blit(GPU, &dst, &clip, &blit);
}

// A glyph of coverage, blended in software when the GPU does not take it
static void prv_coverage_glyph(lv_draw_task_t *t, lv_draw_glyph_dsc_t *glyph) {
  const lv_draw_buf_t *coverage = lv_font_get_glyph_bitmap_internal(glyph->g, glyph->_draw_buf);

  glyph->glyph_data = coverage;
  if (coverage != nullptr && prv_glyph(t, glyph, coverage) != 0) {
    lv_area_t mask_area = *glyph->letter_coords;
    lv_draw_sw_blend_dsc_t blend = {
      .color = glyph->color,
      .opa = glyph->opa,
      .mask_buf = coverage->data,
      .mask_area = &mask_area,
      .mask_stride = (int32_t)coverage->header.stride,
      .blend_area = glyph->letter_coords,
      .mask_res = LV_DRAW_SW_MASK_RES_CHANGED,
    };

    mask_area.x2 = mask_area.x1 + (int32_t)coverage->header.stride - 1;
    lv_draw_sw_blend(t, &blend);
  }
}

// A glyph that is an image, as those of an image font
static void prv_image_glyph(lv_draw_task_t *t, lv_draw_glyph_dsc_t *glyph) {
  lv_draw_image_dsc_t dsc;

  glyph->glyph_data = lv_font_get_glyph_bitmap_internal(glyph->g, glyph->_draw_buf);
  if (glyph->glyph_data == nullptr) {
    return;
  }

  lv_draw_image_dsc_init(&dsc);
  dsc.opa = glyph->opa;
  dsc.src = glyph->glyph_data;
  dsc.recolor = glyph->color;
  lv_draw_sw_image(t, &dsc, glyph->letter_coords);
}

#if LV_USE_FONT_PLACEHOLDER
// The box that stands for a glyph no font has
static void prv_placeholder_glyph(lv_draw_task_t *t, const lv_draw_glyph_dsc_t *glyph) {
  lv_draw_border_dsc_t dsc;

  if (glyph->bg_coords == nullptr) {
    return;
  }

  lv_draw_border_dsc_init(&dsc);
  dsc.opa = glyph->opa;
  dsc.color = glyph->color;
  dsc.width = 1;
  lv_draw_sw_border(t, &dsc, glyph->bg_coords);
}
#endif

// Called for every glyph of a label, and for what it underlines, strikes through or selects
static void prv_letter(lv_draw_task_t *t, lv_draw_glyph_dsc_t *glyph, lv_draw_fill_dsc_t *fill,
                       const lv_area_t *fill_area) {
  if (glyph != nullptr) {
    switch (glyph->format) {
      case LV_FONT_GLYPH_FORMAT_A1:
      case LV_FONT_GLYPH_FORMAT_A2:
      case LV_FONT_GLYPH_FORMAT_A3:
      case LV_FONT_GLYPH_FORMAT_A4:
      case LV_FONT_GLYPH_FORMAT_A8:
        prv_coverage_glyph(t, glyph);
        break;
      case LV_FONT_GLYPH_FORMAT_IMAGE:
        prv_image_glyph(t, glyph);
        break;
#if LV_USE_FONT_PLACEHOLDER
      case LV_FONT_GLYPH_FORMAT_NONE:
        prv_placeholder_glyph(t, glyph);
        break;
#endif
      default:
        break;
    }
  }

  if (fill != nullptr && fill_area != nullptr) {
    lv_draw_sw_fill(t, fill, fill_area);
  }
}

// What the GPU refuses once it sees the job is drawn in software
static void prv_execute(lv_draw_task_t *t) {
  switch (t->type) {
    case LV_DRAW_TASK_TYPE_LABEL: {
      const lv_draw_label_dsc_t *dsc = t->draw_dsc;

      if (dsc->opa > LV_OPA_MIN) {
        lv_draw_label_iterate_characters(t, dsc, &t->area, prv_letter);
      }
      break;
    }
    case LV_DRAW_TASK_TYPE_FILL:
      if (prv_fill(t) != 0) {
        lv_draw_sw_fill(t, t->draw_dsc, &t->area);
      }
      break;
    case LV_DRAW_TASK_TYPE_IMAGE: {
      const lv_draw_image_dsc_t *dsc = t->draw_dsc;
      const lv_image_dsc_t *img = dsc->src;

      if (prv_image(t, &img->header, img->data) != 0) {
        lv_draw_sw_image(t, t->draw_dsc, &t->area);
      }
      break;
    }
    case LV_DRAW_TASK_TYPE_LAYER: {
      const lv_draw_image_dsc_t *dsc = t->draw_dsc;
      const lv_layer_t *src = dsc->src;

      // A layer nothing was drawn on has no buffer
      if (src->draw_buf == nullptr) {
        break;
      }

      if (prv_image(t, &src->draw_buf->header, src->draw_buf->data) != 0) {
        lv_draw_sw_layer(t, t->draw_dsc, &t->area);
      }
      break;
    }
    default:
      break;
  }
}

static int32_t prv_dispatch(lv_draw_unit_t *unit, lv_layer_t *layer) {
  struct pbl_lvgl__draw_gpu_unit *gpu_unit = (struct pbl_lvgl__draw_gpu_unit *)unit;
  lv_draw_task_t *t;

  if (gpu_unit->task != nullptr) {
    return 0;
  }

  t = lv_draw_get_available_task(layer, nullptr, DRAW_UNIT_ID);
  if (t == nullptr || t->preferred_draw_unit_id != DRAW_UNIT_ID) {
    return LV_DRAW_UNIT_IDLE;
  }

  if (lv_draw_layer_alloc_buf(layer) == nullptr) {
    t->state = LV_DRAW_TASK_STATE_FAILED;
    return LV_DRAW_UNIT_IDLE;
  }

  t->state = LV_DRAW_TASK_STATE_IN_PROGRESS;
  gpu_unit->task = t;

  prv_execute(t);

  t->state = LV_DRAW_TASK_STATE_FINISHED;
  gpu_unit->task = nullptr;

  lv_draw_dispatch_request();

  return 1;
}

void pbl_lvgl__draw_gpu_init(void) {
  struct pbl_lvgl__draw_gpu_unit *unit = lv_draw_create_unit(sizeof(*unit));

  unit->base.evaluate_cb = prv_evaluate;
  unit->base.dispatch_cb = prv_dispatch;
  unit->base.name = "PBL_GPU";
}
