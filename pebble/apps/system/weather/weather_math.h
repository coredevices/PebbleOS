/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pebble_compat.h"

#include <pbl/util/bitops.h>

// Format-safe framebuffer row pixel access, shared by every raw-blit routine in
// the app. Colour boards address a row one byte per pixel; the BW boards pack
// 8 pixels per byte (bit x%8 of byte x/8 — the util/bitset.h layout the 1-bit
// renderer writes), so byte indexing there runs off the end of the row.
// `argb8` carries GColor.argb; on BW any colour with a lit RGB bit lands white.
static inline void weather_fb_row_set(uint8_t *row_data, int x, uint8_t argb8) {
#if PBL_BW
  pbl_bitset8_update(row_data, (unsigned)x, (argb8 & 0x3F) != 0);
#else
  row_data[x] = argb8;
#endif
}

static inline uint8_t weather_fb_row_get(const uint8_t *row_data, int x) {
#if PBL_BW
  return pbl_bitset8_get(row_data, (unsigned)x) ? GColorWhiteARGB8 : GColorBlackARGB8;
#else
  return row_data[x];
#endif
}

uint32_t weather_scale_u32(uint32_t value, uint32_t numerator, uint32_t denominator);
int32_t weather_scale_i32(int32_t value, int32_t numerator, int32_t denominator);
int32_t weather_isqrt(int32_t value);
int32_t weather_norm_square(int32_t value);
int32_t weather_norm_bell(int32_t value);
void weather_capture_framebuffer_rect(GBitmap *fb, GBitmap *dst, GRect src_rect, int dst_x);

// Card<->globe horizontal slide duration (SELECT out-left / BACK in-left): a snappy fraction
// of the moook default so the two-phase transition feels quick. Shared by expanded_view.c and
// globe_view.c so the pair can't drift apart.
#define WEATHER_HSLIDE_MS 110

// One-mid-frame moook-soft: the shared slide/drop interpolation for the card<->globe
// transitions. Its built-in 4px overshoot is the "bounce".
int64_t weather_interpolate_moook_soft1(int32_t n, int64_t from, int64_t to);

// Copies `src` (NULL as "") into `dst`, cut short on a UTF-8 character boundary when it does
// not fit. The text renderer draws nothing for a string with a split character.
void weather_copy_text(char *dst, size_t dst_size, const char *src);

// Uppercase 3-letter weekday abbreviation for the day `day_offset` days from now ("MON").
// Falls back to (up to 3 chars of) `fallback`, or "---", if localtime/strftime fail.
void weather_fill_weekday_abbrev(int day_offset, const char *fallback, char *buffer,
                                 size_t buffer_size);

// Whether `then` is in the same hour of the watch's local time as `now`, and not after it.
bool weather_same_local_hour(time_t now, time_t then);

// An hourly chance of precipitation the phone has no figure for
#define WEATHER_PRECIP_UNKNOWN 255
// Hours in the clock's chance of precipitation series: today's, then tomorrow's
#define WEATHER_PRECIP_HOURS 48

// Stores a series of 24 or WEATHER_PRECIP_HOURS chances of precipitation in `hours`. Values over
// 100 percent and the hours past the series are stored as unknown. Returns the hours stored, or
// 0 when no hour is known.
uint8_t weather_store_hourly_precip(uint8_t hours[WEATHER_PRECIP_HOURS], const uint8_t *precip,
                                    size_t count);

// Diurnal curve (0..100): cool overnight, peak mid-afternoon. Shared by the hourly
// synth in weather.c and the v4 test seed in weather_data_source.c.
extern const uint8_t weather_diurnal_curve[24];

//! WHO UV severity ramp (shared by the card dial + the report UV bar).
GColor weather_uv_severity_color(int uv);

// Two half-turn arcs starting at `phase` and at -`phase`, with sparks in the theme background
// color at their starts and three beads. When idle both start at `phase`, with no sparks or
// beads. extra_span lengthens both arcs (TRIG_MAX_ANGLE / 2 closes the ring).
void weather_draw_lava_ring(GContext *ctx, GPoint center, int outer_r, GColor glow_color,
                            uint32_t phase, bool idle, int32_t extra_span);

// Whole-screen Timeline jelly squash-stretch blit, shared by forecast_list and clock_face.
// `scratch` = W*H one-shot framebuffer snapshot. Modes 1-3 match forecast_list's
// SQUASH_DROP_IN/_DOWN_EXIT/_RISE_IN. Mode 4 is the clock's forward exit: geometrically the
// down-exit, WITHOUT the me=m+m/3 haste — the forecast's exit is deliberately compressed into
// its first ~75% (sunset-card text staging) while the clock's hand-tuned exit runs the full
// timeline. Do not merge the two.
#define WEATHER_SQUASH_DROP_IN    1
#define WEATHER_SQUASH_DOWN_EXIT  2
#define WEATHER_SQUASH_RISE_IN    3
#define WEATHER_SQUASH_CLOCK_EXIT 4
// Mode 5: the forecast's clock-burst stage-1 — the whole scrolled screen jelly-stretches
// UP off the top (top edge leads, bottom trails), full timeline like the clock exit.
#define WEATHER_SQUASH_UP_EXIT 5
// Mode 6: the forecast's SELECT exit — the UP_EXIT grammar rotated 90°: the whole
// screen jelly-stretches off the LEFT (left edge leads, right trails the half-lag).
#define WEATHER_SQUASH_LEFT_EXIT 6
// Mode 7: the weather report's entrance — RISE_IN rotated 90°: the whole screen
// jelly-stretches IN from the RIGHT (left edge leads into place, right trails).
void weather_render_squash(GContext *ctx, uint8_t *scratch, AnimationProgress m, int mode);
// Capture-once fast path: re-blit from a snapshot a prior weather_render_squash call
// filled, skipping the per-frame framebuffer copy. Round-only callers (gabbro
// smoothness step 2); rect keeps calling weather_render_squash unchanged.
void weather_render_squash_cached(GContext *ctx, const uint8_t *scratch, AnimationProgress m,
                                  int mode);
//! Invert PDC icon black/white colors when system is in dark mode.
void weather_recolor_gbitmap_for_dark_mode(GBitmap *bitmap);
void weather_recolor_pdc_image_for_dark_mode(GDrawCommandImage *image);
void weather_recolor_pdc_sequence_for_dark_mode(GDrawCommandSequence *sequence);

//! Swaps black and white in a PDC image, as the dark mode recolor does. Applying it twice restores
//! the original colors, so it also undoes the recolor when dark mode flips back.
void weather_invert_pdc_image(GDrawCommandImage *image);
void weather_invert_pdc_sequence(GDrawCommandSequence *sequence);
