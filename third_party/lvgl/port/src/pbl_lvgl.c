/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl_lvgl_priv.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>

#include <pbl/drivers/display.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/scratch.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>

#include <pbl_lvgl.h>

#ifdef CONFIG_INPUT
#include <pbl/input/input.h>
#include <pbl/kernel/msgq.h>
#endif

#ifdef CONFIG_TOUCH
#include <pbl/drivers/touch.h>
#endif

PBL_LOG_MODULE_DEFINE(lvgl, CONFIG_LVGL_LOG_LEVEL);

#define FLUSH_WAIT_MS 1000U

/* The two colors LVGL puts at the start of a 1 bit per pixel buffer */
#define MONO_PALETTE_SIZE 8U

#define TOUCH_DOWN   (1U << 31)
#define TOUCH_X(pos) (((pos) >> 15) & 0x7FFFU)
#define TOUCH_Y(pos) ((pos) & 0x7FFFU)

struct pbl_lvgl__key {
  uint16_t code;
  bool pressed;
};

static lv_display_t *s_disp;
static lv_indev_t *s_pointer;
static lv_indev_t *s_keypad;
static struct pbl_display_buffer s_fb;
static bool s_fb_allocated;
static enum pbl_display_format s_format;
static int32_t s_dirty_y0;
static int32_t s_dirty_y1;
/* The frame being sent while the next one is rendered in the other, with two of them. */
static struct pbl_display_buffer s_sending;
static bool s_two_frames;
static PBL_SEM_DEFINE(s_sent, 0, 1);

static uint32_t prv_tick(void) {
  return (uint32_t)pbl_uptime_ms();
}

static void prv_convert_argb2222(uint8_t *row, const uint16_t *src, int32_t x0, int32_t w) {
  for (int32_t i = 0; i < w; i++) {
    uint16_t c = src[i];

    row[x0 + i] =
        (uint8_t)(0xC0U | ((c >> 14) << 4) | (((c >> 9) & 0x3U) << 2) | ((c >> 3) & 0x3U));
  }
}

#if LV_DRAW_SW_SUPPORT_I1
/* LVGL puts the leftmost pixel of a byte in its top bit, the display in its bottom one. */
static uint8_t prv_reverse(uint8_t b) {
  b = (uint8_t)(((b & 0xF0U) >> 4) | ((b & 0x0FU) << 4));
  b = (uint8_t)(((b & 0xCCU) >> 2) | ((b & 0x33U) << 2));
  return (uint8_t)(((b & 0xAAU) >> 1) | ((b & 0x55U) << 1));
}

/* Rendered in the format of the display but for the order of the bits: whole bytes. */
static void prv_copy_mono(uint8_t *row, const uint8_t *src, int32_t x0, int32_t w) {
  for (int32_t i = 0; i < w / 8; i++) {
    row[x0 / 8 + i] = prv_reverse(src[i]);
  }
}

/* Runs for every area LVGL is about to redraw: bytes are redrawn whole. */
static void prv_round_mono(lv_event_t *e) {
  lv_area_t *area = lv_event_get_param(e);

  area->x1 &= ~0x7;
  area->x2 |= 0x7;
}
#endif

/* Runs from the display's interrupt. */
static void prv_sent(int status, void *arg) {
  pbl_sem_give(&s_sent);
}

static void prv_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  int32_t w = lv_area_get_width(area);
  uint32_t stride = lv_draw_buf_width_to_stride((uint32_t)w, lv_display_get_color_format(disp));

  /* A buffer of indexed colors starts with them. */
  if (s_format == PBL_DISPLAY_FORMAT_MONO) {
    px_map += MONO_PALETTE_SIZE;
  }

  /* A frame rendered in place only has to be sent. */
  for (int32_t y = area->y1; s_format != PBL_DISPLAY_FORMAT_RGB565 && y <= area->y2; y++) {
    uint8_t *row = &s_fb.data[(uint32_t)y * s_fb.pitch];

#if LV_DRAW_SW_SUPPORT_I1
    if (s_format == PBL_DISPLAY_FORMAT_MONO) {
      prv_copy_mono(row, px_map, area->x1, w);
    } else
#endif
    {
      prv_convert_argb2222(row, (const uint16_t *)(const void *)px_map, area->x1, w);
    }
    px_map += stride;
  }

  s_dirty_y0 = LV_MIN(s_dirty_y0, area->y1);
  s_dirty_y1 = LV_MAX(s_dirty_y1, area->y2);

  if (lv_display_flush_is_last(disp)) {
    uint16_t y0 = (uint16_t)s_dirty_y0;
    uint16_t y1 = (uint16_t)s_dirty_y1;
    int ret;

    s_dirty_y0 = INT32_MAX;
    s_dirty_y1 = 0;

    if (s_two_frames) {
      /*
       * The frame goes out while LVGL, told that it is out already, renders the next one in the
       * other: all it does with this one meanwhile is read what changed in it. It is only
       * waited for here, before the next one is sent.
       */
      if (s_sending.data != nullptr) {
        (void)pbl_sem_take(&s_sent, PBL_MSEC(FLUSH_WAIT_MS));
      }
      s_sending = (struct pbl_display_buffer){.data = px_map, .pitch = s_fb.pitch};
      pbl_sem_reset(&s_sent);
      ret = pbl_display_update(DISPLAY, &s_sending, y0, y1, prv_sent, nullptr);
      if (ret != 0) {
        s_sending.data = nullptr;
      }
    } else {
      ret = pbl_display_update_sync(DISPLAY, &s_fb, y0, y1);
    }

    if (ret != 0) {
      PBL_LOG_ERR("lvgl: display update failed (%d)", ret);
    }
  }

  lv_display_flush_ready(disp);
}

static int prv_display_init(void) {
  const struct pbl_display_props *props = pbl_display_get_props(DISPLAY);
  lv_color_format_t format = LV_COLOR_FORMAT_RGB565;
  uint32_t buf_size;
  void *buf = nullptr;

  s_format = props->format;

  /*
   * LVGL renders in the format of the display where it has it, and in RGB565, converted as it
   * is flushed, where it does not.
   */
  if (s_format == PBL_DISPLAY_FORMAT_MONO) {
#if LV_DRAW_SW_SUPPORT_I1
    format = LV_COLOR_FORMAT_I1;
#else
    return -ENOTSUP;
#endif
  }
  buf_size = lv_draw_buf_width_to_stride(props->width, format) * CONFIG_LVGL_DRAW_BUFFER_ROWS;
  if (format == LV_COLOR_FORMAT_I1) {
    buf_size += MONO_PALETTE_SIZE;
  }

  /* A frame the board reserves, or one from the heap. */
  if (pbl_display_get_framebuffer(DISPLAY, &s_fb) != 0) {
    if (s_format == PBL_DISPLAY_FORMAT_MONO) {
      s_fb.pitch = (props->width + 7U) / 8U;
    } else if (s_format == PBL_DISPLAY_FORMAT_RGB565) {
      s_fb.pitch = props->width * sizeof(uint16_t);
    } else {
      s_fb.pitch = props->width;
    }
    s_fb.data = pbl_zalloc(s_fb.pitch * props->height);
    s_fb_allocated = true;
  }

  /* LVGL renders in bands that are copied or converted into the frame, or straight into the
   * frame of an RGB565 display. */
  if (s_format != PBL_DISPLAY_FORMAT_RGB565) {
    buf = pbl_malloc(buf_size);
  }
  if (s_fb.data == nullptr || (s_format != PBL_DISPLAY_FORMAT_RGB565 && buf == nullptr)) {
    goto err;
  }

  s_disp = lv_display_create(props->width, props->height);
  if (s_disp == nullptr) {
    goto err;
  }

  s_dirty_y0 = INT32_MAX;
  s_dirty_y1 = 0;

  lv_display_set_color_format(s_disp, format);
#if LV_DRAW_SW_SUPPORT_I1
  if (format == LV_COLOR_FORMAT_I1) {
    lv_display_add_event_cb(s_disp, prv_round_mono, LV_EVENT_INVALIDATE_AREA, nullptr);
  }
#endif
  if (buf != nullptr) {
    lv_display_set_buffers(s_disp, buf, nullptr, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
  } else {
    /* A second frame, where there is memory for one, is rendered while the first is sent. */
    void *second = pbl_scratch_zalloc(s_fb.pitch * props->height);

    s_two_frames = second != nullptr;
    lv_display_set_buffers(s_disp, s_fb.data, second, s_fb.pitch * props->height,
                           LV_DISPLAY_RENDER_MODE_DIRECT);
  }
  lv_display_set_flush_cb(s_disp, prv_flush);

#if LV_USE_THEME_MONO
  if (s_format == PBL_DISPLAY_FORMAT_MONO) {
    lv_display_set_theme(s_disp, lv_theme_mono_init(s_disp, false, LV_FONT_DEFAULT));
  }
#endif

  return 0;

err:
  pbl_free(buf);
  if (s_fb_allocated) {
    pbl_free(s_fb.data);
  }
  s_fb.data = nullptr;

  return -ENOMEM;
}

#ifdef CONFIG_INPUT
static PBL_MSGQ_DEFINE(s_keys, sizeof(struct pbl_lvgl__key), CONFIG_LVGL_KEY_QUEUE_LEN);
static atomic_uint s_touch;

/* May run in an interrupt. */
static void prv_input_cb(const struct pbl_input_event *evt, void *user_data) {
  static uint32_t s_pending;

  if (evt->type == PBL_INPUT_EV_KEY && evt->code != PBL_INPUT_BTN_TOUCH) {
    struct pbl_lvgl__key key = {.code = evt->code, .pressed = evt->value != 0};

    (void)pbl_msgq_put(&s_keys, &key, PBL_NO_WAIT);
    return;
  }

  if (evt->type == PBL_INPUT_EV_KEY) {
    s_pending = (evt->value != 0) ? (s_pending | TOUCH_DOWN) : (s_pending & ~TOUCH_DOWN);
  } else if (evt->type == PBL_INPUT_EV_ABS && evt->code == PBL_INPUT_ABS_X) {
    s_pending = (s_pending & ~(0x7FFFU << 15)) | (((uint32_t)evt->value & 0x7FFFU) << 15);
  } else if (evt->type == PBL_INPUT_EV_ABS && evt->code == PBL_INPUT_ABS_Y) {
    s_pending = (s_pending & ~0x7FFFU) | ((uint32_t)evt->value & 0x7FFFU);
  }

  if (evt->sync) {
    atomic_store(&s_touch, s_pending);
  }
}

PBL_INPUT_CALLBACK_DEFINE(prv_input_cb, nullptr);

static void prv_keypad_read(lv_indev_t *indev, lv_indev_data_t *data) {
  static uint32_t s_last_key;
  struct pbl_lvgl__key key;

  data->key = s_last_key;
  data->state = LV_INDEV_STATE_RELEASED;

  if (pbl_msgq_get(&s_keys, &key, PBL_NO_WAIT) != 0) {
    return;
  }

  switch (key.code) {
    case PBL_INPUT_KEY_UP:
      data->key = LV_KEY_PREV;
      break;
    case PBL_INPUT_KEY_DOWN:
      data->key = LV_KEY_NEXT;
      break;
    case PBL_INPUT_KEY_SELECT:
      data->key = LV_KEY_ENTER;
      break;
    case PBL_INPUT_KEY_BACK:
      data->key = LV_KEY_ESC;
      break;
    default:
      return;
  }

  s_last_key = data->key;
  data->state = key.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  data->continue_reading = pbl_msgq_num_used(&s_keys) > 0U;
}

static int prv_keypad_init(void) {
  lv_group_t *group = lv_group_create();

  s_keypad = lv_indev_create();
  if (group == nullptr || s_keypad == nullptr) {
    return -ENOMEM;
  }

  lv_group_set_default(group);
  lv_indev_set_type(s_keypad, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_read_cb(s_keypad, prv_keypad_read);
  lv_indev_set_group(s_keypad, group);

  return 0;
}
#endif /* CONFIG_INPUT */

#ifdef CONFIG_TOUCH
static void prv_pointer_read(lv_indev_t *indev, lv_indev_data_t *data) {
  uint32_t touch = atomic_load(&s_touch);

  data->point.x = (int32_t)TOUCH_X(touch);
  data->point.y = (int32_t)TOUCH_Y(touch);
  data->state = (touch & TOUCH_DOWN) != 0U ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static int prv_pointer_init(void) {
  int ret;

  s_pointer = lv_indev_create();
  if (s_pointer == nullptr) {
    return -ENOMEM;
  }

  lv_indev_set_type(s_pointer, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(s_pointer, prv_pointer_read);

  ret = pbl_touch_enable(TOUCH);
  if (ret != 0) {
    return ret;
  }

  return 0;
}
#endif /* CONFIG_TOUCH */

int pbl_lvgl_init(void) {
  int ret;

  lv_init();
  lv_tick_set_cb(prv_tick);

#ifdef CONFIG_LVGL_DRAW_GPU
  pbl_lvgl__draw_gpu_init();
#endif

  ret = prv_display_init();
  if (ret != 0) {
    return ret;
  }

#ifdef CONFIG_INPUT
  ret = prv_keypad_init();
  if (ret != 0) {
    return ret;
  }
#endif

#ifdef CONFIG_TOUCH
  ret = prv_pointer_init();
  if (ret != 0) {
    return ret;
  }
#endif

  return 0;
}

lv_display_t *pbl_lvgl_get_display(void) {
  return s_disp;
}

lv_indev_t *pbl_lvgl_get_pointer(void) {
  return s_pointer;
}

lv_indev_t *pbl_lvgl_get_keypad(void) {
  return s_keypad;
}
