/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/display/stm32_ltdc.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/st/idle.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/stm32_ltdc.h>

// The shortest blanking, as ST's: the panel only takes frames into its frame memory
#define HSYNC_LEN    1U
#define HBACK_PORCH  2U
#define HFRONT_PORCH 1U
#define VSYNC_LEN    1U
#define VBACK_PORCH  2U
#define VFRONT_PORCH 1U

static const struct pbl_display_stm32_ltdc *prv_ltdc(const struct pbl_display *disp) {
  return PBL_CONTAINER_OF(disp, const struct pbl_display_stm32_ltdc, disp);
}

static inline void prv_putreg(const struct pbl_display_stm32_ltdc *ltdc, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, ltdc->base + offset);
}

static uint32_t prv_bpp(enum pbl_display_format format) {
  return format == PBL_DISPLAY_FORMAT_RGB565 ? 2U : 0U;
}

static uint32_t prv_pitch(const struct pbl_display_stm32_ltdc *ltdc) {
  return (uint32_t)ltdc->disp.props.width * prv_bpp(ltdc->disp.props.format);
}

// The timing registers hold accumulated positions, each one less than the count
static uint32_t prv_hv(uint32_t h, uint32_t v) {
  return PBL_FIELD_PREP(LTDC_H_MASK, h - 1U) | PBL_FIELD_PREP(LTDC_V_MASK, v - 1U);
}

// The layer covers the whole active area, @p width by @p rows pixels from @p first, out of a frame
// as wide as the display
static void prv_area(const struct pbl_display_stm32_ltdc *ltdc, const uint8_t *first,
                     uint32_t width, uint32_t rows) {
  const uint32_t hbp = HSYNC_LEN + HBACK_PORCH;
  const uint32_t vbp = VSYNC_LEN + VBACK_PORCH;

  prv_putreg(ltdc, STM32_LTDC_SSCR_OFFSET, prv_hv(HSYNC_LEN, VSYNC_LEN));
  prv_putreg(ltdc, STM32_LTDC_BPCR_OFFSET, prv_hv(hbp, vbp));
  prv_putreg(ltdc, STM32_LTDC_AWCR_OFFSET, prv_hv(hbp + width, vbp + rows));
  prv_putreg(ltdc, STM32_LTDC_TWCR_OFFSET,
             prv_hv(hbp + width + HFRONT_PORCH, vbp + rows + VFRONT_PORCH));

  prv_putreg(ltdc, STM32_LTDC_L1WHPCR_OFFSET,
             PBL_FIELD_PREP(LTDC_LWHPCR_WHSTPOS_MASK, hbp) |
                 PBL_FIELD_PREP(LTDC_LWHPCR_WHSPPOS_MASK, hbp + width - 1U));
  prv_putreg(ltdc, STM32_LTDC_L1WVPCR_OFFSET,
             PBL_FIELD_PREP(LTDC_LWVPCR_WVSTPOS_MASK, vbp) |
                 PBL_FIELD_PREP(LTDC_LWVPCR_WVSPPOS_MASK, vbp + rows - 1U));
  prv_putreg(ltdc, STM32_LTDC_L1CFBAR_OFFSET, (uint32_t)(uintptr_t)first);
  prv_putreg(ltdc, STM32_LTDC_L1CFBLR_OFFSET,
             PBL_FIELD_PREP(LTDC_LCFBLR_CFBP_MASK, prv_pitch(ltdc)) |
                 PBL_FIELD_PREP(LTDC_LCFBLR_CFBLL_MASK, width * prv_bpp(ltdc->disp.props.format) +
                                                            LTDC_LCFBLR_CFBLL_EXTRA));
  prv_putreg(ltdc, STM32_LTDC_L1CFBLNR_OFFSET, PBL_FIELD_PREP(LTDC_LCFBLNR_CFBLNBR_MASK, rows));
  prv_putreg(ltdc, STM32_LTDC_SRCR_OFFSET, LTDC_SRCR_IMR);
}

static void prv_layer_init(const struct pbl_display_stm32_ltdc *ltdc) {
  prv_putreg(ltdc, STM32_LTDC_BCCR_OFFSET, 0U);
  prv_putreg(ltdc, STM32_LTDC_L1PFCR_OFFSET,
             PBL_FIELD_PREP(LTDC_LPFCR_PF_MASK, LTDC_LPFCR_PF_RGB565));
  prv_putreg(ltdc, STM32_LTDC_L1CACR_OFFSET, LTDC_LCACR_OPAQUE);
  prv_putreg(ltdc, STM32_LTDC_L1DCCR_OFFSET, 0U);
  prv_putreg(ltdc, STM32_LTDC_L1BFCR_OFFSET,
             PBL_FIELD_PREP(LTDC_LBFCR_BF1_MASK, LTDC_LBFCR_BF1_CA) |
                 PBL_FIELD_PREP(LTDC_LBFCR_BF2_MASK, LTDC_LBFCR_BF2_1_CA));
  prv_putreg(ltdc, STM32_LTDC_L1CR_OFFSET, LTDC_LCR_LEN);
}

// Each half of the rows of an update goes out on a refresh: the bridge holds while the
// controller is set up for it
static void prv_half(const struct pbl_display_stm32_ltdc *ltdc, bool right) {
  struct pbl_display_stm32_ltdc_state *st = ltdc->state;
  const uint32_t half = ltdc->disp.props.width / 2U;
  const uint8_t *first = st->frame + (size_t)st->y0 * prv_pitch(ltdc) +
                         (right ? half * prv_bpp(ltdc->disp.props.format) : 0U);

  pbl_display_bridge_hold(ltdc->bridge, true);
  prv_area(ltdc, first, half, (uint32_t)(st->y1 - st->y0) + 1U);
  pbl_display_bridge_hold(ltdc->bridge, false);
}

static int prv_window(const struct pbl_display_stm32_ltdc *ltdc, bool right) {
  struct pbl_display_stm32_ltdc_state *st = ltdc->state;
  const uint16_t half = ltdc->disp.props.width / 2U;

  return right ? pbl_panel_set_window(ltdc->panel, half, st->y0, 2U * half - 1U, st->y1)
               : pbl_panel_set_window(ltdc->panel, 0U, st->y0, half - 1U, st->y1);
}

// Stop, which stops the clocks of the controller and the bridge, is kept off while an update runs
static void prv_busy(struct pbl_display_stm32_ltdc_state *st) {
  st->stage = PBL_DISPLAY_STM32_LTDC_TEAR;
  pbl_idle_state_block(SOC_STM32_IDLE_STOP);
}

static void prv_idle(struct pbl_display_stm32_ltdc_state *st) {
  if (st->stage != PBL_DISPLAY_STM32_LTDC_IDLE) {
    st->stage = PBL_DISPLAY_STM32_LTDC_IDLE;
    pbl_idle_state_release(SOC_STM32_IDLE_STOP);
  }
}

// The first half is written behind the scan of the panel from its tearing effect
static int prv_start(const struct pbl_display_stm32_ltdc *ltdc, uint8_t *frame, uint16_t y0,
                     uint16_t y1, bool notify) {
  struct pbl_display_stm32_ltdc_state *st = ltdc->state;
  int ret;

  st->frame = frame;
  st->y0 = y0;
  st->y1 = y1;
  st->notify = notify;

  // A whole frame is more than the data cache holds
  pbl_dcache_flush_all();

  prv_half(ltdc, st->right_first);
  ret = prv_window(ltdc, st->right_first);
  if (ret != 0) {
    return ret;
  }

  return pbl_display_bridge_request_tear(ltdc->bridge);
}

// Ends the update being sent, then starts the one asked for meanwhile
static void prv_end(const struct pbl_display_stm32_ltdc *ltdc, int status) {
  struct pbl_display_stm32_ltdc_state *st = ltdc->state;

  while (true) {
    uint8_t *frame = st->queued;
    const bool notify = st->notify;

    st->queued = nullptr;
    if (frame == nullptr) {
      prv_idle(st);
    } else {
      st->stage = PBL_DISPLAY_STM32_LTDC_TEAR;
    }

    if (notify) {
      pbl_display_complete(&ltdc->disp, status);
    }

    if (frame == nullptr) {
      return;
    }

    status = prv_start(ltdc, frame, st->queued_y0, st->queued_y1, st->queued_notify);
    if (status == 0) {
      return;
    }
  }
}

static int prv_send_second(const struct pbl_display_stm32_ltdc *ltdc) {
  struct pbl_display_stm32_ltdc_state *st = ltdc->state;
  int ret;

  prv_half(ltdc, !st->right_first);
  ret = prv_window(ltdc, !st->right_first);
  if (ret != 0) {
    return ret;
  }

  st->stage = PBL_DISPLAY_STM32_LTDC_SECOND;

  return pbl_display_bridge_refresh(ltdc->bridge);
}

// The rows of an update go to the frame memory of the panel in two halves, each written while
// the scan of the panel is in the other: the first from the tearing effect, the second once the
// first is out. A step that fails ends the update
static void prv_bridge_event(enum pbl_display_bridge_event event, void *arg) {
  const struct pbl_display_stm32_ltdc *ltdc = arg;
  struct pbl_display_stm32_ltdc_state *st = ltdc->state;
  int ret = 0;

  if (event == PBL_DISPLAY_BRIDGE_EVENT_TEAR && st->stage == PBL_DISPLAY_STM32_LTDC_TEAR) {
    st->stage = PBL_DISPLAY_STM32_LTDC_FIRST;
    ret = pbl_display_bridge_refresh(ltdc->bridge);
  } else if (event == PBL_DISPLAY_BRIDGE_EVENT_REFRESH_DONE &&
             st->stage == PBL_DISPLAY_STM32_LTDC_FIRST) {
    ret = prv_send_second(ltdc);
  } else if (event == PBL_DISPLAY_BRIDGE_EVENT_REFRESH_DONE &&
             st->stage == PBL_DISPLAY_STM32_LTDC_SECOND) {
    prv_end(ltdc, 0);
    return;
  }

  if (ret != 0) {
    prv_end(ltdc, ret);
  }
}

// An update asked for while another is being sent follows it, from its frame, with the rows of
// both: the newest frame has them all
static int prv_refresh(const struct pbl_display_stm32_ltdc *ltdc, uint8_t *frame, uint16_t y0,
                       uint16_t y1, bool notify) {
  struct pbl_display_stm32_ltdc_state *st = ltdc->state;
  int ret;

  pbl_irq_lock();
  if (st->stage != PBL_DISPLAY_STM32_LTDC_IDLE) {
    if (st->queued != nullptr) {
      y0 = PBL_MIN(y0, st->queued_y0);
      y1 = PBL_MAX(y1, st->queued_y1);
      notify = notify || st->queued_notify;
    }
    st->queued = frame;
    st->queued_y0 = y0;
    st->queued_y1 = y1;
    st->queued_notify = notify;
    pbl_irq_unlock();
    return 0;
  }
  prv_busy(st);
  pbl_irq_unlock();

  ret = prv_start(ltdc, frame, y0, y1, notify);
  if (ret != 0) {
    pbl_irq_lock();
    prv_idle(st);
    pbl_irq_unlock();
  }

  return ret;
}

// A frame turned by 90 degrees is the mirror of one turned by 270, its right half being written
// first behind the scan
static int prv_set_rotated(const struct pbl_display *disp, bool rotated) {
  const struct pbl_display_stm32_ltdc *ltdc = prv_ltdc(disp);
  const uint16_t deg = (ltdc->rotation + (rotated ? 180U : 0U)) % 360U;
  int ret;

  ret = pbl_panel_set_rotation(ltdc->panel, PBL_PANEL_ROTATION(deg));
  if (ret != 0) {
    return ret;
  }

  ltdc->state->right_first = deg == 90U;

  return 0;
}

static int prv_enable(const struct pbl_display *disp) {
  return pbl_panel_enable(prv_ltdc(disp)->panel);
}

static int prv_init(const struct pbl_display *disp) {
  const struct pbl_display_stm32_ltdc *ltdc = prv_ltdc(disp);
  const struct pbl_panel *panel = ltdc->panel;
  int ret;

  if (ltdc->bridge == nullptr || disp->framebuffer == nullptr ||
      prv_bpp(disp->props.format) == 0U ||
      panel->bus_format != PBL_PANEL_BUS_FORMAT_MIPI_DSI_CMD_RGB888) {
    return -ENOTSUP;
  }

  ret = pbl_device_init(&panel->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_device_init(&ltdc->bridge->dev);
  if (ret != 0) {
    return ret;
  }

  ret = prv_set_rotated(disp, false);
  if (ret != 0) {
    return ret;
  }

  if (disp->props.width > panel->timing->hactive || disp->props.height > panel->timing->vactive) {
    return -ENOTSUP;
  }

  ret = pbl_device_init(&ltdc->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&ltdc->clk);
  if (ret != 0) {
    return ret;
  }

  // Active high syncs and active low data enable, as the DSI wrapper takes them. The controller
  // starts once the panel is set up, as ST starts it
  prv_layer_init(ltdc);
  prv_area(ltdc, disp->framebuffer, disp->props.width / 2U, disp->props.height);
  prv_putreg(ltdc, STM32_LTDC_GCR_OFFSET, LTDC_GCR_HSPOL | LTDC_GCR_VSPOL);

  ret = pbl_panel_prepare(panel);
  if (ret != 0) {
    return ret;
  }

  pbl_modifyl(0U, LTDC_GCR_LTDCEN, ltdc->base + STM32_LTDC_GCR_OFFSET);

  ret = pbl_display_bridge_set_event_cb(ltdc->bridge, prv_bridge_event, (void *)ltdc);
  if (ret != 0) {
    return ret;
  }

  ret = prv_enable(disp);
  if (ret != 0) {
    return ret;
  }

  // The panel gets the frame before the first update
  return prv_refresh(ltdc, disp->framebuffer, 0U, disp->props.height - 1U, false);
}

static int prv_disable(const struct pbl_display *disp) {
  return pbl_panel_disable(prv_ltdc(disp)->panel);
}

// Updates are sent to the panel straight from the buffer given
static int prv_update(const struct pbl_display *disp, const struct pbl_display_buffer *buf,
                      uint16_t y0, uint16_t y1) {
  const struct pbl_display_stm32_ltdc *ltdc = prv_ltdc(disp);

  if (buf->pitch != prv_pitch(ltdc)) {
    return -EINVAL;
  }

  return prv_refresh(ltdc, buf->data, y0, y1, true);
}

// Events of the update stopped are ignored from then on
static void prv_abort(const struct pbl_display *disp) {
  struct pbl_display_stm32_ltdc_state *st = prv_ltdc(disp)->state;

  st->queued = nullptr;
  prv_idle(st);
}

const struct pbl_display_ops pbl_display_stm32_ltdc_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .set_rotated = prv_set_rotated,
  .update = prv_update,
  .abort = prv_abort,
};

#include PBL_DT_INSTANCES(st_stm32_ltdc)
