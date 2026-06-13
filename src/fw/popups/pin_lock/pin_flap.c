/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Shared split-flap (Solari-style) PIN panel widget.
// Draws a bold title and a centred row of rounded panels, one per PIN digit.
// Supports a vertical-roll (Solari flip) animation on the active panel.

#include "pin_flap.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/gcolor_definitions.h"
#include "applib/graphics/gcontext.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_line.h"
#include "applib/graphics/text.h"
#include "applib/ui/animation.h"
#include "board/display.h"
#include "pbl/services/i18n/i18n.h"

// ── panel geometry ────────────────────────────────────────────────────────────
// Each panel is PANEL_W × PANEL_H pixels with PANEL_GAP between panels.
// Chosen so 8 panels fit inside DISP_COLS (200 px on emery):
//   8 * 24 + 7 * 1 = 199 px.
#define PANEL_W       24
#define PANEL_H       28
#define PANEL_GAP      1
#define PANEL_RADIUS   3
// Vertical offset of the panel row from the top of `bounds`.
#define PANELS_TOP_OFFSET  60

// The digit font used inside each panel — LECO 20pt monospace numerals.
#define DIGIT_FONT_KEY  FONT_KEY_LECO_20_BOLD_NUMBERS

// ── helpers ───────────────────────────────────────────────────────────────────

// Draw one split-flap panel at `r`, with a horizontal seam in the middle.
// Fill and border colours are already set by the caller.
static void prv_draw_panel_shell(GContext *ctx, const GRect *r) {
  graphics_fill_round_rect(ctx, r, PANEL_RADIUS, GCornersAll);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_round_rect(ctx, r, PANEL_RADIUS);
  // Solari seam: a 1-px horizontal line across the vertical midpoint.
  int16_t seam_y = r->origin.y + r->size.h / 2;
  graphics_draw_line(ctx,
                     GPoint(r->origin.x + 1, seam_y),
                     GPoint(r->origin.x + r->size.w - 2, seam_y));
}

// Draw the digit centred inside `r`, shifted vertically by `dy` pixels.
// A negative dy moves the text upward; positive moves it downward.
static void prv_draw_digit_offset(GContext *ctx, const GRect *r,
                                  uint8_t digit, int16_t dy) {
  char buf[2] = { (char)('0' + digit), '\0' };
  GFont font = fonts_get_system_font(DIGIT_FONT_KEY);
  GRect shifted = GRect(r->origin.x, r->origin.y + dy, r->size.w, r->size.h);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, buf, font, shifted,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// ── pure helper ───────────────────────────────────────────────────────────────

int16_t pin_flap_roll_offset(int32_t progress, int16_t panel_h) {
  return (int16_t)(((int64_t)progress * panel_h) / ANIMATION_NORMALIZED_MAX);
}

// ── animation callbacks ───────────────────────────────────────────────────────

static void prv_roll_update(Animation *animation, const AnimationProgress progress) {
  PinFlap *flap = (PinFlap *)animation_get_context(animation);
  flap->progress = progress;
  layer_mark_dirty(flap->layer);
}

static void prv_roll_stopped(Animation *animation, bool finished, void *context) {
  PinFlap *flap = (PinFlap *)context;
  flap->animating = false;
  flap->anim = NULL;
  // Final dirty so the layer redraws in static state.
  if (flap->layer) {
    layer_mark_dirty(flap->layer);
  }
}

// ── public API ────────────────────────────────────────────────────────────────

void pin_flap_init(PinFlap *flap, const PinFlapConfig *config) {
  *flap = (PinFlap){ .config = *config };
}

void pin_flap_reset(PinFlap *flap) {
  if (flap->anim) {
    animation_unschedule(flap->anim);
    animation_destroy(flap->anim);
    flap->anim = NULL;
  }
  flap->animating = false;
  flap->progress = 0;
}

void pin_flap_animate_step(PinFlap *flap, Layer *layer,
                           uint8_t from_digit, int8_t direction) {
  // Unschedule any in-flight animation before starting a new one.
  if (flap->anim) {
    animation_unschedule(flap->anim);
    animation_destroy(flap->anim);
    flap->anim = NULL;
  }

  flap->layer = layer;
  flap->from_digit = from_digit;
  flap->direction = direction;
  flap->progress = 0;
  flap->animating = true;

  static const AnimationImplementation s_roll_impl = {
    .update = prv_roll_update,
  };

  Animation *anim = animation_create();
  if (!anim) {
    flap->animating = false;
    return;
  }
  animation_set_duration(anim, 180);
  animation_set_curve(anim, AnimationCurveEaseOut);
  animation_set_implementation(anim, &s_roll_impl);
  animation_set_handlers(anim, (AnimationHandlers){ .stopped = prv_roll_stopped }, flap);
  flap->anim = anim;
  animation_schedule(anim);
}

void pin_flap_draw(PinFlap *flap, GContext *ctx, GRect bounds) {
  const PinEntry *e = flap->config.entry;
  const uint8_t n = e->len;

  // ── title ─────────────────────────────────────────────────────────────────
  GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GRect title_box = GRect(bounds.origin.x,
                          bounds.origin.y + 28,
                          bounds.size.w,
                          24);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, i18n_get(flap->config.title, flap), title_font, title_box,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  i18n_free_all(flap);

  // ── panel row ─────────────────────────────────────────────────────────────
  // Total row width: n panels + (n-1) gaps.
  const int16_t row_w = (int16_t)(n * PANEL_W + (n > 0 ? (n - 1) : 0) * PANEL_GAP);
  // Centre horizontally within bounds.
  const int16_t start_x = bounds.origin.x + (bounds.size.w - row_w) / 2;
  const int16_t panel_y = bounds.origin.y + PANELS_TOP_OFFSET;

  for (uint8_t i = 0; i < n; i++) {
    const int16_t px = start_x + (int16_t)i * (PANEL_W + PANEL_GAP);
    GRect panel = GRect(px, panel_y, PANEL_W, PANEL_H);

    if (i == e->pos) {
      // Active panel: white fill, double border for emphasis.
      graphics_context_set_fill_color(ctx, GColorWhite);
      prv_draw_panel_shell(ctx, &panel);
      // Inner emphasis border (inset by 1 px).
      GRect inner = GRect(panel.origin.x + 1, panel.origin.y + 1,
                          panel.size.w - 2, panel.size.h - 2);
      graphics_draw_round_rect(ctx, &inner, PANEL_RADIUS > 1 ? PANEL_RADIUS - 1 : 0);

      if (flap->animating) {
        // Clip all digit drawing to the panel bounds so the roll stays inside.
        GDrawState saved = graphics_context_get_drawing_state(ctx);
        GDrawState clipped = saved;
        clipped.clip_box = panel;
        graphics_context_set_drawing_state(ctx, clipped);

        // Roll offset: how far the incoming digit has travelled into the panel.
        const int16_t off =
            pin_flap_roll_offset(flap->progress, PANEL_H);
        // direction +1 (up): new digit enters from bottom, old exits to top.
        // direction -1 (down): new digit enters from top, old exits to bottom.
        const int16_t new_dy = (int16_t)(flap->direction > 0
                                         ? PANEL_H - off
                                         : off - PANEL_H);
        const int16_t old_dy = (int16_t)(-flap->direction * off);

        prv_draw_digit_offset(ctx, &panel, e->digits[i], new_dy);
        prv_draw_digit_offset(ctx, &panel, flap->from_digit, old_dy);

        graphics_context_set_drawing_state(ctx, saved);
      } else {
        prv_draw_digit_offset(ctx, &panel, e->digits[i], 0);
      }
    } else if (i < e->pos) {
      // Confirmed panel: light grey fill.
      graphics_context_set_fill_color(ctx, GColorLightGray);
      prv_draw_panel_shell(ctx, &panel);
      // Show masked '*' or the real digit depending on config.
      if (flap->config.mask_confirmed) {
        GFont font = fonts_get_system_font(DIGIT_FONT_KEY);
        graphics_context_set_text_color(ctx, GColorBlack);
        graphics_draw_text(ctx, "*", font, panel,
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
      } else {
        prv_draw_digit_offset(ctx, &panel, e->digits[i], 0);
      }
    } else {
      // Future panel: white fill, no content.
      graphics_context_set_fill_color(ctx, GColorWhite);
      prv_draw_panel_shell(ctx, &panel);
    }
  }
}
