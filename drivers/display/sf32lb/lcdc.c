/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/display/sf32lb_lcdc.h>
#include <pbl/drivers/panel.h>
#include <pbl/drivers/rtc.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/irq.h>
#include <pbl/logging/logging.h>
#include <pbl/mcu/cache.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/soc/sf32lb/sleep.h>
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DEFINE(driver_display_sf32lb_lcdc, CONFIG_DRIVER_DISPLAY_LOG_LEVEL);

// Timeout for detecting the SiFli HAL silent-loss bug: the LCDC kicks off a
// transfer but never delivers the EOF callback, so the compositor would block
// forever. A normal full-frame transfer takes well under 20ms; 500ms is a
// conservative threshold that won't fire on transient stalls but catches a
// real wedge well before the user perceives the freeze.
#define DISPLAY_SILENT_LOSS_TIMEOUT_MS 500

// Bytes of the LCDC peripheral register file snapshotted into BSS before
// crashing. The PebbleOS coredump captures all of HPSYS SRAM but does not
// capture MMIO; copying registers into SRAM gets them into the coredump that
// gets shared with Sifli. 2KB covers IRQ/SETTING/COMMAND/CANVAS/LAYER0/JDI_PAR
// (lcd_if.h struct ends around offset 0x100).
#define DISPLAY_LCDC_REG_DUMP_BYTES 2048

// The HAL completion callback has no context argument
static const struct pbl_display_sf32lb_lcdc *s_lcdc;

static TimerID s_silent_loss_timer = TIMER_INVALID_ID;
// Generation of the update in flight, and of the last one the EOF callback
// completed. The silent-loss handler only crashes if its generation never
// completed, so a timer left running after a completion is harmless.
static uint32_t s_update_gen;
static volatile uint32_t s_completed_gen;
// Captured at silent-loss time so the coredump shipped to Sifli carries the
// LCDC register state at the moment of the wedge. uint32_t storage (not
// uint8_t) so the linker word-aligns it — prv_snapshot_lcdc_regs reads MMIO
// as 32-bit volatiles and would fault on a misaligned destination. Marked
// volatile because the only consumer is the postmortem coredump tool, not
// any C code in this image — without it, the compiler eliminates the stores.
static volatile uint32_t s_lcdc_pre_crash_regs[DISPLAY_LCDC_REG_DUMP_BYTES / sizeof(uint32_t)];

// Ring buffer of recent LCDC interrupts, recorded by the interrupt handler
#define DISPLAY_IRQ_LOG_ENTRIES 32

typedef struct {
  uint32_t timestamp;    // rtc_get_ticks() low 32 bits
  uint32_t irq_before;   // LCD_IF.IRQ as the HAL handler will read it
  uint32_t irq_after;    // LCD_IF.IRQ after the HAL handler cleared bits
  uint32_t jdi_par_ctrl; // LCD_IF.JDI_PAR_CTRL before the handler (INT_LINE_NUM)
  uint32_t status;       // LCD_IF.STATUS before the handler
  uint32_t flags;        // bit0: EOF callback fired; bit1: update in progress at entry
} DisplayIrqLogEntry;

typedef struct {
  uint32_t write_count; // total IRQs logged; newest = (write_count-1) % N
  DisplayIrqLogEntry entries[DISPLAY_IRQ_LOG_ENTRIES];
} DisplayIrqLog;

static volatile DisplayIrqLog s_lcdc_irq_log;

// Set by HAL_LCDC_SendLayerDataCpltCbk (the EOF callback) so the IRQ logger can
// record, per interrupt, whether the HAL reached the completion path.
static volatile bool s_lcdc_eof_cb_fired;

#ifndef CONFIG_RELEASE
// Test hook: arm a one-shot drop of the next LCDC transfer-complete callback,
// simulating the silent-loss failure mode. The silent-loss timer should fire
// ~DISPLAY_SILENT_LOSS_TIMEOUT_MS later and a panic.
static volatile bool s_test_drop_next_complete;
#endif

static const struct pbl_display_sf32lb_lcdc *prv_lcdc(const struct pbl_display *disp) {
  return container_of(disp, const struct pbl_display_sf32lb_lcdc, disp);
}

static int prv_panel_on(const struct pbl_display_sf32lb_lcdc *lcdc) {
  int ret;

  ret = pbl_panel_prepare(lcdc->panel);
  if (ret != 0) {
    return ret;
  }

  return pbl_panel_enable(lcdc->panel);
}

static int prv_panel_off(const struct pbl_display_sf32lb_lcdc *lcdc) {
  int ret;

  HAL_LCDC_DeInit(&lcdc->state->hlcdc);

  ret = pbl_panel_disable(lcdc->panel);
  if (ret != 0) {
    return ret;
  }

  return pbl_panel_unprepare(lcdc->panel);
}

// 222: XX RR GG BB, 332: RR 0G GG BB
static PBL_ALWAYS_INLINE uint32_t prv_to_332(uint32_t p) {
  return ((p & 0x30303030) << 2) | ((p & 0x0C0C0C0C) << 1) | (p & 0x03030303);
}

static PBL_ALWAYS_INLINE uint32_t prv_to_222(uint32_t p) {
  return ((p >> 2) & 0x30303030) | ((p >> 1) & 0x0C0C0C0C) | (p & 0x03030303);
}

static PBL_ALWAYS_INLINE void prv_row_convert(uint32_t *row, uint32_t words,
                                              uint32_t (*conv)(uint32_t)) {
  for (uint32_t i = 0; i < words; i++) {
    row[i] = conv(row[i]);
  }
}

// HMirror is done in software, VMirror by the LCDC
static PBL_ALWAYS_INLINE void prv_row_convert_mirror(uint32_t *row, uint32_t words,
                                                     uint32_t (*conv)(uint32_t)) {
  uint32_t i = 0;
  uint32_t j = words - 1;
  for (; i < j; i++, j--) {
    uint32_t a = row[i];
    uint32_t b = row[j];
    row[i] = PBL_BSWAP32(conv(b));
    row[j] = PBL_BSWAP32(conv(a));
  }
  if (i == j) {
    row[i] = PBL_BSWAP32(conv(row[i]));
  }
}

static void prv_rows_convert(const struct pbl_display_sf32lb_lcdc *lcdc,
                             uint32_t (*conv)(uint32_t)) {
  struct pbl_display_sf32lb_lcdc_state *state = lcdc->state;
  uint32_t words = lcdc->disp.width / 4U;

  for (uint16_t y = state->y0; y <= state->y1; y++) {
    uint32_t *row = (uint32_t *)&state->fb[y * state->pitch];

    if (state->rotated) {
      prv_row_convert_mirror(row, words, conv);
    } else {
      prv_row_convert(row, words, conv);
    }
  }
}

static HAL_StatusTypeDef prv_update_start(const struct pbl_display_sf32lb_lcdc *lcdc) {
  struct pbl_display_sf32lb_lcdc_state *state = lcdc->state;
  uint16_t width = lcdc->disp.width;
  uint16_t height = lcdc->disp.height;
  uint8_t *first = &state->fb[state->y0 * state->pitch];

  uintptr_t fb_addr = (uintptr_t)first;
  size_t fb_size = (size_t)(state->y1 - state->y0 + 1) * state->pitch;
  dcache_align(&fb_addr, &fb_size);
  dcache_flush((const void *)fb_addr, fb_size);

  uint16_t roi_y0 = state->y0;
  uint16_t roi_y1 = state->y1;
  if (state->rotated) {
    roi_y0 = height - 1 - state->y1;
    roi_y1 = height - 1 - state->y0;
  }

  HAL_LCDC_SetROIArea(&state->hlcdc, 0, roi_y0, width - 1, roi_y1);
  HAL_LCDC_LayerSetData(&state->hlcdc, HAL_LCDC_LAYER_DEFAULT, first, 0, roi_y0, width - 1, roi_y1);
  return HAL_LCDC_SendLayerData_IT(&state->hlcdc);
}

static void prv_handle_send_failure(const struct pbl_display_sf32lb_lcdc *lcdc,
                                    HAL_StatusTypeDef status) {
  LCDC_HandleTypeDef *hlcdc = &lcdc->state->hlcdc;

  PBL_LOG_ERR("display: SendLayerData_IT=%d State=%d ErrorCode=0x%08lx", (int)status,
              (int)hlcdc->State, (unsigned long)hlcdc->ErrorCode);
  hlcdc->State = HAL_LCDC_STATE_READY;
  hlcdc->ErrorCode = HAL_LCDC_ERROR_NONE;
}

static void prv_snapshot_lcdc_regs(const LCDC_HandleTypeDef *hlcdc) {
  const volatile uint32_t *src = (const volatile uint32_t *)hlcdc->Instance;
  for (size_t i = 0; i < DISPLAY_LCDC_REG_DUMP_BYTES / sizeof(uint32_t); i++) {
    s_lcdc_pre_crash_regs[i] = src[i];
  }
}

static void prv_silent_loss_handler(void *data) {
  struct pbl_display_sf32lb_lcdc_state *state = s_lcdc->state;

  if (s_completed_gen == (uint32_t)(uintptr_t)data) {
    return;
  }

  prv_snapshot_lcdc_regs(&state->hlcdc);
  uintptr_t snap_addr = (uintptr_t)s_lcdc_pre_crash_regs;
  size_t snap_size = sizeof(s_lcdc_pre_crash_regs);
  dcache_align(&snap_addr, &snap_size);
  dcache_flush((const void *)snap_addr, snap_size);
  pbl_panic("LCDC silent loss: no EOF in %ums (State=%d Err=0x%lx y=%u..%u)",
            (unsigned)DISPLAY_SILENT_LOSS_TIMEOUT_MS, (int)state->hlcdc.State,
            (unsigned long)state->hlcdc.ErrorCode, (unsigned)state->y0, (unsigned)state->y1);
}

static void prv_update_end(const struct pbl_display_sf32lb_lcdc *lcdc, int status) {
  prv_rows_convert(lcdc, prv_to_222);
  soc_sf32lb_sleep_release(SOC_SF32LB_DEEPWFI);
  pbl_display_complete(&lcdc->disp, status);
}

void pbl_display_sf32lb_lcdc_irq_handler(const struct pbl_display_sf32lb_lcdc *lcdc) {
  LCD_IF_TypeDef *regs = lcdc->state->hlcdc.Instance;

  volatile DisplayIrqLogEntry *entry =
      &s_lcdc_irq_log.entries[s_lcdc_irq_log.write_count % DISPLAY_IRQ_LOG_ENTRIES];
  entry->timestamp = (uint32_t)rtc_get_ticks();
  entry->irq_before = regs->IRQ;
  entry->jdi_par_ctrl = regs->JDI_PAR_CTRL;
  entry->status = regs->STATUS;
  entry->flags = pbl_display_update_in_progress(&lcdc->disp) ? 0x2u : 0x0u;

  s_lcdc_eof_cb_fired = false;
  HAL_LCDC_IRQHandler(&lcdc->state->hlcdc);

  entry->irq_after = regs->IRQ;
  if (s_lcdc_eof_cb_fired) {
    entry->flags |= 0x1u;
  }
  s_lcdc_irq_log.write_count++;
}

void HAL_LCDC_SendLayerDataCpltCbk(LCDC_HandleTypeDef *lcdc) {
  // Tell the IRQ logger the HAL reached the completion path for this interrupt.
  s_lcdc_eof_cb_fired = true;

#ifndef CONFIG_RELEASE
  if (s_test_drop_next_complete && pbl_display_update_in_progress(&s_lcdc->disp)) {
    s_test_drop_next_complete = false;
    // Simulate the lost-completion failure mode: leave the generation
    // incomplete and don't end the update. The silent-loss timer should fire
    // ~DISPLAY_SILENT_LOSS_TIMEOUT_MS later and a panic.
    return;
  }
#endif

  s_completed_gen = s_update_gen;
  prv_update_end(s_lcdc, 0);
}

static int prv_init(const struct pbl_display *disp) {
  const struct pbl_display_sf32lb_lcdc *lcdc = prv_lcdc(disp);
  const struct pbl_panel_timing *timing = lcdc->panel->timing;
  LCDC_HandleTypeDef *hlcdc = &lcdc->state->hlcdc;
  int ret;

  if (pbl_device_init(&lcdc->panel->dev) != 0) {
    return -ENODEV;
  }

  if (lcdc->panel->bus_format != PBL_PANEL_BUS_FORMAT_JDI_RGB222 ||
      timing->hactive != disp->width || timing->vactive != disp->height ||
      (disp->width % 4U) != 0U) {
    return -EINVAL;
  }

  for (uint8_t i = 0U; i < lcdc->num_discharge; i++) {
    if (pbl_device_init(&lcdc->discharge[i].port->dev) != 0) {
      return -ENODEV;
    }
  }

  s_lcdc = lcdc;
  s_silent_loss_timer = new_timer_create();

  // Power cycle the panel with its inputs low
  for (uint8_t i = 0U; i < lcdc->num_discharge; i++) {
    ret = pbl_gpio_configure(&lcdc->discharge[i], PBL_GPIO_OUTPUT_LOW);
    if (ret != 0) {
      return ret;
    }
  }

  ret = pbl_panel_unprepare(lcdc->panel);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_pinctrl_select_state(&lcdc->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0) {
    return ret;
  }

  *hlcdc = (LCDC_HandleTypeDef){
    .Instance = lcdc->regs,
    .Init = {
      .lcd_itf = LCDC_INTF_JDI_PARALLEL,
      .color_mode = LCDC_PIXEL_FORMAT_RGB332,
      .freq = timing->clock_hz,
      .cfg = {
        .jdi = {
          .bank_col_head = timing->hback_porch,
          .valid_columns = timing->hactive,
          .bank_col_tail = timing->hfront_porch,
          .bank_row_head = timing->vback_porch,
          .valid_rows = timing->vactive,
          .bank_row_tail = timing->vfront_porch,
          .enb_start_col = timing->enable_start,
          .enb_end_col = timing->enable_end,
        },
      },
    },
  };

  HAL_LCDC_Init(hlcdc);
  HAL_LCDC_LayerReset(hlcdc, HAL_LCDC_LAYER_DEFAULT);
  HAL_LCDC_LayerSetCmpr(hlcdc, HAL_LCDC_LAYER_DEFAULT, 0);
  HAL_LCDC_LayerSetFormat(hlcdc, HAL_LCDC_LAYER_DEFAULT, LCDC_PIXEL_FORMAT_RGB332);

  pbl_irq_enable(lcdc->irqn);

  return prv_panel_on(lcdc);
}

static int prv_enable(const struct pbl_display *disp) {
  return prv_panel_on(prv_lcdc(disp));
}

static int prv_disable(const struct pbl_display *disp) {
  return prv_panel_off(prv_lcdc(disp));
}

static int prv_set_rotated(const struct pbl_display *disp, bool rotated) {
  const struct pbl_display_sf32lb_lcdc *lcdc = prv_lcdc(disp);

  lcdc->state->rotated = rotated;
  HAL_LCDC_LayerVMirror(&lcdc->state->hlcdc, HAL_LCDC_LAYER_DEFAULT, rotated);

  return 0;
}

static int prv_update(const struct pbl_display *disp, const struct pbl_display_buffer *buf,
                      uint16_t y0, uint16_t y1) {
  const struct pbl_display_sf32lb_lcdc *lcdc = prv_lcdc(disp);
  struct pbl_display_sf32lb_lcdc_state *state = lcdc->state;
  HAL_StatusTypeDef status;

  state->fb = buf->data;
  state->pitch = buf->pitch;
  state->y0 = y0;
  state->y1 = y1;

  // Converted in place, to save a framebuffer of RAM
  prv_rows_convert(lcdc, prv_to_332);

  soc_sf32lb_sleep_block(SOC_SF32LB_DEEPWFI);

  // Arm the timer before kickoff so the EOF IRQ, which can fire as soon as
  // SendLayerData_IT returns, never races us into a state where the timer
  // isn't yet armed.
  s_update_gen++;
  new_timer_start(s_silent_loss_timer, DISPLAY_SILENT_LOSS_TIMEOUT_MS, prv_silent_loss_handler,
                  (void *)(uintptr_t)s_update_gen, 0);

  status = prv_update_start(lcdc);
  if (status != HAL_OK) {
    prv_handle_send_failure(lcdc, status);
    s_completed_gen = s_update_gen;
    prv_rows_convert(lcdc, prv_to_222);
    soc_sf32lb_sleep_release(SOC_SF32LB_DEEPWFI);
    return -EIO;
  }

  return 0;
}

const struct pbl_display_ops pbl_display_sf32lb_lcdc_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .set_rotated = prv_set_rotated,
  .update = prv_update,
};

#ifndef CONFIG_RELEASE
void pbl_display_sf32lb_lcdc_test_drop_next_complete(void) {
  s_test_drop_next_complete = true;
}
#endif
