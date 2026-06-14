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
#include "applib/ui/vibes.h"
#include "board/display.h"
#include "pbl/services/i18n/i18n.h"

// ── panel geometry ────────────────────────────────────────────────────────────
// Panels scale to fill the row width based on the PIN length, so a 4-digit PIN
// gets big, clear cards while an 8-digit PIN still fits across the display.
#define ROW_MARGIN          10   // left/right margin around the panel row
#define PANEL_GAP            5
#define PANEL_MAX_W         34    // narrow, drum-wheel proportion
#define PANEL_MIN_W         16
#define PANEL_ASPECT_EXTRA  18    // panel height = width + this (taller than wide)
#define PANEL_RADIUS         4
#define PANELS_TOP_OFFSET   62

// Pick a LECO bold numeric font sized to fill the panel. Only the *bold*
// LECO faces (<= 36) are registered as system fonts; 38/42/60 fall back.
static GFont prv_digit_font(int16_t panel_w) {
  if (panel_w >= 30) return fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS);
  if (panel_w >= 24) return fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS);
  return fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS);
}

// ── helpers ───────────────────────────────────────────────────────────────────

// Draw a soft drop-shadow below+right of the panel for a raised, tactile look.
// Most of it is hidden behind the panel; only the bottom/right edge peeks out.
static void prv_draw_shadow(GContext *ctx, const GRect *r) {
  GRect sh = GRect(r->origin.x + 1, r->origin.y + 3, r->size.w, r->size.h);
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_round_rect(ctx, &sh, PANEL_RADIUS, GCornersAll);
}

// Draw one rounded drum-window panel at `r`. Fill colour is set by the caller.
// No centre seam — the digit rolls like a number drum on an old flip clock.
static void prv_draw_panel_shell(GContext *ctx, const GRect *r) {
  graphics_fill_round_rect(ctx, r, PANEL_RADIUS, GCornersAll);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_round_rect(ctx, r, PANEL_RADIUS);
}

// Draw a single glyph (digit or '*') vertically centred in `panel`, shifted by
// `dy` pixels (used by the roll animation). Negative dy moves it up.
static void prv_draw_glyph(GContext *ctx, const GRect *panel, GFont font,
                           char ch, int16_t dy) {
  char buf[2] = { ch, '\0' };
  const int16_t fh = (int16_t)fonts_get_font_height(font);
  // Number glyphs sit in the upper part of the line box (empty descender space
  // below), so nudge down ~1/8 of the line height to optically centre them.
  const int16_t y = panel->origin.y + (panel->size.h - fh) / 2 + (fh / 8) + dy;
  GRect box = GRect(panel->origin.x, y, panel->size.w, fh + 4);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, buf, font, box,
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
  // Light "tic" as the flap settles — only on a natural finish (not when a
  // rapid next press interrupts the roll), so holding up/down doesn't buzz-storm.
  if (finished && flap->config.haptic) {
    static const uint32_t segments[] = { 22 };  // ~22 ms, a faint tick
    VibePattern pattern = { .durations = segments, .num_segments = 1 };
    vibes_enqueue_custom_pattern(pattern);
  }
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
  if (n == 0) {
    return;
  }

  // ── title (bold, clock-style) ───────────────────────────────────────────────
  GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GRect title_box = GRect(bounds.origin.x, bounds.origin.y + 20, bounds.size.w, 32);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, i18n_get(flap->config.title, flap), title_font, title_box,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  i18n_free_all(flap);

  // ── dynamic panel sizing ────────────────────────────────────────────────────
  const int16_t avail_w = bounds.size.w - 2 * ROW_MARGIN;
  int16_t panel_w = (int16_t)((avail_w - (n - 1) * PANEL_GAP) / n);
  if (panel_w > PANEL_MAX_W) {
    panel_w = PANEL_MAX_W;
  }
  if (panel_w < PANEL_MIN_W) {
    panel_w = PANEL_MIN_W;
  }
  const int16_t panel_h = panel_w + PANEL_ASPECT_EXTRA;
  GFont digit_font = prv_digit_font(panel_w);

  const int16_t row_w = (int16_t)(n * panel_w + (n - 1) * PANEL_GAP);
  const int16_t start_x = bounds.origin.x + (bounds.size.w - row_w) / 2;
  const int16_t panel_y = bounds.origin.y + PANELS_TOP_OFFSET;

  for (uint8_t i = 0; i < n; i++) {
    const int16_t px = start_x + (int16_t)i * (panel_w + PANEL_GAP);
    GRect panel = GRect(px, panel_y, panel_w, panel_h);

    prv_draw_shadow(ctx, &panel);

    if (i == e->pos) {
      // Active panel: white fill, double border for emphasis.
      graphics_context_set_fill_color(ctx, GColorWhite);
      prv_draw_panel_shell(ctx, &panel);
      GRect inner = GRect(panel.origin.x + 1, panel.origin.y + 1,
                          panel.size.w - 2, panel.size.h - 2);
      graphics_draw_round_rect(ctx, &inner, PANEL_RADIUS > 1 ? PANEL_RADIUS - 1 : 0);

      if (flap->animating) {
        // Clip the rolling digits to the panel so they stay inside the card.
        GDrawState saved = graphics_context_get_drawing_state(ctx);
        GDrawState clipped = saved;
        clipped.clip_box = panel;
        graphics_context_set_drawing_state(ctx, clipped);

        const int16_t off = pin_flap_roll_offset(flap->progress, panel_h);
        // +1 (up): new enters from bottom, old exits to top.
        // -1 (down): new enters from top, old exits to bottom.
        const int16_t new_dy = (int16_t)(flap->direction > 0 ? panel_h - off : off - panel_h);
        const int16_t old_dy = (int16_t)(-flap->direction * off);

        prv_draw_glyph(ctx, &panel, digit_font, (char)('0' + e->digits[i]), new_dy);
        prv_draw_glyph(ctx, &panel, digit_font, (char)('0' + flap->from_digit), old_dy);

        graphics_context_set_drawing_state(ctx, saved);
      } else {
        prv_draw_glyph(ctx, &panel, digit_font, (char)('0' + e->digits[i]), 0);
      }
    } else if (i < e->pos) {
      // Confirmed panel: light grey fill; masked '*' or the real digit.
      graphics_context_set_fill_color(ctx, GColorLightGray);
      prv_draw_panel_shell(ctx, &panel);
      const char ch = flap->config.mask_confirmed ? '*' : (char)('0' + e->digits[i]);
      prv_draw_glyph(ctx, &panel, digit_font, ch, 0);
    } else {
      // Future panel: white fill, no content.
      graphics_context_set_fill_color(ctx, GColorWhite);
      prv_draw_panel_shell(ctx, &panel);
    }
  }
}
