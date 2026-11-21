/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/display/stm32_ltdc.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/st/idle.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/stm32_ltdc.h>

#define BYTES_PER_PIXEL 2U

static const struct pbl_display_stm32_ltdc *prv_ltdc(const struct pbl_display *disp) {
  return PBL_CONTAINER_OF(disp, const struct pbl_display_stm32_ltdc, disp);
}

static inline uint32_t prv_getreg(const struct pbl_display_stm32_ltdc *ltdc, uint32_t offset) {
  return pbl_readl(ltdc->base + offset);
}

static inline void prv_putreg(const struct pbl_display_stm32_ltdc *ltdc, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, ltdc->base + offset);
}

// The timing registers hold accumulated positions, each one less than the count
static uint32_t prv_hv(uint32_t h, uint32_t v) {
  return PBL_FIELD_PREP(LTDC_H_MASK, h - 1U) | PBL_FIELD_PREP(LTDC_V_MASK, v - 1U);
}

static void prv_timing(const struct pbl_display_stm32_ltdc *ltdc,
                       const struct pbl_panel_timing *t) {
  uint32_t hsync = t->hsync_len;
  uint32_t vsync = t->vsync_len;
  uint32_t hbp = hsync + t->hback_porch;
  uint32_t vbp = vsync + t->vback_porch;
  uint32_t hact = hbp + t->hactive;
  uint32_t vact = vbp + t->vactive;

  prv_putreg(ltdc, STM32_LTDC_SSCR_OFFSET, prv_hv(hsync, vsync));
  prv_putreg(ltdc, STM32_LTDC_BPCR_OFFSET, prv_hv(hbp, vbp));
  prv_putreg(ltdc, STM32_LTDC_AWCR_OFFSET, prv_hv(hact, vact));
  prv_putreg(ltdc, STM32_LTDC_TWCR_OFFSET, prv_hv(hact + t->hfront_porch, vact + t->vfront_porch));
  prv_putreg(ltdc, STM32_LTDC_BCCR_OFFSET, 0U);
}

// The layer covers the active area of the timing, out of a frame as wide as the display
static void prv_layer(const struct pbl_display_stm32_ltdc *ltdc, const struct pbl_panel_timing *t) {
  const struct pbl_display *disp = &ltdc->disp;
  uint32_t hstart = (uint32_t)t->hsync_len + t->hback_porch;
  uint32_t vstart = (uint32_t)t->vsync_len + t->vback_porch;
  uint32_t pitch = (uint32_t)disp->props.width * BYTES_PER_PIXEL;
  uint32_t len = (uint32_t)t->hactive * BYTES_PER_PIXEL;

  prv_putreg(ltdc, STM32_LTDC_L1WHPCR_OFFSET,
             PBL_FIELD_PREP(LTDC_LWHPCR_WHSTPOS_MASK, hstart) |
                 PBL_FIELD_PREP(LTDC_LWHPCR_WHSPPOS_MASK, hstart + t->hactive - 1U));
  prv_putreg(ltdc, STM32_LTDC_L1WVPCR_OFFSET,
             PBL_FIELD_PREP(LTDC_LWVPCR_WVSTPOS_MASK, vstart) |
                 PBL_FIELD_PREP(LTDC_LWVPCR_WVSPPOS_MASK, vstart + disp->props.height - 1U));
  prv_putreg(ltdc, STM32_LTDC_L1PFCR_OFFSET,
             PBL_FIELD_PREP(LTDC_LPFCR_PF_MASK, LTDC_LPFCR_PF_RGB565));
  prv_putreg(ltdc, STM32_LTDC_L1CACR_OFFSET, LTDC_LCACR_OPAQUE);
  prv_putreg(ltdc, STM32_LTDC_L1DCCR_OFFSET, 0U);
  prv_putreg(ltdc, STM32_LTDC_L1BFCR_OFFSET,
             PBL_FIELD_PREP(LTDC_LBFCR_BF1_MASK, LTDC_LBFCR_BF1_CA) |
                 PBL_FIELD_PREP(LTDC_LBFCR_BF2_MASK, LTDC_LBFCR_BF2_1_CA));
  prv_putreg(ltdc, STM32_LTDC_L1CFBAR_OFFSET, (uint32_t)(uintptr_t)disp->framebuffer);
  prv_putreg(ltdc, STM32_LTDC_L1CFBLR_OFFSET,
             PBL_FIELD_PREP(LTDC_LCFBLR_CFBP_MASK, pitch) |
                 PBL_FIELD_PREP(LTDC_LCFBLR_CFBLL_MASK, len + LTDC_LCFBLR_CFBLL_EXTRA));
  prv_putreg(ltdc, STM32_LTDC_L1CFBLNR_OFFSET,
             PBL_FIELD_PREP(LTDC_LCFBLNR_CFBLNBR_MASK, disp->props.height));
  prv_putreg(ltdc, STM32_LTDC_L1CR_OFFSET, LTDC_LCR_LEN);
}

static int prv_enable(const struct pbl_display *disp);

static bool prv_command_mode(const struct pbl_display_stm32_ltdc *ltdc) {
  return ltdc->host != nullptr &&
         ltdc->panel->bus_format == PBL_PANEL_BUS_FORMAT_MIPI_DSI_CMD_RGB888;
}

// The DSI host holds the controller while its source changes
static void prv_source(const struct pbl_display_stm32_ltdc *ltdc, const uint8_t *first) {
  (void)pbl_mipi_dsi_hold(ltdc->host, true);
  prv_putreg(ltdc, STM32_LTDC_L1CFBAR_OFFSET, (uint32_t)(uintptr_t)first);
  prv_putreg(ltdc, STM32_LTDC_SRCR_OFFSET, LTDC_SRCR_IMR);
  (void)pbl_mipi_dsi_hold(ltdc->host, false);
}

static void prv_start(const struct pbl_display_stm32_ltdc *ltdc, uint8_t *frame, bool notify) {
  struct pbl_display_stm32_ltdc_state *st = ltdc->state;

  // A whole frame is more than the data cache holds
  pbl_dcache_flush_all();

  st->frame = frame;
  st->notify = notify;
  st->stage = PBL_DISPLAY_STM32_LTDC_TEAR;
  prv_source(ltdc, frame);
  (void)pbl_panel_set_tear(ltdc->panel, true);
}

// In command mode a frame goes to the frame memory of the panel in two halves, left then right,
// each written while the scan of the panel is in the other: the first from the tearing effect,
// the second once the first is out
static void prv_dsi_event(enum pbl_mipi_dsi_event event, void *arg) {
  const struct pbl_display_stm32_ltdc *ltdc = arg;
  struct pbl_display_stm32_ltdc_state *st = ltdc->state;
  const uint16_t w = ltdc->disp.props.width;
  const uint16_t h = ltdc->disp.props.height;

  if (event == PBL_MIPI_DSI_EVENT_TEAR && st->stage == PBL_DISPLAY_STM32_LTDC_TEAR) {
    (void)pbl_panel_set_tear(ltdc->panel, false);
    st->stage = PBL_DISPLAY_STM32_LTDC_LEFT;
    (void)pbl_mipi_dsi_refresh(ltdc->host);
  } else if (event == PBL_MIPI_DSI_EVENT_REFRESH_DONE && st->stage == PBL_DISPLAY_STM32_LTDC_LEFT) {
    prv_source(ltdc, st->frame + (size_t)(w / 2U) * BYTES_PER_PIXEL);
    (void)pbl_panel_set_window(ltdc->panel, w / 2U, 0U, w - 1U, h - 1U);
    st->stage = PBL_DISPLAY_STM32_LTDC_RIGHT;
    (void)pbl_mipi_dsi_refresh(ltdc->host);
  } else if (event == PBL_MIPI_DSI_EVENT_REFRESH_DONE &&
             st->stage == PBL_DISPLAY_STM32_LTDC_RIGHT) {
    prv_source(ltdc, st->frame);
    (void)pbl_panel_set_window(ltdc->panel, 0U, 0U, w / 2U - 1U, h - 1U);
    st->stage = PBL_DISPLAY_STM32_LTDC_IDLE;
    if (st->notify) {
      pbl_display_complete(&ltdc->disp, 0);
    }
    if (st->queued != nullptr) {
      uint8_t *frame = st->queued;

      st->queued = nullptr;
      prv_start(ltdc, frame, st->queued_notify);
    }
  }
}

// A frame asked for while another is being sent follows it
static int prv_refresh(const struct pbl_display_stm32_ltdc *ltdc, uint8_t *frame, bool notify) {
  struct pbl_display_stm32_ltdc_state *st = ltdc->state;

  pbl_irq_lock();
  if (st->stage != PBL_DISPLAY_STM32_LTDC_IDLE) {
    st->queued = frame;
    st->queued_notify = notify;
    pbl_irq_unlock();
    return 0;
  }
  st->stage = PBL_DISPLAY_STM32_LTDC_TEAR;
  pbl_irq_unlock();

  prv_start(ltdc, frame, notify);

  return 0;
}

static int prv_init(const struct pbl_display *disp) {
  const struct pbl_display_stm32_ltdc *ltdc = prv_ltdc(disp);
  const struct pbl_panel *panel = ltdc->panel;
  const struct pbl_panel_timing *t = panel->timing;
  int ret;

  ret = pbl_device_init(&panel->dev);
  if (ret != 0) {
    return ret;
  }

  // A frame wider than the panel, and as wide as it is tall, goes to it turned a quarter turn
  if (disp->props.width > t->hactive && disp->props.width <= t->vactive) {
    ret = pbl_panel_set_quarter_turn(panel, true);
    if (ret != 0) {
      return ret;
    }
  }

  if (disp->framebuffer == nullptr || disp->props.format != PBL_DISPLAY_FORMAT_RGB565 ||
      (panel->bus_format != PBL_PANEL_BUS_FORMAT_MIPI_DSI_RGB888 && !prv_command_mode(ltdc)) ||
      disp->props.width > t->hactive || disp->props.height > t->vactive) {
    return -ENOTSUP;
  }

  // In command mode the controller sends half of the frame on each refresh
  struct pbl_panel_timing lt = *t;

  if (prv_command_mode(ltdc)) {
    lt.hactive = t->hactive / 2U;
  }

  ret = pbl_device_init(&ltdc->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&ltdc->clk);
  if (ret != 0) {
    return ret;
  }

  ltdc->state->scanout = disp->framebuffer;
  prv_timing(ltdc, &lt);
  prv_layer(ltdc, &lt);
  prv_putreg(ltdc, STM32_LTDC_SRCR_OFFSET, LTDC_SRCR_IMR);

  // The DSI host takes the data enable inverted and active high syncs in video mode, all active
  // low in command mode
  prv_putreg(ltdc, STM32_LTDC_GCR_OFFSET,
             (prv_command_mode(ltdc) ? 0U : LTDC_GCR_HSPOL | LTDC_GCR_VSPOL) | LTDC_GCR_LTDCEN);
  pbl_irq_enable(ltdc->irq);

  ret = pbl_panel_prepare(panel);
  if (ret != 0) {
    return ret;
  }

  if (prv_command_mode(ltdc)) {
    ret = pbl_mipi_dsi_set_event_cb(ltdc->host, prv_dsi_event, (void *)ltdc);
    if (ret != 0) {
      return ret;
    }

    ret = pbl_panel_set_window(panel, 0U, 0U, disp->props.width / 2U - 1U, disp->props.height - 1U);
    if (ret != 0) {
      return ret;
    }
  }

  ret = prv_enable(disp);
  if (ret != 0) {
    return ret;
  }

  // The panel gets the frame before the first update
  return prv_command_mode(ltdc) ? prv_refresh(ltdc, disp->framebuffer, false) : 0;
}

// The controller scans the frame while the panel shows it, which Stop would stop
static int prv_enable(const struct pbl_display *disp) {
  const struct pbl_display_stm32_ltdc *ltdc = prv_ltdc(disp);
  int ret;

  if (ltdc->state->enabled) {
    return 0;
  }

  ret = pbl_panel_enable(ltdc->panel);
  if (ret != 0) {
    return ret;
  }

  pbl_idle_state_block(SOC_STM32_IDLE_STOP);
  ltdc->state->enabled = true;

  return 0;
}

static int prv_disable(const struct pbl_display *disp) {
  const struct pbl_display_stm32_ltdc *ltdc = prv_ltdc(disp);
  int ret;

  if (!ltdc->state->enabled) {
    return 0;
  }

  ret = pbl_panel_disable(ltdc->panel);
  if (ret != 0) {
    return ret;
  }

  pbl_idle_state_release(SOC_STM32_IDLE_STOP);
  ltdc->state->enabled = false;

  return 0;
}

static int prv_set_rotated(const struct pbl_display *disp, bool rotated) {
  return rotated ? -ENOTSUP : 0;
}

// Rows of another buffer are copied into the frame scanned out; in command mode, the panel is
// sent the whole frame of the buffer
static int prv_update(const struct pbl_display *disp, const struct pbl_display_buffer *buf,
                      uint16_t y0, uint16_t y1) {
  uint8_t *scanout = prv_ltdc(disp)->state->scanout;
  uint32_t pitch = (uint32_t)disp->props.width * BYTES_PER_PIXEL;

  if (prv_command_mode(prv_ltdc(disp))) {
    if (buf->pitch != pitch) {
      return -EINVAL;
    }
    return prv_refresh(prv_ltdc(disp), buf->data, true);
  }

  if (buf->data != scanout) {
    for (uint32_t y = y0; y <= y1; y++) {
      memcpy(scanout + (size_t)y * pitch, buf->data + (size_t)y * buf->pitch, pitch);
    }
  }

  pbl_dcache_flush_range(scanout + (size_t)y0 * pitch, (size_t)(y1 - y0 + 1U) * pitch);
  pbl_display_complete(disp, 0);

  return 0;
}

// The new address is taken in the vertical blanking, where the reload interrupt says it is shown
static int prv_flip(const struct pbl_display *disp, const struct pbl_display_buffer *buf) {
  const struct pbl_display_stm32_ltdc *ltdc = prv_ltdc(disp);

  if (buf->pitch != (uint32_t)disp->props.width * BYTES_PER_PIXEL) {
    return -EINVAL;
  }

  if (prv_command_mode(ltdc)) {
    return prv_refresh(ltdc, buf->data, true);
  }

  // A whole frame is more than the data cache holds
  pbl_dcache_flush_all();

  ltdc->state->next = buf->data;
  prv_putreg(ltdc, STM32_LTDC_L1CFBAR_OFFSET, (uint32_t)(uintptr_t)buf->data);
  prv_putreg(ltdc, STM32_LTDC_ICR_OFFSET, LTDC_IRQ_RR);
  prv_putreg(ltdc, STM32_LTDC_IER_OFFSET, LTDC_IRQ_RR);
  prv_putreg(ltdc, STM32_LTDC_SRCR_OFFSET, LTDC_SRCR_VBR);

  return 0;
}

void pbl_display_stm32_ltdc_irq_handler(const struct pbl_display_stm32_ltdc *ltdc) {
  if ((prv_getreg(ltdc, STM32_LTDC_ISR_OFFSET) & LTDC_IRQ_RR) == 0U) {
    return;
  }

  prv_putreg(ltdc, STM32_LTDC_ICR_OFFSET, LTDC_IRQ_RR);
  prv_putreg(ltdc, STM32_LTDC_IER_OFFSET, 0U);
  ltdc->state->scanout = ltdc->state->next;
  pbl_display_complete(&ltdc->disp, 0);
}

const struct pbl_display_ops pbl_display_stm32_ltdc_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .set_rotated = prv_set_rotated,
  .update = prv_update,
  .flip = prv_flip,
};

#include PBL_DT_INSTANCES(st_stm32_ltdc)
