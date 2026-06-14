/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Shared split-flap (Solari-style) PIN panel widget.
// Draws a bold title and a centred row of rounded panels, one per PIN digit.
// Supports a vertical-roll (Solari flip) animation on the active panel.
// Also draws an animated padlock below the panels.

#include "pin_flap.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/gcolor_definitions.h"
#include "applib/graphics/gcontext.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_circle.h"
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

// Pick a LECO bold numeric font (the stylish LED face, and it has a '*' glyph
// for masking). 36 is the largest LECO registered as a system font.
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
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
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
  // Pebble text renders lower than the rect top, so nudge up to optically centre.
  const int16_t y = panel->origin.y + (panel->size.h - fh) / 2 - (fh / 12) + dy;
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

// ── padlock drawing ───────────────────────────────────────────────────────────

// Padlock geometry (all px, body centred at cx,cy).
#define LOCK_BODY_W      26
#define LOCK_BODY_H      20
#define LOCK_BODY_R       4
#define LOCK_SHACKLE_W   16    // outer width of shackle U
#define LOCK_SHACKLE_H   14    // height of shackle above body top
#define LOCK_SHACKLE_T    4    // stroke thickness of shackle legs/arch
#define LOCK_HOLE_R       3    // keyhole circle radius
#define LOCK_HOLE_SLOT    4    // keyhole slot length below circle

// Draws the padlock centred at (cx, cy_body_top) where cy_body_top is the top
// of the body rect. The shackle extends above that.
//   state    — PIN_FLAP_LOCK_* constant
//   progress — 0..ANIMATION_NORMALIZED_MAX
static void prv_draw_padlock(GContext *ctx, GRect bounds,
                             uint8_t state, int32_t progress) {
  // Centre horizontally; place body at ~74% of screen height.
  const int16_t cx    = bounds.origin.x + bounds.size.w / 2;
  const int16_t body_y = bounds.origin.y + (bounds.size.h * 3) / 4 - LOCK_BODY_H / 2;

  // Shake: horizontal oscillation decaying over progress.
  // Use a 4-step pattern: +a, -a, +a, -a with amplitude 4px.
  int16_t dx = 0;
  if (state == PIN_FLAP_LOCK_SHAKING) {
    // 4 half-cycles over full progress range; amplitude decays linearly.
    const int32_t MAX = ANIMATION_NORMALIZED_MAX;
    const int32_t amplitude = 4;
    // Which half-cycle are we in?  4 steps over the range.
    int32_t step = (progress * 4) / MAX;  // 0..3
    int32_t sign = (step % 2 == 0) ? 1 : -1;
    // Decay: 1 at start, ~0 at end.
    int32_t decay = (MAX - progress);   // goes MAX→0
    dx = (int16_t)(sign * amplitude * decay / MAX);
  }

  // Opening: shackle lifts on the left, pivots right leg out.
  // We raise both legs proportionally and tilt shackle open at end.
  int16_t shackle_lift = 0;
  bool clack = false;
  if (state == PIN_FLAP_LOCK_OPENING) {
    const int32_t MAX = ANIMATION_NORMALIZED_MAX;
    shackle_lift = (int16_t)((int32_t)10 * progress / MAX);
    clack = (progress > MAX / 2);
  }

  // ── body ─────────────────────────────────────────────────────────────────────
  const GRect body = GRect(cx - LOCK_BODY_W / 2 + dx, body_y,
                           LOCK_BODY_W, LOCK_BODY_H);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_round_rect(ctx, &body, LOCK_BODY_R, GCornersAll);

  // Keyhole: white circle + short slot below it.
  const GPoint khole_center = GPoint(cx + dx, body_y + LOCK_BODY_H / 2 - 2);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, khole_center, LOCK_HOLE_R);
  // Slot below keyhole circle.
  const GRect kslot = GRect(cx - 1 + dx, khole_center.y + LOCK_HOLE_R - 1,
                            3, LOCK_HOLE_SLOT);
  graphics_fill_rect(ctx, &kslot);

  // ── shackle (U-shaped hasp above body) ───────────────────────────────────────
  // Draw as two vertical bars + a rounded top connecting them.
  // When opening, left leg lifts and the right leg stays near body (pivot effect).
  const int16_t leg_x_l = cx - LOCK_SHACKLE_W / 2 + dx;
  const int16_t leg_x_r = cx + LOCK_SHACKLE_W / 2 - LOCK_SHACKLE_T + dx;
  // Both legs: left lifts fully, right lifts only slightly when opening
  // (pivot around the right side gives an "opening" look).
  const int16_t lift_l = shackle_lift;
  const int16_t lift_r = (state == PIN_FLAP_LOCK_OPENING) ? shackle_lift / 3 : 0;

  // Top of shackle arch.
  const int16_t arch_top = body_y - LOCK_SHACKLE_H;

  // Left leg.
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
  {
    GRect leg_l = GRect(leg_x_l, arch_top - lift_l,
                        LOCK_SHACKLE_T, LOCK_SHACKLE_H - lift_l);
    graphics_fill_round_rect(ctx, &leg_l, 2, GCornersTop);
  }
  // Right leg.
  {
    GRect leg_r = GRect(leg_x_r, arch_top - lift_r,
                        LOCK_SHACKLE_T, LOCK_SHACKLE_H - lift_r);
    graphics_fill_round_rect(ctx, &leg_r, 2, GCornersTop);
  }
  // Connecting arch across the top.
  {
    GRect arch = GRect(leg_x_l, arch_top - lift_l - LOCK_SHACKLE_T,
                       LOCK_SHACKLE_W, LOCK_SHACKLE_T + 2);
    graphics_fill_round_rect(ctx, &arch, 2, GCornersAll);
  }

  // ── clack lines (opening, past 50%) ──────────────────────────────────────────
  if (clack) {
    graphics_context_set_stroke_color(ctx, GColorBlack);
    // Three short radiating lines near the top-left of the shackle arch.
    const int16_t cl_x = leg_x_l - 2;
    const int16_t cl_y = arch_top - lift_l - LOCK_SHACKLE_T - 2;
    graphics_draw_line(ctx, GPoint(cl_x - 4, cl_y - 4), GPoint(cl_x - 8, cl_y - 8));
    graphics_draw_line(ctx, GPoint(cl_x - 2, cl_y - 5), GPoint(cl_x - 4, cl_y - 10));
    graphics_draw_line(ctx, GPoint(cl_x - 5, cl_y - 2), GPoint(cl_x - 10, cl_y - 4));
  }
}

// ── padlock animation callbacks ───────────────────────────────────────────────

static void prv_lock_update(Animation *animation, const AnimationProgress progress) {
  PinFlap *flap = (PinFlap *)animation_get_context(animation);
  flap->lock_progress = progress;
  layer_mark_dirty(flap->layer);
}

static void prv_shake_stopped(Animation *animation, bool finished, void *context) {
  PinFlap *flap = (PinFlap *)context;
  flap->lock_anim = NULL;
  flap->lock_state = PIN_FLAP_LOCK_CLOSED;
  flap->lock_progress = 0;
  if (flap->layer) {
    layer_mark_dirty(flap->layer);
  }
}

static void prv_open_stopped(Animation *animation, bool finished, void *context) {
  PinFlap *flap = (PinFlap *)context;
  flap->lock_anim = NULL;
  // Stay in OPENING state so the open padlock remains visible.
  // Capture the callback before any possible teardown.
  void (*on_done)(void *) = flap->lock_on_open_done;
  void *on_ctx = flap->lock_on_open_ctx;
  flap->lock_on_open_done = NULL;
  flap->lock_on_open_ctx = NULL;
  if (on_done) {
    on_done(on_ctx);
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
  // Also reset padlock state.
  if (flap->lock_anim) {
    animation_unschedule(flap->lock_anim);
    animation_destroy(flap->lock_anim);
    flap->lock_anim = NULL;
  }
  flap->lock_state = PIN_FLAP_LOCK_CLOSED;
  flap->lock_progress = 0;
  flap->lock_on_open_done = NULL;
  flap->lock_on_open_ctx = NULL;
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
      // Active panel: white fill, accent-tinted inner ring for emphasis.
      graphics_context_set_fill_color(ctx, GColorWhite);
      prv_draw_panel_shell(ctx, &panel);
      GRect inner = GRect(panel.origin.x + 1, panel.origin.y + 1,
                          panel.size.w - 2, panel.size.h - 2);
      graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(flap->config.accent, GColorBlack));
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
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
      prv_draw_panel_shell(ctx, &panel);
      const char ch = flap->config.mask_confirmed ? '*' : (char)('0' + e->digits[i]);
      prv_draw_glyph(ctx, &panel, digit_font, ch, 0);
    } else {
      // Future panel: white fill, no content.
      graphics_context_set_fill_color(ctx, GColorWhite);
      prv_draw_panel_shell(ctx, &panel);
    }
  }

  // Draw the padlock below the panel row.
  prv_draw_padlock(ctx, bounds, flap->lock_state, flap->lock_progress);
}

void pin_flap_padlock_shake(PinFlap *flap, Layer *layer) {
  if (flap->lock_anim) {
    animation_unschedule(flap->lock_anim);
    animation_destroy(flap->lock_anim);
    flap->lock_anim = NULL;
  }

  flap->layer = layer;
  flap->lock_state = PIN_FLAP_LOCK_SHAKING;
  flap->lock_progress = 0;

  static const AnimationImplementation s_shake_impl = {
    .update = prv_lock_update,
  };

  Animation *anim = animation_create();
  if (!anim) {
    flap->lock_state = PIN_FLAP_LOCK_CLOSED;
    return;
  }
  animation_set_duration(anim, 350);
  animation_set_curve(anim, AnimationCurveLinear);
  animation_set_implementation(anim, &s_shake_impl);
  animation_set_handlers(anim, (AnimationHandlers){ .stopped = prv_shake_stopped }, flap);
  flap->lock_anim = anim;
  animation_schedule(anim);
}

void pin_flap_padlock_open(PinFlap *flap, Layer *layer,
                           void (*on_done)(void *ctx), void *ctx) {
  if (flap->lock_anim) {
    animation_unschedule(flap->lock_anim);
    animation_destroy(flap->lock_anim);
    flap->lock_anim = NULL;
  }

  flap->layer = layer;
  flap->lock_state = PIN_FLAP_LOCK_OPENING;
  flap->lock_progress = 0;
  flap->lock_on_open_done = on_done;
  flap->lock_on_open_ctx = ctx;

  static const AnimationImplementation s_open_impl = {
    .update = prv_lock_update,
  };

  Animation *anim = animation_create();
  if (!anim) {
    // On OOM: fire callback immediately without animation.
    flap->lock_state = PIN_FLAP_LOCK_OPENING;
    flap->lock_on_open_done = NULL;
    flap->lock_on_open_ctx = NULL;
    if (on_done) {
      on_done(ctx);
    }
    return;
  }
  animation_set_duration(anim, 350);
  animation_set_curve(anim, AnimationCurveEaseOut);
  animation_set_implementation(anim, &s_open_impl);
  animation_set_handlers(anim, (AnimationHandlers){ .stopped = prv_open_stopped }, flap);
  flap->lock_anim = anim;
  animation_schedule(anim);
}
