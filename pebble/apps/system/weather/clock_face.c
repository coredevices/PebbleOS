/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clock_face.h"

#include "pebble_compat.h"
#include "weather_math.h"
#include "weather_types.h"

#include <time.h>

#include <pbl/kernel/types.h>   // PBL_TICK_HZ
#include <pbl/services/clock.h> // clock_is_24h_style

#include <applib/graphics/gdraw_command_image.h>
#include <applib/graphics/gdraw_command_transforms.h>
#include <applib/graphics/raw_image.h>
#include <applib/ui/animation_interpolate.h>
#include <applib/ui/app_window_stack.h>
#include <syscall/syscall.h>

// ---- Layout ----
#define MAX_DAYS 7
// One bitmap slot per drawable WeatherType enum value (0..8).
#define NUM_TYPE_SLOTS 9

// Geometry — tuned so 25×25 icons at the orbit don't clip the screen or the bar.
#if PBL_DISPLAY_HEIGHT >= 200
// Emery: 200×228 rect — oval to fill the screen
#define ICON_ORBIT_RX PBL_IF_ROUND_ELSE(100, 87)
#define ICON_ORBIT_RY PBL_IF_ROUND_ELSE(100, 94)
#else
// Small displays. Chalk: 180x180 round. Flint/asterix: 144x168 rect - the
// oval hugs the screen edge minus the 25px icons.
#define ICON_ORBIT_RX PBL_IF_ROUND_ELSE(63, 58)
#define ICON_ORBIT_RY PBL_IF_ROUND_ELSE(63, 68)
#endif

#define ICON_SIZE 25 // Emery/native tiny weather bitmap size
// Small rect (flint/asterix 144x168): r17 discs merge at this orbit's ~30px
// neighbour spacing — shrink them to keep a visible gap.
#define CLOCK_SMALL_RECT (!PBL_ROUND && PBL_DISPLAY_HEIGHT < 200)
#define CLOCK_DISC_R     (CLOCK_SMALL_RECT ? 13 : (ICON_SIZE * 7 / 10))
// Round clock icons draw 1:1 from the 25x25 TINY PNGs (CLOCK ids now map to TINY),
// so the slot must be 25 to avoid the previous 50->30 crop. Emery uses ICON_SIZE.
// Small rect: the 12-icon ring's orbit spacing is ~30px; with the BW discs
// gone the bare 25px art of diagonal neighbours visually collides, so the
// dial icons shrink to 20px at load (see prv_shrink_dial_icon).
#define CLOCK_ICON_SIZE         PBL_IF_ROUND_ELSE(25, (CLOCK_SMALL_RECT ? 20 : ICON_SIZE))
#define SWIPE_THRESHOLD         20
#define CLOCK_INTRO_DURATION_MS 520
// Glow step period. On round it is two animation service ticks, so each step falls on a
// display frame.
#define CLOCK_GLOW_TIMER_MS PBL_IF_ROUND_ELSE(66, 50)
#define CLOCK_GLOW_OUTER_R  PBL_IF_ROUND_ELSE(42, 32)
// A smaller glow ring for today's dial
#define CLOCK_GLOW_OUTER_R_TODAY   PBL_IF_ROUND_ELSE(38, 28)
#define CLOCK_GLOW_IDLE_TIMEOUT_MS 5000

// ---- State ----
// What the ring of bubbles shows. A tap advances to the next mode.
typedef enum {
  ClockFaceModeIcons,  // weather icons on condition-coloured discs
  ClockFaceModeTemps,  // per-hour temperatures
  ClockFaceModePrecip, // per-hour chance of precipitation, as water filling each bubble
  ClockFaceModeCount,
} ClockFaceMode;

// One mode's centre content, captured once on the theme's background so it can slide and scale
// without being redrawn. The pixels come from malloc_try: the dial draws without them.
typedef struct {
  GBitmap bmp;     // wraps `pixels`: 8-bit on colour, 1-bit on BW
  uint8_t *pixels; // NULL until first shown
  int16_t cx, cy;  // the content's visual centre in bmp, its scale pivot
} CentreImage;

// A string drawn once into a 1-bit mask and stamped from it afterwards (see prv_draw_text)
typedef struct {
  uint8_t *bits; // 1 bit per pixel, rows of (w + 7) / 8 bytes, NULL for a free slot
  GFont font;
  char text[6];
  uint8_t lw;    // the text's laid-out width
  int8_t ox, oy; // the mask's offset from the laid-out text's top left
  uint8_t w, h;  // the mask's size
} TextStamp;
#define TEXT_STAMP_SLOTS 32

typedef struct {
  Window *window;
  Layer *canvas;
  WeatherLocationForecast days[MAX_DAYS];
  size_t num_days;
  int day_index;                         // which day this clock represents (0 = today)
  uint8_t hourly_types[24];              // WeatherType per hour 0-23
  bool hourly_valid;                     // true once hourly data received
  GBitmap *type_bitmaps[NUM_TYPE_SLOTS]; // one loaded bitmap per unique WeatherType
  GFont temp_font;
  CentreImage centre[ClockFaceModeCount]; // each mode's centre content
  WeatherType centre_type;                // the centre icon's and ring's weather, set hourly
  AnimationProgress anim_progress;        // intro animation progress, 0 to ANIMATION_NORMALIZED_MAX
  Animation *intro_anim;
  bool skip_intro_animation;
  AppTimer *glow_timer;
#if PBL_ROUND
  Animation *glow_drv;      // glow driver on the animation service tick (see prv_glow_drv_*)
  uint32_t glow_drv_acc_ms; // ms gathered toward the next glow step
#endif
  uint32_t glow_last_ms; // when the glow last advanced
  uint32_t glow_phase;
  uint32_t glow_idle_ms;  // ms the glow has advanced since the last press
  uint32_t glow_open_ms;  // when the ring began to open again after closing
  int32_t glow_open_from; // how far closed it was then (see prv_glow_extra_span)
  // ---- Tap modes ----
  int8_t hourly_temps[24]; // temperature per hour 0-23
  bool hourly_temps_valid;
  // TOMORROW's hourly series (v4 minor 5) — feeds the dial's post-midnight
  // positions on day 0 (abs hours 24-47).
  uint8_t tomorrow_types[24];
  int8_t tomorrow_temps[24];
  bool tomorrow_hourly_valid;
  ClockFaceMode mode;    // what the ring shows while no transition runs
  ClockFaceMode mode_to; // the transition's target while reveal_active
  bool reveal_active;    // true while a mode transition is in progress
  AnimationProgress reveal_p;
  Animation *reveal_anim;
  // Chance of precipitation per hour, percent (255 unknown): today's hours 0-23, then tomorrow's
  // when known.
  uint8_t hourly_precip[WEATHER_PRECIP_HOURS];
  uint8_t hourly_precip_hours; // how many of hourly_precip are filled: 0, 24 or 48
  uint32_t fill_started_ms;    // when the latest flip into the precipitation mode began
  Animation *fill_anim;        // redraws while the water rises
  bool tap_queued;             // a tap during a flip, played once it ends
  AppTimer *tap_timer;
  TextStamp stamps[TEXT_STAMP_SLOTS];
  uint8_t stamp_next; // the slot the next new stamp takes
#ifdef CONFIG_TOUCH
  int16_t touch_start_x;
  int16_t touch_start_y;
  bool touch_active;
  bool touch_started_during_intro;
#endif
  // ---- UP → forecast: whole-screen Timeline squash-stretch exit ----
  // The clock is captured each frame and jelly-stretched down off the bottom (two moook edges:
  // the bottom leads, the top trails — the bitmap analogue of scale_segmented's per-point lag).
  // BOTH shapes: weather_render_squash is chord-clamped, so round drives the same mode.
  AnimationProgress fwd_exit_p; // 0 → MAX
  Animation *fwd_exit_anim;
  bool fwd_exit_active;
  bool fwd_captured;        // ROUND capture-once: snapshot filled — later frames skip
                            // the clock render + fb copy and only resample (step 2)
  AppTimer *fwd_push_timer; // 0ms timer → hand off to the forecast once fully off-top
  uint8_t *fwd_scratch;     // full-screen snapshot we re-sample while overwriting the fb
} ClockFaceData;

static ClockFaceData *s_cf;
static ClockFaceWrapCallback s_wrap_callback;
static void *s_wrap_context;

// System-app statics persist across launches (the fw image is not reloaded), and a
// crashed run never reaches unload — clear them before the next run reads them.
void clock_face_reset(void) {
  s_cf = nullptr;
  s_wrap_callback = nullptr;
  s_wrap_context = nullptr;
}

// ---- Helpers ----

#define prv_bg_color_for_type weather_type_bg_color
#if CLOCK_SMALL_RECT
// 50px source art — downscaled to 20px at load (see prv_shrink_dial_icon).
#define prv_icon_res weather_type_icon_small_resource
#else
#define prv_icon_res weather_type_icon_tiny_resource
#endif

#if CLOCK_SMALL_RECT
// Shrink the 50px SMALL icon to CLOCK_ICON_SIZE by box sampling with a dark
// threshold: a destination pixel inks when >=30% of the source pixels it
// covers are dark. The 25px TINY art can't downscale (its 1px strokes either
// vanish or merge); the 50px art gives each destination pixel a 2-3px box to
// vote from, which keeps the strokes one pixel and clean.
static GBitmap *prv_shrink_dial_icon(GBitmap *src) {
  if (!src)
    return nullptr;
  const GBitmapFormat fmt = gbitmap_get_format(src);
  uint8_t bpp;
  uint32_t entries;
  switch (fmt) {
    case GBitmapFormat1BitPalette:
      bpp = 1;
      entries = 2;
      break;
    case GBitmapFormat2BitPalette:
      bpp = 2;
      entries = 4;
      break;
    case GBitmapFormat4BitPalette:
      bpp = 4;
      entries = 16;
      break;
    default:
      return src; // unexpected format — draw at native size
  }
  const GRect sb = gbitmap_get_bounds(src);
  const int dw = CLOCK_ICON_SIZE;
  GColor *src_pal = gbitmap_get_palette(src);
  if (!src_pal || sb.size.w <= dw)
    return src;

  // Rank each palette entry once: 0 transparent, 1 light, 2 ink. The 50px art
  // carries gray shading; anything that quantizes to the dark half is ink.
  uint8_t rank[16] = {};
  for (uint32_t i = 0; i < entries; i++) {
    if (!src_pal[i].a)
      continue;
    const GColor8 gray = gcolor_get_grayscale(src_pal[i]);
    rank[i] = (gcolor_equal(gray, GColorBlack) || gcolor_equal(gray, GColorDarkGray)) ? 2 : 1;
  }

  // The destination palette is purified to pure black/white: mid-gray fills
  // would dither on 1-bit and turn the shrunken icons into mush.
  GColor *pal = malloc_try(entries * sizeof(GColor));
  if (!pal)
    return src;
  for (uint32_t i = 0; i < entries; i++) {
    if (rank[i] == 2)
      pal[i] = system_theme_get_fg_color();
    else if (rank[i] == 1)
      pal[i] = system_theme_get_bg_color();
    else
      pal[i] = src_pal[i]; // keep transparency
  }
  GBitmap *dst =
      gbitmap_create_blank_with_palette(GSize(dw, dw), fmt, pal, true /* free palette */);
  if (!dst) {
    free(pal);
    return src;
  }

  const uint8_t *sdata = gbitmap_get_data(src);
  const uint16_t sstride = gbitmap_get_bytes_per_row(src);
  uint8_t *ddata = gbitmap_get_data(dst);
  const uint16_t dstride = gbitmap_get_bytes_per_row(dst);
  const int sw = sb.size.w, sh = sb.size.h;
  // Canonical palette index per rank (first dark / light / transparent entry).
  int dark_i = -1, light_i = -1, trans_i = -1;
  for (uint32_t i = 0; i < entries; i++) {
    if (rank[i] == 2 && dark_i < 0)
      dark_i = (int)i;
    if (rank[i] == 1 && light_i < 0)
      light_i = (int)i;
    if (rank[i] == 0 && trans_i < 0)
      trans_i = (int)i;
  }
  for (int y = 0; y < dw; y++) {
    const int sy0 = y * sh / dw;
    const int sy1 = ((y + 1) * sh + dw - 1) / dw; // exclusive, >= sy0+1
    for (int x = 0; x < dw; x++) {
      const int sx0 = x * sw / dw;
      const int sx1 = ((x + 1) * sw + dw - 1) / dw;
      int n_dark = 0, n_light = 0, n_total = 0;
      for (int sy = sy0; sy < sy1 && sy < sh; sy++) {
        for (int sx = sx0; sx < sx1 && sx < sw; sx++) {
          const uint8_t ci = raw_image_get_value_for_bitdepth(
              sdata, (uint32_t)(sb.origin.x + sx), (uint32_t)(sb.origin.y + sy), sstride, bpp);
          n_total++;
          if (rank[ci] == 2)
            n_dark++;
          else if (rank[ci] == 1)
            n_light++;
        }
      }
      int pick = trans_i;
      if (dark_i >= 0 && n_dark * 10 >= n_total * 3) {
        pick = dark_i;
      } else if (light_i >= 0 && n_light * 2 > n_total) {
        pick = light_i;
      }
      if (pick < 0)
        pick = 0;
      raw_image_set_value_for_bitdepth(ddata, (uint32_t)x, (uint32_t)y, dstride, bpp,
                                       (uint8_t)pick);
    }
  }
  gbitmap_destroy(src);
  return dst;
}
#endif

// Load the tiny bitmap set once; the clock reuses these during animations.
static void prv_load_bitmaps(void) {
  if (!s_cf)
    return;
  for (int i = 0; i < NUM_TYPE_SLOTS; i++) {
    if (s_cf->type_bitmaps[i]) {
      gbitmap_destroy(s_cf->type_bitmaps[i]);
      s_cf->type_bitmaps[i] = nullptr;
    }
  }
  // Load only the types this push can DRAW (the day set + the hourly reveal
  // series), not all nine — the type set is fixed for the window's life (day
  // flips re-push). Every consumer clamps out-of-range bytes to Generic, so
  // the used-set applies the same clamp, and Generic is ALWAYS loaded (it is
  // also the NULL-slot fallback at draw time). A few KB of session heap back.
  bool used[NUM_TYPE_SLOTS] = {false};
  used[WeatherType_Generic] = true;
  for (size_t i = 0; i < s_cf->num_days; i++) {
    uint8_t wt = (uint8_t)s_cf->days[i].current_weather_type;
    used[wt <= WeatherType_RainAndSnow ? wt : (uint8_t)WeatherType_Generic] = true;
  }
  if (s_cf->hourly_valid) { // draw only samples hourly when the series is complete
    for (int h = 0; h < 24; h++) {
      uint8_t wt = s_cf->hourly_types[h];
      used[wt <= WeatherType_RainAndSnow ? wt : (uint8_t)WeatherType_Generic] = true;
    }
  }
  if (s_cf->tomorrow_hourly_valid) { // day 0's post-midnight positions (v4 minor 5)
    for (int h = 0; h < 24; h++) {
      uint8_t wt = s_cf->tomorrow_types[h];
      used[wt <= WeatherType_RainAndSnow ? wt : (uint8_t)WeatherType_Generic] = true;
    }
  }
  for (int i = WeatherType_PartlyCloudy; i <= WeatherType_RainAndSnow; i++) {
    if (used[i]) {
      s_cf->type_bitmaps[i] = gbitmap_create_with_resource(prv_icon_res((WeatherType)i));
#if CLOCK_SMALL_RECT
      s_cf->type_bitmaps[i] = prv_shrink_dial_icon(s_cf->type_bitmaps[i]);
#endif
    }
  }
}

// ---- Ring glow ----
// The ring's two arcs turn in opposite directions while the clock is in use. They close into a
// full ring CLOCK_GLOW_IDLE_TIMEOUT_MS after the last press, and the driver stops.
#define CLOCK_GLOW_TURN_MS       4000
#define CLOCK_GLOW_WRAP_TURN_MS  (6 * CLOCK_GLOW_TIMER_MS)
#define CLOCK_GLOW_WRAP_SPIN_MS  (12 * CLOCK_GLOW_TIMER_MS)
#define CLOCK_GLOW_WRAP_CLOSE_MS (4 * CLOCK_GLOW_TIMER_MS)
#define CLOCK_GLOW_WRAP_MS       (CLOCK_GLOW_WRAP_SPIN_MS + CLOCK_GLOW_WRAP_CLOSE_MS)
#define CLOCK_GLOW_OPEN_MS       400

// Milliseconds from the tick counter, which never steps back. time_ms() can step back by most
// of a second where its seconds and sub-seconds come from different counters.
static uint32_t prv_now_ms(void) {
  return (uint32_t)((sys_get_ticks() * 1000 + PBL_TICK_HZ / 2) / PBL_TICK_HZ);
}

// Whether the glow should keep moving: the clock is on top and the ring has not closed.
static bool prv_glow_should_run(void) {
  return s_cf && s_cf->window && window_stack_get_top_window() == s_cf->window &&
         s_cf->glow_idle_ms < CLOCK_GLOW_IDLE_TIMEOUT_MS + CLOCK_GLOW_WRAP_MS;
}

// Advance the glow by dt ms. Returns false when the driver should stop.
static bool prv_glow_advance(uint32_t dt) {
  if (!prv_glow_should_run())
    return false;
  if (dt > 500)
    dt = 500;
  if (s_cf->anim_progress >= ANIMATION_NORMALIZED_MAX) { // held still through the intro
    const uint32_t turn_ms = (s_cf->glow_idle_ms < CLOCK_GLOW_IDLE_TIMEOUT_MS)
                                 ? CLOCK_GLOW_TURN_MS
                                 : CLOCK_GLOW_WRAP_TURN_MS;
    s_cf->glow_phase =
        (s_cf->glow_phase + (uint32_t)TRIG_MAX_ANGLE * dt / turn_ms) % (uint32_t)TRIG_MAX_ANGLE;
    s_cf->glow_idle_ms += dt;
  }
  if (s_cf->canvas)
    layer_mark_dirty(s_cf->canvas);
  return true;
}

// How far beyond their half turns the arcs reach: closing at the end of the idle wrap, and
// opening again after the next press.
static int32_t prv_glow_extra_span(void) {
  const int32_t half = TRIG_MAX_ANGLE / 2;
  const uint32_t closing_from = CLOCK_GLOW_IDLE_TIMEOUT_MS + CLOCK_GLOW_WRAP_SPIN_MS;
  if (s_cf->glow_idle_ms > closing_from) {
    const uint32_t t = MIN(s_cf->glow_idle_ms - closing_from, (uint32_t)CLOCK_GLOW_WRAP_CLOSE_MS);
    return (int32_t)((uint32_t)half * t / CLOCK_GLOW_WRAP_CLOSE_MS);
  }
  const uint32_t t = prv_now_ms() - s_cf->glow_open_ms;
  if (s_cf->glow_open_from <= 0 || t >= CLOCK_GLOW_OPEN_MS)
    return 0;
  // Eases out: quick to start opening, gentle into place.
  const int64_t left = CLOCK_GLOW_OPEN_MS - t;
  return (int32_t)(s_cf->glow_open_from * left * left * left /
                   ((int64_t)CLOCK_GLOW_OPEN_MS * CLOCK_GLOW_OPEN_MS * CLOCK_GLOW_OPEN_MS));
}

#if !PBL_ROUND
static void prv_clock_glow_timer_callback(void *context) {
  (void)context;
  if (!s_cf)
    return;
  s_cf->glow_timer = nullptr;
  const uint32_t now = prv_now_ms();
  const uint32_t dt = now - s_cf->glow_last_ms;
  s_cf->glow_last_ms = now;
  if (prv_glow_advance(dt)) {
    s_cf->glow_timer =
        app_timer_register(CLOCK_GLOW_TIMER_MS, prv_clock_glow_timer_callback, nullptr);
  }
}

static void prv_start_clock_glow(void) {
  if (!s_cf || s_cf->glow_timer)
    return;
  s_cf->glow_last_ms = prv_now_ms();
  s_cf->glow_timer =
      app_timer_register(CLOCK_GLOW_TIMER_MS, prv_clock_glow_timer_callback, nullptr);
}
#else
// Frame-coalesced glow (gabbro): same INFINITE-animation pattern as the forecast icon driver —
// glow stepping rides the animation service's tick, so at most one render per interval even
// while the intro/reveal animations run. (Free-running 50ms beat against the 33ms service and
// the ~20-25ms display period read as judder; see the display research.)
static void prv_glow_drv_stop_cb(void *context) { // 0ms hop: never unschedule inside .update
  (void)context;
  // A press since the stop was scheduled may have restarted the glow
  if (s_cf && s_cf->glow_drv && !prv_glow_should_run()) {
    Animation *a = s_cf->glow_drv;
    s_cf->glow_drv = nullptr;
    animation_unschedule(a);
    animation_destroy(a);
  }
}

static void prv_glow_drv_update(Animation *anim, AnimationProgress p) {
  (void)anim;
  (void)p;
  if (!s_cf)
    return;
  const uint32_t now = prv_now_ms();
  s_cf->glow_drv_acc_ms += now - s_cf->glow_last_ms;
  s_cf->glow_last_ms = now;
  if (s_cf->glow_drv_acc_ms < CLOCK_GLOW_TIMER_MS)
    return;
  const uint32_t dt = s_cf->glow_drv_acc_ms;
  s_cf->glow_drv_acc_ms = 0;
  if (!prv_glow_advance(dt))
    app_timer_register(0, prv_glow_drv_stop_cb, nullptr);
}
static const AnimationImplementation s_glow_drv_impl = {.update = prv_glow_drv_update};

static void prv_start_clock_glow(void) {
  if (!s_cf || s_cf->glow_drv)
    return;
  Animation *a = animation_create();
  if (!a)
    return;
  animation_set_duration(a, ANIMATION_DURATION_INFINITE);
  animation_set_implementation(a, &s_glow_drv_impl);
  s_cf->glow_last_ms = prv_now_ms();
  s_cf->glow_drv_acc_ms = 0;
  s_cf->glow_drv = a;
  animation_schedule(a);
}
#endif

static void prv_note_clock_interaction(void) {
  if (!s_cf)
    return;
  // A closing or closed ring reopens from its current span
  s_cf->glow_open_from = prv_glow_extra_span();
  s_cf->glow_open_ms = prv_now_ms();
  s_cf->glow_idle_ms = 0;
  if (s_cf->canvas)
    layer_mark_dirty(s_cf->canvas);
  prv_start_clock_glow();
}

// Return the absolute 24h hour for a clock-face position.
// Today (day 0): maps to the CURRENT hour + the next 11:
//   position P → the current-or-upcoming hour h where h%12 == P%12.
//   At 11:00: pos 11→11 (now), pos 12→12, pos 1→13, pos 2→14 ... pos 10→22.
// Other days: a fixed daytime window so the morning reads as AM:
//   pos 7→07, 8→08 ... 12→12, 1→13 ... 6→18 (07:00 through 18:00).
// Current hour, localtime()d at most once per SECOND — this helper runs per icon per frame
// (12-24x/frame at up to 20fps via prv_type_for_pos/prv_temp_for_pos), and newlib's
// localtime re-derives timezone/DST every call.
static int prv_cur_hour(void) {
  static time_t cached_now;
  static int cached_hour = -1;
  time_t now = time(nullptr); // rtc read — cheap, unlike localtime
  if (now != cached_now || cached_hour < 0) {
    cached_now = now;
    cached_hour = localtime(&now)->tm_hour;
  }
  return cached_hour;
}

static int prv_abs_hour_for_pos(int pos_1_to_12) {
  if (s_cf && s_cf->day_index != 0) {
    return (pos_1_to_12 >= 7) ? pos_1_to_12 : pos_1_to_12 + 12;
  }
  int cur_h = prv_cur_hour();
  int target_mod = pos_1_to_12 % 12; // pos 12→0, pos 1→1, ..., pos 11→11
  // i starts at 0 so the CURRENT hour lands on its own position (and is the
  // one highlighted) — starting at 1 put the current hour off the dial and
  // highlighted the NEXT hour, reading an hour ahead.
  // Deliberately UNWRAPPED: values >= 24 are tomorrow's hours (h - 24), which
  // the hourly arrays (today only) have no sample for. Wrapping with % 24 made
  // every position past midnight read THIS MORNING's data and present it as
  // tonight's forecast — lookups must bounds-check instead.
  for (int i = 0; i <= 11; i++) {
    int h = cur_h + i;
    if (h % 12 == target_mod)
      return h;
  }
  return pos_1_to_12 % 12; // unreachable
}

// Map a clock-face hour position (1-12) to the WeatherType for that hour.
// Uses real hourly data if available, otherwise falls back to daily forecast.
static WeatherType prv_type_for_pos(int pos_1_to_12) {
  if (!s_cf)
    return WeatherType_Unknown;
  int abs_hour = prv_abs_hour_for_pos(pos_1_to_12);
  if (s_cf->hourly_valid && abs_hour >= 0 && abs_hour < 24) {
    return (WeatherType)s_cf->hourly_types[abs_hour];
  }
  // Post-midnight positions (24-47): tomorrow's hourly series when the record
  // carries it (v4 minor 5); the daily-type fallback below covers its absence.
  if (abs_hour >= 24 && abs_hour < 48 && s_cf->tomorrow_hourly_valid) {
    const uint8_t wt = s_cf->tomorrow_types[abs_hour - 24];
    if (wt <= WeatherType_RainAndSnow)
      return (WeatherType)wt;
  }
  // Fallback: daily
  if (s_cf->num_days == 0)
    return WeatherType_Unknown;
  if (abs_hour >= 24 && s_cf->num_days > 1) {
    // Past midnight — the hourly series covers today only, so use tomorrow's
    // daily type for tonight's small hours.
    return s_cf->days[1].current_weather_type;
  }
  return s_cf->days[0].current_weather_type;
}

// Map a clock-face hour position (1-12) to its chance of precipitation, percent, or -1 where
// it is unknown (an hour past the end of the series, or one the phone had no figure for).
static int prv_precip_for_pos(int pos_1_to_12) {
  if (!s_cf)
    return -1;
  const int abs_hour = prv_abs_hour_for_pos(pos_1_to_12);
  if (abs_hour < 0 || abs_hour >= s_cf->hourly_precip_hours)
    return -1;
  const uint8_t chance = s_cf->hourly_precip[abs_hour];
  return chance <= 100 ? chance : -1;
}

// Map a clock-face hour position (1-12) to the temperature for that hour.
// Returns true and fills *out_temp if hourly temperature data is available.
static bool prv_temp_for_pos(int pos_1_to_12, int *out_temp) {
  if (!s_cf)
    return false;
  int abs_hour = prv_abs_hour_for_pos(pos_1_to_12);
  if (abs_hour >= 24 && abs_hour < 48 && s_cf->tomorrow_hourly_valid) {
    *out_temp = (int)s_cf->tomorrow_temps[abs_hour - 24]; // v4 minor 5
    return true;
  }
  if (!s_cf->hourly_temps_valid || abs_hour < 0 || abs_hour >= 24)
    return false;
  *out_temp = (int)s_cf->hourly_temps[abs_hour];
  return true;
}

// ---- Mode flip (tap) ----
// A tap flips the bubbles one by one clockwise from 7 o'clock: each flip takes CLOCK_FLIP_ITEM
// of the ring's CLOCK_FLIP_MS, the next starting CLOCK_FLIP_STAGGER after it.
#define CLOCK_FLIP_MS      700
#define CLOCK_FLIP_ITEM    (ANIMATION_NORMALIZED_MAX * 45 / 100)
#define CLOCK_FLIP_STAGGER ((ANIMATION_NORMALIZED_MAX - CLOCK_FLIP_ITEM) / 11)
// Duration of the centre's slide on a tap
#define CLOCK_CENTRE_SLIDE_MS 360

static int prv_flip_rank(int pos) {
  return (pos - 7 + 12) % 12; // clockwise: 7 is 0, 8 is 1, ... 6 is 11
}

// Per-position flip progress, 0 to ANIMATION_NORMALIZED_MAX, for a tap's mode change
static int prv_reveal_rp(AnimationProgress reveal_t, int pos) {
  int32_t local = (int32_t)reveal_t - prv_flip_rank(pos) * CLOCK_FLIP_STAGGER;
  if (local <= 0)
    return 0;
  int32_t rp = weather_scale_i32(local, ANIMATION_NORMALIZED_MAX, CLOCK_FLIP_ITEM);
  if (rp > ANIMATION_NORMALIZED_MAX)
    rp = ANIMATION_NORMALIZED_MAX;
  return (int)rp;
}

// Progress, 0 to ANIMATION_NORMALIZED_MAX, of `value` between `from` and `to`, clamped
static int32_t prv_progress_between(int32_t value, int32_t from, int32_t to) {
  if (value <= from)
    return 0;
  if (value >= to)
    return ANIMATION_NORMALIZED_MAX;
  return weather_scale_i32(value - from, ANIMATION_NORMALIZED_MAX, to - from);
}

// A pinching circle's radius: full at either end of a flip, half at its midpoint.
static int prv_flip_radius(int r, int32_t rp) {
  const int32_t mag = (rp <= ANIMATION_NORMALIZED_MAX / 2)
                          ? (ANIMATION_NORMALIZED_MAX - 2 * rp)  // 1 to 0
                          : (2 * rp - ANIMATION_NORMALIZED_MAX); // 0 to 1
  return r - weather_scale_i32(ANIMATION_NORMALIZED_MAX - mag, r / 2, ANIMATION_NORMALIZED_MAX);
}

// ---- Precipitation mode ----
// Each bubble fills with water once its flip ends, as the charging cup in
// shell/normal/battery_ui.c does. The fill is timed apart from the flip, which can then take taps.
#define WATER_FRAME_MS    35
#define WATER_FRAMES      17
#define WATER_SURGE_FROM  6  // s_water_ripple frames where the surface surges up,
#define WATER_SURGE_TO    10 // stretched for a tall rise
#define WATER_RIPPLE_RISE 64
#define WATER_RIPPLE_MAX  8
#define WATER_CUP_RANGE   212 // the cup's empty-to-full travel, 1/8 px
#define WATER_DROP_LEN    4
// The longest fill: a full bubble's surge takes twice its time in s_water_ripple.
#define CLOCK_FILL_MS (WATER_FRAME_MS * (WATER_FRAMES + WATER_SURGE_TO - WATER_SURGE_FROM))
// The frame's colours, from the system theme. Each is a syscall, so they are read once a frame
// and passed down.
typedef struct {
  GColor bg, fg;        // the screen's background and foreground
  GColor bubble;        // a mode bubble's fill
  GColor bubble_stroke; // its outline
  GColor water;         // the precipitation water, and its mode's ring
  GColor water_line;    // the water's surface and the raindrop
} ClockPalette;

static ClockPalette prv_palette(bool dark, GColor fg, GColor bg) {
  return (ClockPalette){
    .bg = bg,
    .fg = fg,
    .bubble = dark ? GColorDarkGray : GColorWhite,
    // On BW strokes cannot dither and LightGray draws as white, so the outline is black or
    // white. In dark mode the water is white too, solid against the dithered bubble.
    .bubble_stroke = dark ? PBL_IF_BW_ELSE(GColorWhite, GColorDarkGray)
                          : PBL_IF_BW_ELSE(GColorBlack, GColorLightGray),
    .water = PBL_IF_COLOR_ELSE(GColorPictonBlue, dark ? GColorWhite : GColorDarkGray),
    // On BW, white in dark mode, as black is lost on the dithered bubble
    .water_line = PBL_IF_COLOR_ELSE(GColorVividCerulean, dark ? GColorWhite : GColorBlack),
  };
}

// The ring leaves a gap for the percent sign, centred on the bottom-right diagonal.
#define PRECIP_GAP_ANGLE (TRIG_MAX_ANGLE * 3 / 8)
#define PRECIP_GAP_HALF  (TRIG_MAX_ANGLE * 26 / 360)
// The centre icon's size. Small rect displays load the 25px icons, the others the 50px ones,
// scaled to 40px on large rect displays.
#define CENTRE_ICON_SIZE PBL_IF_ROUND_ELSE(50, (CLOCK_SMALL_RECT ? 25 : 40))

// When a bubble's flip into the mode ends and its water starts to rise, in ms after the tap
static int32_t prv_fill_start_ms(int pos) {
  return weather_scale_i32(prv_flip_rank(pos) * CLOCK_FLIP_STAGGER + CLOCK_FLIP_ITEM, CLOCK_FLIP_MS,
                           ANIMATION_NORMALIZED_MAX);
}

// The water's height for each 5% of a bubble's area, per-mille of its diameter: a chance fills
// that share of the bubble, not of its height.
static const uint16_t s_area_level[21] = {
  0,   97,  156, 207, 254, 298, 340, 381, 421, 461,  500,
  539, 579, 619, 660, 702, 746, 793, 844, 903, 1000,
};

// The water's height, px, for `percent` in a bubble of inner radius r.
static int prv_level_for_percent(int r, int percent) {
  if (percent >= 100)
    return 2 * r;
  const int i = percent / 5;
  const int pm = s_area_level[i] + (s_area_level[i + 1] - s_area_level[i]) * (percent % 5) / 5;
  return (2 * r * pm + 500) / 1000;
}

// The charging cup's level animation as {left, right} offsets of the surface's ends from its
// final level (positive is lower): a share of the rise out of WATER_RIPPLE_RISE when above
// WATER_RIPPLE_MAX, else 1/8 px on the cup, scaled from WATER_CUP_RANGE to the bubble.
static const int8_t s_water_ripple[WATER_FRAMES][2] = {
  {64, 64}, {64, 64},   {64, 64},   {63, 64},   {62, 63},   {61, 62},  {60, 60}, {60, 30}, {39, -7},
  {1, -20}, {-20, -17}, {-22, -13}, {-21, -12}, {-19, -11}, {-4, -14}, {0, -4},  {0, 0},
};

// The s_water_ripple frame, in Q8, fill_ms into a fill whose surge takes surge_ms
static int32_t prv_water_frame_q8(int32_t fill_ms, int32_t surge_ms) {
  if (fill_ms >= CLOCK_FILL_MS)
    return (int32_t)(WATER_FRAMES - 1) << 8;
  const int32_t surge_at = WATER_SURGE_FROM * WATER_FRAME_MS;
  int32_t q8;
  if (fill_ms < surge_at) {
    q8 = (fill_ms << 8) / WATER_FRAME_MS;
  } else if (fill_ms < surge_at + surge_ms) {
    q8 = (WATER_SURGE_FROM << 8) +
         ((fill_ms - surge_at) << 8) * (WATER_SURGE_TO - WATER_SURGE_FROM) / surge_ms;
  } else {
    q8 = (WATER_SURGE_TO << 8) + ((fill_ms - surge_at - surge_ms) << 8) / WATER_FRAME_MS;
  }
  return MIN(q8, (int32_t)(WATER_FRAMES - 1) << 8);
}

// One end of the surface `frame_q8` (frames, Q8) into the fill, in 1/8 px below where it
// settles, for a rise of rise_q3 (1/8 px) in a bubble of inner radius r.
static int32_t prv_water_end_q3(int32_t frame_q8, int end, int32_t rise_q3, int r) {
  int f = frame_q8 >> 8;
  if (f >= WATER_FRAMES - 1)
    return 0;
  const int32_t frac = frame_q8 & 0xFF;
  int32_t offs[2];
  for (int i = 0; i < 2; i++) {
    const int32_t v = s_water_ripple[f + i][end];
    offs[i] = (v > WATER_RIPPLE_MAX) ? v * rise_q3 / WATER_RIPPLE_RISE
                                     : v * (2 * r * 8) / WATER_CUP_RANGE;
  }
  return offs[0] + (offs[1] - offs[0]) * frac / 256;
}

// n / d rounded up, for d > 0.
static int prv_div_ceil(int n, int d) {
  return n >= 0 ? (n + d - 1) / d : -((-n) / d);
}

// The columns of row y, relative to c.x, that hold water: inside the circle of radius r about c
// and on or below the surface, a line from left_y at x = -r to left_y + fall at x = r. Sets
// *lo > *hi when there are none.
static void prv_water_span(GPoint c, int r, int left_y, int fall, int y, int *lo, int *hi) {
  *lo = 1;
  *hi = 0;
  const int dy = y - c.y;
  if (dy * dy > r * r)
    return;
  const int half = weather_isqrt(r * r - dy * dy);
  int a = -half;
  int b = half;
  // The surface at x is left_y + fall * (x + r) / (2r); this row holds water where that is at
  // or above y.
  const int t = y - left_y;
  if (fall >= 0) {
    if (t < 0)
      return;
    if (fall > 0)
      b = MIN(b, t * 2 * r / fall - r);
  } else {
    a = MAX(a, prv_div_ceil(-t * 2 * r, -fall) - r);
  }
  *lo = a;
  *hi = b;
}

// Water in a bubble of inner radius r, fill_ms into its fill toward `percent`, settled from
// CLOCK_FILL_MS on. A raindrop falls in, then the surface, a line whose ends move separately,
// surges up and settles. The surface is drawn in water_line.
static void prv_draw_water(GContext *ctx, GPoint c, int r, int percent, int32_t fill_ms,
                           const ClockPalette *pal) {
  if (r < 2 || percent <= 0)
    return;
  const int level = prv_level_for_percent(r, percent);
  // A tall rise surges for longer, up to twice as long for a full bubble.
  const int32_t surge_ms =
      (WATER_SURGE_TO - WATER_SURGE_FROM) * WATER_FRAME_MS * (2 * r + level) / (2 * r);
  const int32_t frame_q8 = prv_water_frame_q8(fill_ms, surge_ms);
  const int frame = frame_q8 >> 8;

  // The raindrop falls through the empty bubble while the level holds, gathering speed.
  if (frame >= 1 && frame <= WATER_SURGE_FROM) {
    const int32_t fall = (WATER_SURGE_FROM - 1) * 256;
    const int32_t u = MIN(frame_q8 - 256, fall);
    const int travel = 2 * r - WATER_DROP_LEN;
    const int drop_y = c.y - r + (int)(travel * u * u / (fall * fall));
    graphics_context_set_fill_color(ctx, pal->water_line);
    graphics_fill_rect(ctx, GRect(c.x - 1, drop_y, 2, WATER_DROP_LEN), 0, GCornerNone);
  }

  const int32_t rise_q3 = (int32_t)level * 8;
  const int surface = c.y + r - level;
  const int left_y = surface + (int)(prv_water_end_q3(frame_q8, 0, rise_q3, r) / 8);
  const int right_y = surface + (int)(prv_water_end_q3(frame_q8, 1, rise_q3, r) / 8);
  if (MIN(left_y, right_y) >= c.y + r)
    return; // not risen yet: nothing above the bubble's floor
  // A row at a time. The surface line is each column's top 2 px of water: the part of a row's
  // water that the row 2 px up does not have.
  int up_lo[2] = {1, 1}; // the spans 2 rows and 1 row up, empty to start
  int up_hi[2] = {0, 0};
  for (int y = MAX(MIN(left_y, right_y), c.y - r); y <= c.y + r; y++) {
    int lo, hi;
    prv_water_span(c, r, left_y, right_y - left_y, y, &lo, &hi);
    const int lo2 = up_lo[0];
    const int hi2 = up_hi[0];
    up_lo[0] = up_lo[1];
    up_hi[0] = up_hi[1];
    up_lo[1] = lo;
    up_hi[1] = hi;
    if (lo > hi)
      continue;
    graphics_context_set_fill_color(ctx, pal->water);
    graphics_fill_rect(ctx, GRect(c.x + lo, y, hi - lo + 1, 1), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, pal->water_line);
    if (lo2 > hi2) {
      graphics_fill_rect(ctx, GRect(c.x + lo, y, hi - lo + 1, 1), 0, GCornerNone);
      continue;
    }
    if (lo < lo2) {
      const int end = MIN(hi, lo2 - 1);
      graphics_fill_rect(ctx, GRect(c.x + lo, y, end - lo + 1, 1), 0, GCornerNone);
    }
    if (hi > hi2) {
      const int start = MAX(lo, hi2 + 1);
      graphics_fill_rect(ctx, GRect(c.x + start, y, hi - start + 1, 1), 0, GCornerNone);
    }
  }
}

// ---- Text stamps ----
// Each string is laid out once into a 1-bit mask and stamped from it in any colour, as laying
// text out is the costly part of a dial frame.
#define TEXT_STAMP_MARGIN 4 // room for glyphs drawn past the laid-out text

// Copy the screen's pixels in `area` (layer coordinates) to or from buf, a byte a pixel.
static void prv_fb_area(GContext *ctx, GBitmap *fb, GRect area, uint8_t *buf, bool write) {
  const GPoint o = ctx->draw_state.drawing_box.origin;
  const int fb_h = gbitmap_get_bounds(fb).size.h;
  for (int j = 0; j < area.size.h; j++) {
    const int y = o.y + area.origin.y + j;
    if (y < 0 || y >= fb_h)
      continue;
    const GBitmapDataRowInfo ri = gbitmap_get_data_row_info(fb, (uint16_t)y);
    for (int i = 0; i < area.size.w; i++) {
      const int x = o.x + area.origin.x + i;
      if (x < (int)ri.min_x || x > (int)ri.max_x)
        continue;
      uint8_t *p = &buf[j * area.size.w + i];
      if (write)
        weather_fb_row_set(ri.data, x, *p);
      else
        *p = weather_fb_row_get(ri.data, x);
    }
  }
}

// The stamp for `text` in `font`, made on first use: the text is drawn in the middle of the
// layer to read its pixels, then the saved screen area is restored. NULL if that fails.
static const TextStamp *prv_text_stamp(GContext *ctx, const char *text, GFont font) {
  for (int i = 0; i < TEXT_STAMP_SLOTS; i++) {
    const TextStamp *st = &s_cf->stamps[i];
    if (st->bits && st->font == font && strcmp(st->text, text) == 0)
      return st;
  }
  if (strlen(text) >= sizeof(s_cf->stamps[0].text))
    return nullptr;
  const GSize size = graphics_text_layout_get_content_size(
      text, font, GRect(0, 0, 100, 60), GTextOverflowModeFill, GTextAlignmentLeft);
  if (size.w <= 0 || size.h <= 0 || size.w > 100 || size.h > 60)
    return nullptr;
  const GSize layer = ctx->draw_state.drawing_box.size;
  const int m = TEXT_STAMP_MARGIN;
  const GRect box = GRect(layer.w / 2 - size.w / 2, layer.h / 2 - size.h / 2, size.w, size.h);
  const GRect area =
      GRect(box.origin.x - m, box.origin.y - m, box.size.w + 2 * m, box.size.h + 2 * m);
  uint8_t *saved = malloc_try((size_t)area.size.w * area.size.h);
  if (!saved)
    return nullptr;
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) {
    free(saved);
    return nullptr;
  }
  prv_fb_area(ctx, fb, area, saved, false);
  graphics_release_frame_buffer(ctx, fb);

  const GColor bg = system_theme_get_bg_color();
  const GDrawState state = ctx->draw_state;
  ctx->draw_state.clip_box = ctx->draw_state.drawing_box; // the full-screen canvas
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, area, 0, GCornerNone);
  graphics_context_set_text_color(ctx, system_theme_get_fg_color());
  graphics_draw_text(ctx, text, font, GRect(box.origin.x, box.origin.y, box.size.w + m, box.size.h),
                     GTextOverflowModeFill, GTextAlignmentLeft, nullptr);
  ctx->draw_state = state;

  fb = graphics_capture_frame_buffer(ctx);
  if (!fb) {
    free(saved);
    return nullptr;
  }
  uint8_t *drawn = malloc_try((size_t)area.size.w * area.size.h);
  TextStamp *st = nullptr;
  if (drawn) {
    prv_fb_area(ctx, fb, area, drawn, false);
    int x0 = area.size.w, y0 = area.size.h, x1 = -1, y1 = -1;
    for (int j = 0; j < area.size.h; j++) {
      for (int i = 0; i < area.size.w; i++) {
        if (drawn[j * area.size.w + i] == bg.argb)
          continue;
        x0 = MIN(x0, i);
        x1 = MAX(x1, i);
        y0 = MIN(y0, j);
        y1 = MAX(y1, j);
      }
    }
    const int w = (x1 >= x0) ? x1 - x0 + 1 : 0;
    const int h = (y1 >= y0) ? y1 - y0 + 1 : 0;
    const int stride = (w + 7) / 8;
    uint8_t *bits = app_zalloc((size_t)MAX(1, stride * h)); // may fail: the caller falls back
    if (bits) {
      for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
          if (drawn[(y0 + j) * area.size.w + x0 + i] != bg.argb)
            bits[j * stride + i / 8] |= (uint8_t)(1 << (i % 8));
        }
      }
      st = &s_cf->stamps[s_cf->stamp_next];
      s_cf->stamp_next = (uint8_t)((s_cf->stamp_next + 1) % TEXT_STAMP_SLOTS);
      free(st->bits);
      *st = (TextStamp){
        .bits = bits,
        .font = font,
        .lw = (uint8_t)size.w,
        .ox = (int8_t)(x0 - m),
        .oy = (int8_t)(y0 - m),
        .w = (uint8_t)w,
        .h = (uint8_t)h,
      };
      strncpy(st->text, text, sizeof(st->text) - 1);
    }
    free(drawn);
  }
  prv_fb_area(ctx, fb, area, saved, true);
  graphics_release_frame_buffer(ctx, fb);
  free(saved);
  return st;
}

// Draw st's mask in `color`, its laid-out text's top left at `at`, within the clip
static void prv_stamp_text(GContext *ctx, const TextStamp *st, GPoint at, GColor color) {
  if (!st->w || !st->h)
    return;
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb)
    return;
  const GPoint o = ctx->draw_state.drawing_box.origin;
  const GRect clip = ctx->draw_state.clip_box;
  const int fb_h = gbitmap_get_bounds(fb).size.h;
  const int x0 = o.x + at.x + st->ox;
  const int y0 = o.y + at.y + st->oy;
  const int stride = (st->w + 7) / 8;
  const uint8_t ink = color.argb; // on BW weather_fb_row_set draws any colour but black as white
  for (int j = 0; j < st->h; j++) {
    const int y = y0 + j;
    if (y < 0 || y >= fb_h || y < clip.origin.y || y >= clip.origin.y + clip.size.h)
      continue;
    const GBitmapDataRowInfo ri = gbitmap_get_data_row_info(fb, (uint16_t)y);
    const int lo = MAX((int)clip.origin.x, (int)ri.min_x);
    const int hi = MIN(clip.origin.x + clip.size.w - 1, (int)ri.max_x);
    const uint8_t *bits = st->bits + j * stride;
    for (int i = 0; i < st->w; i++) {
      const int x = x0 + i;
      if (x < lo || x > hi || !(bits[i / 8] & (1 << (i % 8))))
        continue;
      weather_fb_row_set(ri.data, x, ink);
    }
  }
  graphics_release_frame_buffer(ctx, fb);
}

// Draw one line of text in `box` as graphics_draw_text would, in `color`, from its stamp.
static void prv_draw_text(GContext *ctx, const char *text, GFont font, GRect box,
                          GTextAlignment align, GColor color) {
  const TextStamp *st = prv_text_stamp(ctx, text, font);
  if (!st) {
    graphics_context_set_text_color(ctx, color);
    graphics_draw_text(ctx, text, font, box, GTextOverflowModeFill, align, nullptr);
    return;
  }
  int x = box.origin.x;
  if (align == GTextAlignmentCenter)
    x += (box.size.w - st->lw) / 2;
  else if (align == GTextAlignmentRight)
    x += box.size.w - st->lw;
  prv_stamp_text(ctx, st, GPoint(x, box.origin.y), color);
}

// The laid-out width of one line of text.
static int prv_text_width(GContext *ctx, const char *text, GFont font) {
  const TextStamp *st = prv_text_stamp(ctx, text, font);
  if (st)
    return st->lw;
  return graphics_text_layout_get_content_size(text, font, GRect(0, 0, 100, 60),
                                               GTextOverflowModeFill, GTextAlignmentLeft)
      .w;
}

// ---- Centre ----
// The weather icon for `type`, scaled to CENTRE_ICON_SIZE in a heap copy, as the resource may be
// mapped read-only. NULL without the memory for the copy.
static GDrawCommandImage *prv_load_centre_icon(WeatherType type) {
  GDrawCommandImage *raw =
      gdraw_command_image_create_with_resource(weather_type_pdc_resource(type, !CLOCK_SMALL_RECT));
  if (!raw)
    return nullptr;
  const size_t size = gdraw_command_image_get_data_size(raw);
  GDrawCommandImage *icon = malloc_try(size);
  if (icon && !gdraw_command_image_copy(icon, size, raw)) {
    free(icon);
    icon = nullptr;
  }
  gdraw_command_image_destroy(raw);
  if (icon && gdraw_command_image_get_bounds_size(icon).w != CENTRE_ICON_SIZE)
    gdraw_command_image_scale(icon, GSize(CENTRE_ICON_SIZE, CENTRE_ICON_SIZE));
  weather_recolor_pdc_image_for_dark_mode(icon);
  return icon;
}

// The weather the centre's icon and ring show: the current hour's on today's dial, else the
// day's.
static WeatherType prv_centre_weather_type(void) {
  if (s_cf->day_index == 0) {
    const int pos = prv_cur_hour() % 12;
    return prv_type_for_pos(pos == 0 ? 12 : pos);
  }
  return s_cf->num_days > 0 ? s_cf->days[s_cf->day_index].current_weather_type
                            : WeatherType_Unknown;
}

// Today's current temperature by the main screen's rule, applied at the hour on the dial: the
// phone's reading while it is from this hour, then the hour's forecast.
static int prv_current_temp(void) {
  const WeatherLocationForecast *today = &s_cf->days[0];
  const int hour = prv_cur_hour();
  const bool has_forecast = s_cf->hourly_valid && s_cf->hourly_temps_valid &&
                            s_cf->hourly_types[hour] <= WeatherType_RainAndSnow;
  if (!has_forecast || weather_same_local_hour(time(nullptr), today->time_updated_utc))
    return today->current_temp_now;
  return s_cf->hourly_temps[hour];
}

static GFont prv_sign_font(void) {
  static GFont s_font;
  if (!s_font)
    s_font =
        fonts_get_system_font(CLOCK_SMALL_RECT ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_24_BOLD);
  return s_font;
}

// Whether the centre has anything to show for `mode`.
static bool prv_centre_valid(ClockFaceMode mode) {
  switch (mode) {
    case ClockFaceModeTemps:
      // Today shows the current temperature, other days their weekday.
      return s_cf->day_index != 0 ||
             (s_cf->num_days > 0 &&
              s_cf->days[0].current_temp_now != WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP);
    case ClockFaceModePrecip:
      return s_cf->hourly_precip_hours >= 24;
    default:
      return true;
  }
}

static void prv_free_centre_image(CentreImage *img) {
  free(img->pixels);
  img->pixels = nullptr;
}

// Copy `box` of the screen, drawn on the theme's background, into img with its pivot at (cx, cy)
// in the box, and clear the box again.
static void prv_capture_centre_image(GContext *ctx, GRect box, int cx, int cy, CentreImage *img) {
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (fb) {
    // On BW the capture is 1-bit, rows padded to a multiple of 4 bytes as in a blank bitmap
    const uint16_t stride =
        PBL_IF_COLOR_ELSE((uint16_t)box.size.w, (uint16_t)(((box.size.w + 31) / 32) * 4));
    uint8_t *pixels = malloc_try((size_t)stride * box.size.h);
    if (pixels) {
      img->bmp = (GBitmap){0};
      gbitmap_set_data(&img->bmp, pixels, PBL_IF_COLOR_ELSE(GBitmapFormat8Bit, GBitmapFormat1Bit),
                       stride, false);
      gbitmap_set_bounds(&img->bmp, GRect(0, 0, box.size.w, box.size.h));
      weather_capture_framebuffer_rect(fb, &img->bmp, box, 0);
      img->pixels = pixels;
      img->cx = (int16_t)cx;
      img->cy = (int16_t)cy;
    }
    graphics_release_frame_buffer(ctx, fb);
  }
  graphics_context_set_fill_color(ctx, system_theme_get_bg_color());
  graphics_fill_rect(ctx, box, 0, GCornerNone);
}

// A centre figure, `text` in `font`, pivoting on its visual centre.
static void prv_capture_text(GContext *ctx, GPoint c, const char *text, GFont font,
                             CentreImage *img) {
  const GSize nsz = graphics_text_layout_get_content_size(
      text, font, GRect(0, 0, 100, 50), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
  const int pad = 4;
  const int font_top_pad = -4;
  const int w = pad + nsz.w + pad;
  const int h = nsz.h + 8;
  const GRect box = GRect(c.x - w / 2, c.y - h / 2, w, h);
  const int ty = h / 2 - nsz.h / 2 + font_top_pad;
  graphics_context_set_text_color(ctx, system_theme_get_fg_color());
  graphics_draw_text(ctx, text, font,
                     GRect(box.origin.x + pad, box.origin.y + ty, nsz.w + 2, nsz.h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, nullptr);
  prv_capture_centre_image(ctx, box, pad + nsz.w / 2, h / 2 - font_top_pad / 2, img);
}

// A centre icon: the official icon for `type`, pivoting on its middle.
static void prv_capture_icon(GContext *ctx, GPoint c, WeatherType type, CentreImage *img) {
  GDrawCommandImage *icon = prv_load_centre_icon(type);
  if (!icon)
    return;
  const int m = 2; // room for strokes past the icon's bounds
  const int half = CENTRE_ICON_SIZE / 2;
  const GRect box =
      GRect(c.x - half - m, c.y - half - m, CENTRE_ICON_SIZE + 2 * m, CENTRE_ICON_SIZE + 2 * m);
  gdraw_command_image_draw(ctx, icon, GPoint(box.origin.x + m, box.origin.y + m));
  gdraw_command_image_destroy(icon);
  prv_capture_centre_image(ctx, box, half + m, half + m, img);
}

// Capture `mode`'s centre content, staged at c on the still-blank screen.
static void prv_capture_centre(GContext *ctx, GPoint c, ClockFaceMode mode) {
  CentreImage *img = &s_cf->centre[mode];
  char buf[8];
  switch (mode) {
    case ClockFaceModeIcons:
      prv_capture_icon(ctx, c, s_cf->centre_type, img);
      break;
    case ClockFaceModeTemps:
      if (s_cf->day_index == 0) {
        snprintf(buf, sizeof(buf), "%d", prv_current_temp());
        prv_capture_text(ctx, c, buf, s_cf->temp_font, img);
      } else {
        weather_fill_weekday_abbrev(s_cf->day_index, nullptr, buf, sizeof(buf));
        prv_capture_text(ctx, c, buf, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK), img);
      }
      break;
    case ClockFaceModePrecip:
      // With the percent sign in the ring, the rain icon reads as chance of precipitation
      prv_capture_icon(ctx, c, WeatherType_LightRain, img);
      break;
    default:
      break;
  }
}

// A centre image's size, per-mille of its capture, as the intro grows it (grow_p).
static int32_t prv_centre_scale_pm(ClockFaceMode mode, int32_t grow_p) {
  int32_t pm = 1000;
  if (mode == ClockFaceModeTemps) {
    pm = (s_cf->day_index == 0) ? 1100 : 800;
  }
  return pm * grow_p / ANIMATION_NORMALIZED_MAX;
}

// Draw a centre image at scale_pm per-mille, its pivot at `at`. With mask_r >= 0 only its pixels
// within mask_r of c show: the ring's inside, while the centre slides.
static void prv_draw_centre_image(GContext *ctx, const CentreImage *img, GPoint at,
                                  int32_t scale_pm, GPoint c, int mask_r, const ClockPalette *pal) {
  if (!img->pixels)
    return;
  const uint8_t bg_argb = pal->bg.argb;
#if PBL_BW
  const bool bg_bit = (bg_argb & 0x3F) != 0; // the test weather_fb_row_set uses
  const uint8_t fg_argb = pal->fg.argb;
#endif
  const GSize src = gbitmap_get_bounds(&img->bmp).size;
  const int dst_w = MAX(1, src.w * scale_pm / 1000);
  const int dst_h = MAX(1, src.h * scale_pm / 1000);
  const int x0 = at.x - img->cx * dst_w / src.w;
  const int y0 = at.y - img->cy * dst_h / src.h;
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb)
    return;
  const int fb_h = gbitmap_get_bounds(fb).size.h;
  const uint8_t *sdata = gbitmap_get_data(&img->bmp);
  const uint16_t sbpr = gbitmap_get_bytes_per_row(&img->bmp);
  const int32_t sx_step = ((int32_t)src.w << 16) / dst_w;
  const int32_t sy_step = ((int32_t)src.h << 16) / dst_h;
  int32_t sy_fp = sy_step >> 1;
  for (int dy = 0; dy < dst_h; dy++, sy_fp += sy_step) {
    const int ay = y0 + dy;
    if (ay < 0 || ay >= fb_h)
      continue;
    const GBitmapDataRowInfo ri = gbitmap_get_data_row_info(fb, (uint16_t)ay);
    int lo = MAX(x0, (int)ri.min_x);
    int hi = MIN(x0 + dst_w - 1, (int)ri.max_x);
    if (mask_r >= 0) {
      const int d = ay - c.y;
      if (d * d > mask_r * mask_r)
        continue;
      const int half = weather_isqrt(mask_r * mask_r - d * d);
      lo = MAX(lo, c.x - half);
      hi = MIN(hi, c.x + half);
    }
    const int sy = MIN(sy_fp >> 16, src.h - 1);
    const uint8_t *srow = sdata + (uint32_t)sy * sbpr;
    int32_t sx_fp = (sx_step >> 1) + (lo - x0) * sx_step;
    for (int ax = lo; ax <= hi; ax++, sx_fp += sx_step) {
      const int sx = MIN(sx_fp >> 16, src.w - 1);
      // The theme's background is transparent. The capture is 8-bit on colour, 1-bit on BW.
#if PBL_BW
      if ((pbl_bitset8_get(srow, (unsigned)sx) != 0) == bg_bit)
        continue;
      weather_fb_row_set(ri.data, ax, fg_argb);
#else
      if (srow[sx] == bg_argb)
        continue;
      weather_fb_row_set(ri.data, ax, srow[sx]);
#endif
    }
  }
  graphics_release_frame_buffer(ctx, fb);
}

// Where the percent sign's gap sits on the ring: the bottom-right diagonal. On small rect
// displays the current-hour marker at 4 o'clock covers that spot, so for that hour the gap
// moves down.
static int32_t prv_percent_gap_angle(void) {
#if CLOCK_SMALL_RECT
  if (s_cf->day_index == 0 && prv_cur_hour() % 12 == 4)
    return TRIG_MAX_ANGLE * 160 / 360;
#endif
  return PRECIP_GAP_ANGLE;
}

// The percent sign in a gap cleared from the ring, its glyph centred on the ring's line
static void prv_draw_percent_gap(GContext *ctx, GPoint c, int ring_r, const ClockPalette *pal) {
  const int32_t angle = prv_percent_gap_angle();
  graphics_context_set_fill_color(ctx, pal->bg);
  graphics_fill_radial(
      ctx, GRect(c.x - ring_r - 2, c.y - ring_r - 2, ring_r * 2 + 4, ring_r * 2 + 4),
      GOvalScaleModeFitCircle, 6, angle - PRECIP_GAP_HALF, angle + PRECIP_GAP_HALF);
  const int gx = c.x + (int)((int32_t)sin_lookup(angle) * ring_r / TRIG_MAX_RATIO);
  const int gy = c.y - (int)((int32_t)cos_lookup(angle) * ring_r / TRIG_MAX_RATIO);
  const TextStamp *st = prv_text_stamp(ctx, "%", prv_sign_font());
  if (st) {
    prv_stamp_text(ctx, st, GPoint(gx - st->ox - st->w / 2, gy - st->oy - st->h / 2), pal->fg);
  } else {
    graphics_context_set_text_color(ctx, pal->fg);
    graphics_draw_text(ctx, "%", prv_sign_font(), GRect(gx - 12, gy - 16, 24, 24),
                       GTextOverflowModeFill, GTextAlignmentCenter, nullptr);
  }
}

// The ring's colour for a mode: the centre weather's, or the water's. A weather whose colour is
// the theme's background (black skies on BW in dark mode) takes the foreground instead.
static GColor prv_ring_color(ClockFaceMode mode, const ClockPalette *pal) {
  if (mode == ClockFaceModePrecip)
    return pal->water;
  GColor weather = prv_bg_color_for_type(s_cf->centre_type);
#if PBL_BW
  weather = gcolor_get_grayscale(weather); // what the 1-bit renderer draws
#endif
  return gcolor_equal(weather, pal->bg) ? pal->fg : weather;
}

// The ring in `mode`'s colour over layer columns x0 to x1 - 1 only, with the percent gap when it
// is the water's.
static void prv_draw_ring_part(GContext *ctx, GPoint c, int ring_r, ClockFaceMode mode, int x0,
                               int x1, bool idle, int32_t extra_span, const ClockPalette *pal) {
  const GDrawState saved = ctx->draw_state;
  GRect *clip = &ctx->draw_state.clip_box;
  const int ox = ctx->draw_state.drawing_box.origin.x;
  const int left = MAX((int)clip->origin.x, ox + x0);
  const int right = MIN(clip->origin.x + clip->size.w, ox + x1);
  if (right > left) {
    clip->origin.x = (int16_t)left;
    clip->size.w = (int16_t)(right - left);
    weather_draw_lava_ring(ctx, c, ring_r, prv_ring_color(mode, pal), s_cf->glow_phase, idle,
                           extra_span);
    if (mode == ClockFaceModePrecip)
      prv_draw_percent_gap(ctx, c, ring_r, pal);
  }
  ctx->draw_state = saved;
}

// Cubic ease-out for the centre's slide
static int32_t prv_slide_ease(int32_t t) {
  const int64_t max = ANIMATION_NORMALIZED_MAX;
  const int64_t q = max - t;
  return (int32_t)(max - q * q * q / (max * max));
}

// ---- Intro animation (expand from dot + clockwise spin) ----

static void prv_intro_update(Animation *anim, AnimationProgress progress) {
  if (s_cf) {
    if (progress > ANIMATION_NORMALIZED_MAX * 99 / 100) {
      progress = ANIMATION_NORMALIZED_MAX;
    }
    s_cf->anim_progress = progress;
    if (s_cf->canvas)
      layer_mark_dirty(s_cf->canvas);
  }
}
static const AnimationImplementation s_intro_impl = {.update = prv_intro_update};

// ---- (Removed) clock -> detail_face exit/return animation ----
// The full-screen weather detail view (temperature + UV/rain + location + last-updated) was made
// redundant by the expanded weather card, so the clock's drill-in was removed. The exit_/return_
// struct fields remain (never set now) so the draw proc's dead branches keep compiling harmlessly.

// ---- Mode flip animation (tap) ----
static void prv_queued_tap(void *context);

static void prv_reveal_update(Animation *anim, AnimationProgress progress) {
  if (!s_cf)
    return;
  s_cf->reveal_p = progress;
  if (progress >= ANIMATION_NORMALIZED_MAX && s_cf->reveal_active) {
    s_cf->reveal_active = false;
    s_cf->mode = s_cf->mode_to;
    // Play a tap queued during the flip from a timer: never reschedule inside .update
    if (s_cf->tap_queued && !s_cf->tap_timer)
      s_cf->tap_timer = app_timer_register(0, prv_queued_tap, nullptr);
    s_cf->tap_queued = false;
  }
  if (s_cf->canvas)
    layer_mark_dirty(s_cf->canvas);
}
static const AnimationImplementation s_reveal_impl = {.update = prv_reveal_update};

// Redraws while the water rises, which outlasts the flip.
static void prv_fill_update(Animation *anim, AnimationProgress progress) {
  (void)anim;
  (void)progress;
  if (s_cf && s_cf->canvas)
    layer_mark_dirty(s_cf->canvas);
}
static const AnimationImplementation s_fill_impl = {.update = prv_fill_update};

static bool prv_mode_available(ClockFaceMode mode) {
  switch (mode) {
    case ClockFaceModeTemps:
      return s_cf->hourly_temps_valid;
    case ClockFaceModePrecip:
      return s_cf->hourly_precip_hours >= 24;
    default:
      return true;
  }
}

// Advance the ring to its next mode, skipping modes without their hourly series
static void prv_advance_mode(void) {
  if (!s_cf)
    return;
  if (s_cf->anim_progress < ANIMATION_NORMALIZED_MAX)
    return; // wait for intro to finish
  if (s_cf->reveal_active) {
    s_cf->tap_queued = true; // played once the flip ends
    return;
  }
  ClockFaceMode next = s_cf->mode;
  do {
    next = (ClockFaceMode)((next + 1) % ClockFaceModeCount);
  } while (!prv_mode_available(next));
  if (next == s_cf->mode)
    return; // nothing else to show
  s_cf->mode_to = next;
  s_cf->reveal_active = true;
  s_cf->reveal_p = 0;
  if (s_cf->reveal_anim) {
    animation_unschedule(s_cf->reveal_anim);
    animation_destroy(s_cf->reveal_anim);
  }
  s_cf->reveal_anim = animation_create();
  animation_set_auto_destroy(s_cf->reveal_anim, false); // destroyed here or at unload
  animation_set_implementation(s_cf->reveal_anim, &s_reveal_impl);
  animation_set_duration(s_cf->reveal_anim, CLOCK_FLIP_MS);
  animation_set_curve(s_cf->reveal_anim, AnimationCurveLinear);
  animation_schedule(s_cf->reveal_anim);
  if (next == ClockFaceModePrecip) {
    s_cf->fill_started_ms = prv_now_ms();
    if (s_cf->fill_anim) {
      animation_unschedule(s_cf->fill_anim);
      animation_destroy(s_cf->fill_anim);
    }
    s_cf->fill_anim = animation_create();
    animation_set_auto_destroy(s_cf->fill_anim, false);
    animation_set_implementation(s_cf->fill_anim, &s_fill_impl);
    animation_set_duration(s_cf->fill_anim, prv_fill_start_ms(6) + CLOCK_FILL_MS);
    animation_schedule(s_cf->fill_anim);
  }
}

static void prv_queued_tap(void *context) {
  (void)context;
  if (!s_cf)
    return;
  s_cf->tap_timer = nullptr;
  prv_advance_mode();
}

static void prv_free_centre_images(void) {
  for (int m = 0; m < ClockFaceModeCount; m++)
    prv_free_centre_image(&s_cf->centre[m]);
}

static void prv_free_stamps(void) {
  for (int i = 0; i < TEXT_STAMP_SLOTS; i++) {
    free(s_cf->stamps[i].bits);
    s_cf->stamps[i].bits = nullptr;
  }
  s_cf->stamp_next = 0;
}

// Update the centre weather for the current hour, recapturing the icon when it changes
static void prv_refresh_centre(void) {
  const WeatherType type = prv_centre_weather_type();
  if (type != s_cf->centre_type) {
    s_cf->centre_type = type;
    prv_free_centre_image(&s_cf->centre[ClockFaceModeIcons]);
  }
}

static void prv_play_animation(void) {
  if (!s_cf)
    return;
  if (s_cf->intro_anim) {
    animation_unschedule(s_cf->intro_anim);
    animation_destroy(s_cf->intro_anim);
    s_cf->intro_anim = nullptr;
  }
  s_cf->anim_progress = 0;
  // The centre's content is captured again on the first frame.
  prv_free_centre_images();
  s_cf->intro_anim = animation_create();
  animation_set_auto_destroy(s_cf->intro_anim, false);
  animation_set_implementation(s_cf->intro_anim, &s_intro_impl);
  animation_set_duration(s_cf->intro_anim, CLOCK_INTRO_DURATION_MS);
  animation_set_curve(s_cf->intro_anim, AnimationCurveLinear);
  animation_schedule(s_cf->intro_anim);
}

// ---- Draw proc ----
static int32_t prv_clock_intro_motion_progress(AnimationProgress progress) {
  const int32_t max = ANIMATION_NORMALIZED_MAX;
  if ((int32_t)progress >= max)
    return max;

  const int32_t overshoot = max / 34;
  const int32_t rebound = max / 135;
  const int32_t hit = max * 78 / 100;
  const int32_t settle = max * 93 / 100;

  if ((int32_t)progress < hit) {
    return weather_scale_i32(progress, max + overshoot, hit);
  }

  if ((int32_t)progress < settle) {
    return max + overshoot -
           weather_scale_i32((int32_t)progress - hit, overshoot + rebound, settle - hit);
  }

  return max - rebound + weather_scale_i32((int32_t)progress - settle, rebound, max - settle);
}

static int32_t prv_clock_center_scale_progress(AnimationProgress progress) {
  const int32_t max = ANIMATION_NORMALIZED_MAX;
  if ((int32_t)progress >= max)
    return max;
  const int32_t min_scale = max / 40; // start from a point — blooms straight out of the dot
  // Clean ease-out from the point to full size. No overshoot/settle phase (that tail wobble was
  // dropping the centre temperature for a frame).
  const int32_t q = max - (int32_t)progress;
  const int32_t ease_out = max - (int32_t)((int64_t)q * q / max);
  return min_scale + weather_scale_i32(ease_out, max - min_scale, max);
}

// ---- UP → forecast: whole-screen Timeline squash-stretch ---------------------------------
// The whole-screen jelly squash-stretch lives in weather_math.c (weather_render_squash),
// shared with the forecast's squash modes. CLOCK_EXIT is the unhasted down-exit: unlike the
// forecast's DOWN_EXIT it runs the full timeline (no me=m+m/3 compression) — hand-tuned so
// the sunset-card staging reads right; do not switch modes.
// BOTH shapes: the renderer clamps every write to the row's chord, so round squashes inside
// the glass. The scratch is W*H (round 260x260 = 67,600B vs rect 200x228 = 45,600B); the
// malloc_try fallback below degrades to an instant return if the app heap can't take it.
static void prv_render_fwd_squash(GContext *ctx) {
  if (!s_cf || !s_cf->fwd_scratch)
    return;
  weather_render_squash(ctx, s_cf->fwd_scratch, s_cf->fwd_exit_p, WEATHER_SQUASH_CLOCK_EXIT);
#if PBL_ROUND
  // Capture-once (gabbro smoothness step 2): the snapshot now holds the fully-drawn
  // clock — from the next frame prv_canvas_draw short-circuits to resample-only.
  s_cf->fwd_captured = true;
#endif
}

static void prv_fwd_exit_push_callback(void *ctx);
static void prv_fwd_exit_update(Animation *anim, AnimationProgress progress) {
  if (!s_cf)
    return;
  s_cf->fwd_exit_p = progress;
  if (s_cf->canvas)
    layer_mark_dirty(s_cf->canvas);
  if (progress >= ANIMATION_NORMALIZED_MAX && !s_cf->fwd_push_timer) {
    s_cf->fwd_push_timer = app_timer_register(0, prv_fwd_exit_push_callback, nullptr);
  }
}
static const AnimationImplementation s_fwd_exit_impl = {.update = prv_fwd_exit_update};

static void prv_fwd_exit_push_callback(void *ctx) {
  if (!s_cf)
    return;
  s_cf->fwd_push_timer = nullptr;
  if (s_cf->fwd_scratch) {
    free(s_cf->fwd_scratch);
    s_cf->fwd_scratch = nullptr;
  }
  // Hand off to weather.c, which dismisses this clock and jelly-rises the forecast back in.
  if (s_wrap_callback)
    s_wrap_callback(s_wrap_context);
}

static void prv_start_fwd_exit_animation(void) {
  if (!s_cf || s_cf->fwd_exit_active || !s_cf->canvas)
    return;
  GRect bounds = layer_get_bounds(s_cf->canvas);
  s_cf->fwd_scratch = malloc_try((size_t)bounds.size.w * (size_t)bounds.size.h);
  if (!s_cf->fwd_scratch) { // out of heap: fall back to an instant return
    if (s_wrap_callback)
      s_wrap_callback(s_wrap_context);
    return;
  }
  s_cf->fwd_exit_active = true;
  s_cf->fwd_captured = false; // round capture-once: fresh exit, fresh snapshot
  s_cf->fwd_exit_p = 0;
  if (s_cf->fwd_exit_anim) {
    animation_unschedule(s_cf->fwd_exit_anim);
    animation_destroy(s_cf->fwd_exit_anim);
  }
  s_cf->fwd_exit_anim = animation_create();
  animation_set_implementation(s_cf->fwd_exit_anim, &s_fwd_exit_impl);
  animation_set_duration(s_cf->fwd_exit_anim, interpolate_moook_soft_duration(3));
  animation_set_curve(s_cf->fwd_exit_anim, AnimationCurveLinear);
  animation_schedule(s_cf->fwd_exit_anim);
}

static void prv_canvas_draw(Layer *layer, GContext *ctx) {
#if PBL_ROUND
  // Transparent window bg on round (step 3) — the degenerate no-state frame must paint
  // the white itself, since this proc is what guarantees full coverage.
  if (!s_cf) {
    graphics_context_set_fill_color(ctx, system_theme_get_bg_color());
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
    return;
  }
#else
  if (!s_cf)
    return;
#endif
#if PBL_ROUND
  // Capture-once (gabbro smoothness step 2): during the UP-to-forecast squash the clock
  // scene is frozen in fwd_scratch after the first frame — skip the whole clock render
  // and only resample. Nothing draws after the squash in this proc (it is the last call),
  // so a full short-circuit is safe; the resample overwrites every pixel.
  if (s_cf->fwd_exit_active && s_cf->fwd_captured && s_cf->fwd_scratch) {
    weather_render_squash_cached(ctx, s_cf->fwd_scratch, s_cf->fwd_exit_p,
                                 WEATHER_SQUASH_CLOCK_EXIT);
    return;
  }
#endif
  GRect bounds = layer_get_bounds(layer);
  int W = bounds.size.w;
  int H = bounds.size.h;

  // Only do this once a draw
  const bool isDark = system_theme_is_dark_mode();
  const GColor fg_color = system_theme_get_fg_color();
  const GColor bg_color = system_theme_get_bg_color();
  const ClockPalette pal = prv_palette(isDark, fg_color, bg_color);

  // Background — matches other screens
  graphics_context_set_fill_color(ctx, bg_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Clock face centre — full screen, no bar offset
  int cx = W / 2;
  int cy = H / 2;

  const bool is_today = (s_cf->day_index == 0);

  AnimationProgress p = s_cf->anim_progress;
  int32_t intro_p = prv_clock_intro_motion_progress(p);
  int32_t centre_scale_p = prv_clock_center_scale_progress(p);
  bool anim_done = (p >= ANIMATION_NORMALIZED_MAX);
  // Mode faces, used once the intro has finished while the ring shows a mode other than the
  // weather icons or is flipping. Each bubble flips from face_from to face_to.
  const bool reveal_engaged =
      anim_done && (s_cf->reveal_active || s_cf->mode != ClockFaceModeIcons);
  const ClockFaceMode face_from = s_cf->mode;
  const ClockFaceMode face_to = s_cf->reveal_active ? s_cf->mode_to : s_cf->mode;
  int32_t elapsed_ms = 0;
  AnimationProgress reveal_t = ANIMATION_NORMALIZED_MAX;
  if (s_cf->reveal_active) {
    reveal_t = s_cf->reveal_p;
    elapsed_ms = weather_scale_i32(reveal_t, CLOCK_FLIP_MS, ANIMATION_NORMALIZED_MAX);
  }
  // Time since the latest flip into the precipitation mode began
  const int32_t since_fill = (int32_t)MIN(prv_now_ms() - s_cf->fill_started_ms, 100000u);

  // ---- Centre ----
  // Each mode has its own centre content. On a tap the leaving one slides out to the right and
  // the arriving one follows from the left, both masked to the ring's inside.
  const GPoint centre = GPoint(cx, cy);
  const int ring_r = is_today ? CLOCK_GLOW_OUTER_R_TODAY : CLOCK_GLOW_OUTER_R;
  const int ring_inner = ring_r - 3;
  bool shown[ClockFaceModeCount] = {};
  int centre_dx[ClockFaceModeCount] = {};
  int32_t slide = ANIMATION_NORMALIZED_MAX; // the slide's eased progress
  if (s_cf->reveal_active && face_from != face_to) {
    slide = prv_slide_ease(prv_progress_between(elapsed_ms, 0, CLOCK_CENTRE_SLIDE_MS));
    if (slide < ANIMATION_NORMALIZED_MAX) {
      const int spacing = 2 * ring_inner; // the two are one inner diameter apart
      const int out_dx = weather_scale_i32(slide, spacing, ANIMATION_NORMALIZED_MAX);
      shown[face_from] = true;
      centre_dx[face_from] = out_dx;
      centre_dx[face_to] = out_dx - spacing;
    }
  }
  const bool sliding = (slide < ANIMATION_NORMALIZED_MAX);
  shown[face_to] = true;
  // Capture missing centre content before anything else is drawn, as the capture draws it on
  // the blank screen.
  for (int m = 0; m < ClockFaceModeCount; m++) {
    if (shown[m] && !s_cf->centre[m].pixels && prv_centre_valid((ClockFaceMode)m))
      prv_capture_centre(ctx, centre, (ClockFaceMode)m);
  }
  // Clockwise spin: icons start 90° counter-clockwise from target, sweep clockwise to land
  int32_t angle_offset = -(int32_t)(TRIG_MAX_ANGLE / 4) *
                         (int32_t)(ANIMATION_NORMALIZED_MAX - intro_p) /
                         (int32_t)ANIMATION_NORMALIZED_MAX;

  // Current hour — the dial highlights the CURRENT hour's position ("now").
  int cur_h24 = prv_cur_hour();

  // ---- 12 weather icons at hour positions (oval orbit on Emery) ----
  // During exit animation the icons fall downward and shrink away.
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  int icon_xs[12];
  int icon_ys[12];
  GBitmap *icon_bmps[12];

  for (int pos = 1; pos <= 12; pos++) {
    int32_t angle = (int32_t)TRIG_MAX_ANGLE * pos / 12 + angle_offset;
    // 3/9 o'clock icons pull 7px inward so 25px art clears the screen edge
    // (rx 87 + 12.5 = 199.5 on emery's 200px). NOT on small rect: its dial
    // icons are 20px and fit at the true orbit — the leftover nudge made the
    // ring visibly lopsided and starved the pill's lane at 3/9.
    int nudge =
        (CLOCK_SMALL_RECT || PBL_IF_ROUND_ELSE(1, 0)) ? 0 : ((pos == 3 || pos == 9) ? 7 : 0);
    int orbit_rx = ((int)(ICON_ORBIT_RX - nudge) * (int)intro_p / (int)ANIMATION_NORMALIZED_MAX);
    int orbit_ry = ((int)ICON_ORBIT_RY * (int)intro_p / (int)ANIMATION_NORMALIZED_MAX);
    int ix = cx + (int)((int32_t)sin_lookup(angle) * orbit_rx / TRIG_MAX_RATIO);
    int iy = cy - (int)((int32_t)cos_lookup(angle) * orbit_ry / TRIG_MAX_RATIO);

    WeatherType wt = prv_type_for_pos(pos);
    int idx = (wt <= WeatherType_RainAndSnow) ? (int)wt : (int)WeatherType_Generic;

    int full_r = CLOCK_DISC_R;
    int circle_r =
        anim_done ? full_r : 2 + (int)((full_r - 2) * (int)intro_p / (int)ANIMATION_NORMALIZED_MAX);

    icon_xs[pos - 1] = ix;
    icon_ys[pos - 1] = iy;
    icon_bmps[pos - 1] =
        s_cf->type_bitmaps[idx] ? s_cf->type_bitmaps[idx] : s_cf->type_bitmaps[WeatherType_Generic];

    if (reveal_engaged) {
      // Each circle flips from one face to the next: a weather disc, or an outlined bubble with
      // the temperature or the water.
      const bool flipping = (face_from != face_to);
      int rp = flipping ? prv_reveal_rp(reveal_t, pos) : ANIMATION_NORMALIZED_MAX;
      // Pinch the circle as it flips; the content swaps at the midpoint.
      int rr = prv_flip_radius(full_r, rp);
      const bool to_side = (rp >= ANIMATION_NORMALIZED_MAX / 2);
      const ClockFaceMode face = to_side ? face_to : face_from;
      // Hide the content near the midpoint so it stays inside the shrinking circle
      const bool show_content = to_side ? (rp >= ANIMATION_NORMALIZED_MAX * 76 / 100)
                                        : (rp <= ANIMATION_NORMALIZED_MAX * 24 / 100);
      const bool show_icon = (face == ClockFaceModeIcons) && show_content;
      const bool show_temp_text = (face == ClockFaceModeTemps) && show_content;

      if (rr < 1)
        continue; // fully shrunk — nothing to draw

      graphics_context_set_compositing_mode(ctx, GCompOpAssign);
      if (face != ClockFaceModeIcons) {
        graphics_context_set_fill_color(ctx, pal.bubble);
        graphics_fill_circle(ctx, GPoint(ix, iy), rr);
        if (face == ClockFaceModePrecip) {
          // The water rises once this bubble's flip into the mode ends, and shrinks with the
          // bubble when leaving it.
          const int32_t fill_ms = MAX(0, since_fill - prv_fill_start_ms(pos));
          const int chance = prv_precip_for_pos(pos);
          if (chance >= 0) {
            prv_draw_water(ctx, GPoint(ix, iy), rr - 1, chance, fill_ms, &pal);
          } else if (rr >= 6) {
            // No figure for this hour: a dash, so it does not read as 0%.
            graphics_context_set_fill_color(ctx, pal.fg);
            graphics_fill_rect(ctx, GRect(ix - 3, iy, 6, 2), 0, GCornerNone);
          }
        }
        graphics_context_set_stroke_color(ctx, pal.bubble_stroke);
        graphics_context_set_stroke_width(ctx, 1);
        graphics_draw_circle(ctx, GPoint(ix, iy), rr);
      } else {
        const GColor disc = weather_type_disc_color(wt);
        if (!gcolor_equal(disc, GColorClear)) {
          graphics_context_set_fill_color(ctx, disc);
          graphics_fill_circle(ctx, GPoint(ix, iy), rr);
        }
      }

      if (show_icon) {
        GBitmap *bmp = icon_bmps[pos - 1];
        if (bmp) {
          weather_icon_draw(ctx, bmp,
                            GRect(ix - CLOCK_ICON_SIZE / 2, iy - CLOCK_ICON_SIZE / 2,
                                  CLOCK_ICON_SIZE, CLOCK_ICON_SIZE),
                            false /* no dark grounds on this screen */);
        }
      } else if (show_temp_text) {
        int temp;
        if (prv_temp_for_pos(pos, &temp)) {
          // Centre the number in the circle; the degree sign hangs off to the
          // right without shifting the number's centring.
          char nbuf[6];
          snprintf(nbuf, sizeof(nbuf), "%d", temp);
          static GFont tfont; // stable system handle — don't re-look-up per icon per frame
          if (!tfont)
            tfont = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
          const int nw = prv_text_width(ctx, nbuf, tfont);
          prv_draw_text(ctx, nbuf, tfont, GRect(ix - nw / 2, iy - 11, nw, 18), GTextAlignmentCenter,
                        pal.fg);
          prv_draw_text(ctx, "\xC2\xB0", tfont, GRect(ix + nw / 2, iy - 13, 10, 18),
                        GTextAlignmentLeft, pal.fg);
        }
      }
      continue; // reveal path draws both circle and content; skip the bitmap pass
    }

    if (circle_r < 1)
      continue; // fully shrunk — skip background circle

    const GColor disc = weather_type_disc_color(wt);
    if (gcolor_equal(disc, GColorClear))
      continue;
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    graphics_context_set_fill_color(ctx, disc);
    graphics_fill_circle(ctx, GPoint(ix, iy), circle_r);
  }

  // Draw icon bitmaps — doing this in a separate pass minimizes framebuffer captures
  if (!reveal_engaged && (anim_done || p > ANIMATION_NORMALIZED_MAX / 2)) {
    for (int i = 0; i < 12; i++) {
      if (!icon_bmps[i])
        continue;
      int bx = icon_xs[i] - CLOCK_ICON_SIZE / 2;
      int by = icon_ys[i] - CLOCK_ICON_SIZE / 2;
      weather_icon_draw(ctx, icon_bmps[i], GRect(bx, by, CLOCK_ICON_SIZE, CLOCK_ICON_SIZE),
                        false /* no dark grounds on this screen */);
    }
  }

  // The centre content, scaled up during the intro
  const int32_t grow_p = anim_done ? ANIMATION_NORMALIZED_MAX : centre_scale_p;
  for (int m = 0; m < ClockFaceModeCount; m++) {
    if (shown[m] && prv_centre_valid((ClockFaceMode)m)) {
      prv_draw_centre_image(ctx, &s_cf->centre[m], GPoint(cx + centre_dx[m], cy),
                            prv_centre_scale_pm((ClockFaceMode)m, grow_p), centre,
                            sliding ? ring_inner : -1, &pal);
    }
  }

  // The ring, over the content. While the centre slides, the arriving mode's colour sweeps in
  // from the left just ahead of its content.
  if (anim_done) {
    const bool idle = (s_cf->glow_idle_ms >= CLOCK_GLOW_IDLE_TIMEOUT_MS);
    const int32_t extra_span = prv_glow_extra_span();
    const bool sweep =
        sliding && (face_from == ClockFaceModePrecip || face_to == ClockFaceModePrecip ||
                    !gcolor_equal(prv_ring_color(face_from, &pal), prv_ring_color(face_to, &pal)));
    if (sweep) {
      // Across the ring and its percent sign.
      const int x_left = cx - ring_r - 1;
      const int x_right = cx + ring_r + 6;
      const int x = x_left + weather_scale_i32(slide, x_right - x_left, ANIMATION_NORMALIZED_MAX);
      prv_draw_ring_part(ctx, centre, ring_r, face_to, 0, x, idle, extra_span, &pal);
      prv_draw_ring_part(ctx, centre, ring_r, face_from, x, W, idle, extra_span, &pal);
    } else {
      prv_draw_ring_part(ctx, centre, ring_r, face_to, 0, W, idle, extra_span, &pal);
    }
  }

  // ---- Hour number labels — fade in during final third of animation ----
  // Pebble has no alpha; simulate fade: invisible (white) → light gray → dark gray
  // Start appearing at 60% progress, reach full color at 100%.
  // Small rect: labels and pill hold until the ring has LANDED — they are
  // position references, and rendering them against still-sweeping icons
  // reads as a broken layout on any frozen/slow frame (no fade to lose
  // there anyway: gray label ink dithers on 1-bit, so it draws solid black).
  AnimationProgress fade_start = CLOCK_SMALL_RECT
                                     ? ANIMATION_NORMALIZED_MAX
                                     : (AnimationProgress)(ANIMATION_NORMALIZED_MAX * 6 / 10);
  if (!anim_done && p < fade_start)
    return;
  GColor label_color;
  if (anim_done || p > (AnimationProgress)(ANIMATION_NORMALIZED_MAX * 85 / 100)) {
    label_color = GColorDarkGray;
  } else if (p > (AnimationProgress)(ANIMATION_NORMALIZED_MAX * 72 / 100)) {
    label_color = GColorDarkGray; // second step — already dark
  } else {
    label_color = GColorLightGray; // first step — light gray on white
  }
  if (CLOCK_SMALL_RECT) {
    label_color = fg_color; // gray dithers to broken specks on 1-bit
  }
  GFont label_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
#if PBL_DISPLAY_HEIGHT >= 200
#define LABEL_ORBIT_RX (ICON_ORBIT_RX - 42)
#define LABEL_ORBIT_RY (ICON_ORBIT_RY - 42)
#define DOT_ORBIT_RX   (ICON_ORBIT_RX - 27)
#define DOT_ORBIT_RY   (ICON_ORBIT_RY - 27)
#else
// Chalk keeps its ring; the small rects give the labels and dots more air
// so they clear the centre temperature.
#define LABEL_ORBIT_RX (ICON_ORBIT_RX - PBL_IF_ROUND_ELSE(30, 24))
#define LABEL_ORBIT_RY (ICON_ORBIT_RY - PBL_IF_ROUND_ELSE(30, 24))
#define DOT_ORBIT_RX   (ICON_ORBIT_RX - PBL_IF_ROUND_ELSE(18, 13))
#define DOT_ORBIT_RY   (ICON_ORBIT_RY - PBL_IF_ROUND_ELSE(18, 13))
#endif
#if CLOCK_SMALL_RECT
  // Where the pill sits (0 = not on this day's dial) — anchors ring-adjacent
  // to it are suppressed below.
  int now_pos = 0;
  if (s_cf->day_index == 0) {
    for (int pos = 1; pos <= 12; pos++) {
      if (prv_abs_hour_for_pos(pos) == cur_h24) {
        now_pos = pos;
        break;
      }
    }
  }
#endif
  for (int pos = 1; pos <= 12; pos++) {
    int32_t angle = (int32_t)TRIG_MAX_ANGLE * pos / 12;
    // The CURRENT hour gets a bubble instead of a label; only today's clock
    // highlights "now" — for other days "now" isn't on that day.
    int abs_h = prv_abs_hour_for_pos(pos);
    bool is_now = (s_cf->day_index == 0) && (abs_h == cur_h24);
    // Label nudge: 3 and 9 o'clock pulled 5px inward (their text sits widest
    // horizontally). NOT on small rect: anchors and pill share one orbit
    // there, and nudging only some of them reads as misalignment (the 9
    // anchor closer to the centre than the 3-side pill).
    int label_nudge = PBL_IF_ROUND_ELSE(0, (pos == 3 || pos == 9) ? 5 : 0);
    if (CLOCK_SMALL_RECT)
      label_nudge = 0;
    int lx =
        cx + (int)((int32_t)sin_lookup(angle) * (LABEL_ORBIT_RX - label_nudge) / TRIG_MAX_RATIO);
    int ly = cy - (int)((int32_t)cos_lookup(angle) * LABEL_ORBIT_RY / TRIG_MAX_RATIO);
    // Dot between number and icon — 3 and 9 o'clock nudged 6px inward
    int dot_nudge = PBL_IF_ROUND_ELSE(0, (pos == 3 || pos == 9) ? 6 : 0);
    int ddx = cx + (int)((int32_t)sin_lookup(angle) * (DOT_ORBIT_RX - dot_nudge) / TRIG_MAX_RATIO);
    int ddy = cy - (int)((int32_t)cos_lookup(angle) * (DOT_ORBIT_RY - dot_nudge) / TRIG_MAX_RATIO);
#if CLOCK_SMALL_RECT
    // 144px: a full 12-label inner ring cannot fit — two-digit labels collide
    // near the vertical. The dial is a true clock layout (hour N sits at its
    // clock position), so only the cardinal anchors are labelled; the pill
    // names the current hour and clock convention gives the rest. No tick
    // dots either — the icons already mark the positions.
    (void)ddx;
    (void)ddy;
    if (!is_now && (pos % 3) != 0)
      continue;
    // An anchor ring-adjacent to the pill shares its cramped corner (e.g. the
    // 17h pill at 5 o'clock against the 18 anchor at 6) — the pill wins.
    if (!is_now && now_pos > 0) {
      const int ring_d = (pos - now_pos + 12) % 12;
      if (ring_d == 1 || ring_d == 11)
        continue;
    }
#else
    if (!is_now) {
      graphics_context_set_fill_color(ctx, label_color);
      graphics_fill_circle(ctx, GPoint(ddx, ddy), 2);
    }
#endif
    int disp_h = abs_h % 24; // abs_h >= 24 = tomorrow's hour — label wraps
    if (!clock_is_24h_style()) {
      disp_h %= 12;
      if (disp_h == 0)
        disp_h = 12; // 12h dials write 12, never 0
    }
    char lbuf[4];
    snprintf(lbuf, sizeof(lbuf), "%d", disp_h);
    int lw = (disp_h >= 10) ? 28 : 20;
    int loff = (disp_h >= 10) ? -14 : -10;
    GRect label_rect = GRect(lx + loff, ly - 12, lw, 18);
    GTextAlignment label_align = GTextAlignmentCenter;
    int pill_len = lw + 12;
    int pill_center_x = lx;
    int pill_center_y = ly + 1;

    // The blue indicator fades in (scales up from a small dot) a few frames before
    // it settles, then stretches into the oval once the bitmaps have landed.
    int32_t pill_appear = (ANIMATION_NORMALIZED_MAX * 70) / 100;  // dot starts growing
    int32_t pill_settled = (ANIMATION_NORMALIZED_MAX * 78) / 100; // full circle, begins stretch
    bool show_pill = is_now && (anim_done || p > pill_appear);

    if (show_pill) {
      // Small rect: the 24px label lane cannot hold the stretched two-digit
      // oval (it nips either the icon or the centre temperature). The pill is
      // a compact r=9 CIRCLE there, with the number a size down (see below) —
      // at r=9 the rim clears the icon art, the centre temperature and both
      // by >=2px at every one of the twelve positions (r=10 grazes at the
      // horizontals and the temp-corner diagonals).
      int r = 9;
      int D = (pill_len - (r * 2)) / 2;

      int D_out = D - 5; // Pull the pill inwards 5px from the outside (the number-facing end)
      if (CLOCK_SMALL_RECT || D_out < 0)
        D_out = 0;
      int D_in =
          D -
          8; // Pull the pill inwards from the center of the watch face (less padding on the inside)
      if (CLOCK_SMALL_RECT || D_in < 0)
        D_in = 0;

      if (!anim_done) {
        if (p < pill_settled) {
          // Grow the dot from a small radius up to full size while it fades in.
          int32_t grow_p =
              (p - pill_appear) * ANIMATION_NORMALIZED_MAX / (pill_settled - pill_appear);
          if (grow_p < 0)
            grow_p = 0;
          r = 2 + (int)((int32_t)(r - 2) * grow_p / ANIMATION_NORMALIZED_MAX);
          D_out = 0;
          D_in = 0;
        } else {
          // Stretch the full circle out into the oval.
          int32_t stretched_p = (p - pill_settled) * ANIMATION_NORMALIZED_MAX /
                                (ANIMATION_NORMALIZED_MAX - pill_settled);
          D_out = (int)((int32_t)D_out * stretched_p / ANIMATION_NORMALIZED_MAX);
          D_in = (int)((int32_t)D_in * stretched_p / ANIMATION_NORMALIZED_MAX);
        }
      }

      int bcx = pill_center_x;
      int bcy = pill_center_y;

      int c1x = bcx + (int)((int32_t)sin_lookup(angle) * D_out / TRIG_MAX_RATIO);
      int c1y = bcy - (int)((int32_t)cos_lookup(angle) * D_out / TRIG_MAX_RATIO);
      int c2x = bcx - (int)((int32_t)sin_lookup(angle) * D_in / TRIG_MAX_RATIO);
      int c2y = bcy + (int)((int32_t)cos_lookup(angle) * D_in / TRIG_MAX_RATIO);

      const GColor pill_color = PBL_IF_COLOR_ELSE(GColorVividCerulean, fg_color);
      graphics_context_set_fill_color(ctx, pill_color);
      graphics_context_set_stroke_color(ctx, pill_color);
      graphics_context_set_stroke_width(ctx, r * 2 + 1);

      graphics_fill_circle(ctx, GPoint(c1x, c1y), r);
      graphics_fill_circle(ctx, GPoint(c2x, c2y), r);
      graphics_draw_line(ctx, GPoint(c1x, c1y), GPoint(c2x, c2y));

      graphics_context_set_stroke_width(ctx, 1);
    }
    // The pill's number steps a size down on small rect (a G18B two-digit
    // pokes past the r=9 circle's chord). LOCAL override only — assigning
    // label_font here would leak the small font into every anchor drawn
    // after the pill's loop position.
    GFont draw_font = label_font;
    GRect draw_rect = label_rect;
    if (CLOCK_SMALL_RECT && show_pill) {
      draw_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
      draw_rect = GRect(lx - 14, ly - 7, 28, 14);
    }
    prv_draw_text(ctx, lbuf, draw_font, draw_rect, label_align, show_pill ? pal.bg : label_color);
  }

  // UP-to-forecast: wrap the fully-drawn clock in the whole-screen squash-stretch.
  if (s_cf->fwd_exit_active)
    prv_render_fwd_squash(ctx);
}

// ---- Tick handler ----
static void prv_tick_handler(struct pbl_tm *tick_time, TimeUnits units_changed) {
  if (!s_cf)
    return;
  prv_refresh_centre();
  if (units_changed & HOUR_UNIT)
    prv_free_centre_image(&s_cf->centre[ClockFaceModeTemps]); // today's figure is the hour's
  if (s_cf->canvas)
    layer_mark_dirty(s_cf->canvas);
}

// ---- Touch input (touch colour platforms) ----
#ifdef CONFIG_TOUCH
static void prv_touch_handler(const TouchEvent *event, void *context) {
  (void)context;
  if (!s_cf)
    return;

  if (event->type == TouchEvent_Touchdown) {
    s_cf->touch_start_x = event->x;
    s_cf->touch_start_y = event->y;
    s_cf->touch_active = true;
    s_cf->touch_started_during_intro = s_cf->anim_progress < ANIMATION_NORMALIZED_MAX;
  } else if (event->type == TouchEvent_Liftoff && s_cf->touch_active) {
    s_cf->touch_active = false;
    if (s_cf->touch_started_during_intro) {
      s_cf->touch_started_during_intro = false;
      prv_note_clock_interaction();
      return;
    }

    int16_t dx = event->x - s_cf->touch_start_x;
    int16_t dy = event->y - s_cf->touch_start_y;
    int16_t adx = dx < 0 ? -dx : dx;
    int16_t ady = dy < 0 ? -dy : dy;
    if (adx <= SWIPE_THRESHOLD && ady <= SWIPE_THRESHOLD) {
      // Tap: advance the ring to its next mode, as SELECT does
      prv_advance_mode();
    } else if (ady >= adx && dy > 0) {
      // Swipe down — return to the forecast main screen (mirrors the UP button).
      if (!s_cf->fwd_exit_active && !s_cf->reveal_active &&
          s_cf->anim_progress >= ANIMATION_NORMALIZED_MAX) {
        prv_start_fwd_exit_animation();
      }
    }
    prv_note_clock_interaction();
  }
}
#endif

// ---- Button input ----
static void prv_click_back(ClickRecognizerRef r, void *ctx) {
  prv_note_clock_interaction();
  app_window_stack_pop_all(true); // BACK exits the app from any ring page
}
static void prv_click_select(ClickRecognizerRef r, void *ctx) {
  // Advance the ring to its next mode, as a tap does
  prv_note_clock_interaction();
  prv_advance_mode();
}
static void prv_click_down(ClickRecognizerRef r, void *ctx) {
  // The clock is the bottom of the vertical stack now that the detail drill-in is gone, so DOWN
  // has nothing below it — just keep the clock awake.
  prv_note_clock_interaction();
}
static void prv_click_up(ClickRecognizerRef r, void *ctx) {
  prv_note_clock_interaction();
  // UP returns to the forecast main screen: the clock jelly-stretches down off the bottom, then
  // the forecast jelly-drops in from above (handoff fires from prv_fwd_exit_push_callback). Ignore
  // while the intro or any other transition is still in flight.
  if (s_cf && !s_cf->fwd_exit_active && !s_cf->reveal_active &&
      s_cf->anim_progress >= ANIMATION_NORMALIZED_MAX) {
    prv_start_fwd_exit_animation();
  }
}
static void prv_click_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_BACK, prv_click_back);
  window_single_click_subscribe(BUTTON_ID_UP, prv_click_up);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_click_select);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_click_down);
}

// ---- Window lifecycle ----
static void prv_window_load(Window *window) {
  // ROUND (gabbro smoothness step 3): transparent — the root-layer proc fills the whole
  // window with the bg colour every frame unless transparent, and prv_canvas_draw always
  // paints every pixel (its own white fill / the squash resample). See forecast_list's
  // matching change. The !s_cf early-return keeps a white guard.
#if PBL_ROUND
  window_set_background_color(window, GColorClear);
#else
  window_set_background_color_follows_theme(window, false /* dimmed */);
#endif
  GRect bounds = layer_get_bounds(window_get_root_layer(window));
  s_cf->canvas = layer_create(bounds);
  layer_set_update_proc(s_cf->canvas, prv_canvas_draw);
  layer_add_child(window_get_root_layer(window), s_cf->canvas);

#if PBL_DISPLAY_HEIGHT >= 200
  s_cf->temp_font = fonts_get_system_font(FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM);
#else
  s_cf->temp_font = fonts_get_system_font(FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM);
#endif

  prv_load_bitmaps();
  s_cf->centre_type = prv_centre_weather_type();
  tick_timer_service_subscribe(HOUR_UNIT, prv_tick_handler);

  if (s_cf->skip_intro_animation) {
    s_cf->anim_progress = ANIMATION_NORMALIZED_MAX;
    if (s_cf->canvas)
      layer_mark_dirty(s_cf->canvas);
  } else {
    // Schedule intro animation: bloom + clockwise spin into position (no HUD bar)
    prv_play_animation();
  }
}

static void prv_window_appear(Window *window) {
  if (!s_cf)
    return;
  // Reset the mode so the clock shows weather icons on return.
  s_cf->reveal_active = false;
  s_cf->mode = ClockFaceModeIcons;
  s_cf->reveal_p = 0;
  s_cf->tap_queued = false;
  if (s_cf->tap_timer) {
    app_timer_cancel(s_cf->tap_timer);
    s_cf->tap_timer = nullptr;
  }
  if (s_cf->reveal_anim) {
    animation_unschedule(s_cf->reveal_anim);
    animation_destroy(s_cf->reveal_anim);
    s_cf->reveal_anim = nullptr;
  }
  if (s_cf->canvas)
    layer_mark_dirty(s_cf->canvas);
#ifdef CONFIG_TOUCH
  touch_service_subscribe(prv_touch_handler, s_cf);
#endif
  prv_note_clock_interaction();
}

static void prv_window_unload(Window *window) {
  if (s_cf->glow_timer) {
    app_timer_cancel(s_cf->glow_timer);
    s_cf->glow_timer = nullptr;
  }
#if PBL_ROUND
  if (s_cf->glow_drv) {
    Animation *a = s_cf->glow_drv;
    s_cf->glow_drv = nullptr;
    animation_unschedule(a);
    animation_destroy(a);
  }
#endif
  if (s_cf->intro_anim) {
    animation_unschedule(s_cf->intro_anim);
    animation_destroy(s_cf->intro_anim);
    s_cf->intro_anim = nullptr;
  }
  if (s_cf->reveal_anim) {
    animation_unschedule(s_cf->reveal_anim);
    animation_destroy(s_cf->reveal_anim);
    s_cf->reveal_anim = nullptr;
  }
  if (s_cf->fwd_exit_anim) {
    animation_unschedule(s_cf->fwd_exit_anim);
    animation_destroy(s_cf->fwd_exit_anim);
    s_cf->fwd_exit_anim = nullptr;
  }
  if (s_cf->fwd_push_timer) {
    app_timer_cancel(s_cf->fwd_push_timer);
    s_cf->fwd_push_timer = nullptr;
  }
  if (s_cf->fwd_scratch) {
    free(s_cf->fwd_scratch);
    s_cf->fwd_scratch = nullptr;
  }
  if (s_cf->fill_anim) {
    animation_unschedule(s_cf->fill_anim);
    animation_destroy(s_cf->fill_anim);
    s_cf->fill_anim = nullptr;
  }
  if (s_cf->tap_timer) {
    app_timer_cancel(s_cf->tap_timer);
    s_cf->tap_timer = nullptr;
  }
  prv_free_centre_images();
  prv_free_stamps();
  tick_timer_service_unsubscribe();

#ifdef CONFIG_TOUCH
  touch_service_unsubscribe();
#endif

  for (int i = 0; i < NUM_TYPE_SLOTS; i++) {
    if (s_cf->type_bitmaps[i]) {
      gbitmap_destroy(s_cf->type_bitmaps[i]);
      s_cf->type_bitmaps[i] = nullptr;
    }
  }
  layer_destroy(s_cf->canvas);
  s_cf->canvas = nullptr;
  window_destroy(window);
  free(s_cf);
  s_cf = nullptr;
}

// ---- Public API ----

void clock_face_handle_dark_mode_change(void) {
  if (!s_cf || !s_cf->canvas) {
    return;
  }
  // The dial icons are recolored for the theme on load. The centre captures and text stamps
  // hold the old colours, so they are made again.
  prv_load_bitmaps();
  prv_free_centre_images();
  prv_free_stamps();
  layer_mark_dirty(s_cf->canvas);
}

void clock_face_set_wrap_callback(ClockFaceWrapCallback callback, void *context) {
  s_wrap_callback = callback;
  s_wrap_context = context;
}

bool clock_face_is_showing(void) {
  return s_cf != nullptr;
}

static void prv_clock_face_push(const WeatherLocationForecast *days, size_t num_days, int day_index,
                                const uint8_t *hourly_types, size_t hourly_count, bool animated,
                                bool play_intro) {
  if (s_cf)
    return;

  s_cf = app_zalloc(sizeof(ClockFaceData)); // may fail: the clock is then not shown
  if (!s_cf)
    return;

  size_t n = num_days < MAX_DAYS ? num_days : MAX_DAYS;
  s_cf->num_days = n;
  for (size_t i = 0; i < n; i++)
    s_cf->days[i] = days[i];
  s_cf->day_index = (day_index >= 0 && (size_t)day_index < n) ? day_index : 0;
  s_cf->skip_intro_animation = !play_intro;

  // Seed hourly data so it's available immediately on first draw.
  if (hourly_count > 0) {
    size_t hn = hourly_count < 24 ? hourly_count : 24;
    for (size_t i = 0; i < hn; i++)
      s_cf->hourly_types[i] = hourly_types[i];
    s_cf->hourly_valid = (hn >= 24);
  }

  s_cf->window = window_create();
  window_set_background_color(s_cf->window, GColorBlack);
  window_set_window_handlers(s_cf->window, (WindowHandlers){
                                             .load = prv_window_load,
                                             .unload = prv_window_unload,
                                             .appear = prv_window_appear,
                                           });
  window_set_click_config_provider(s_cf->window, prv_click_provider);
  window_stack_push(s_cf->window, animated);
}

void clock_face_push(const WeatherLocationForecast *days, size_t num_days, int day_index,
                     const uint8_t *hourly_types, size_t hourly_count, bool animated) {
  prv_clock_face_push(days, num_days, day_index, hourly_types, hourly_count, animated, true);
}

void clock_face_push_static(const WeatherLocationForecast *days, size_t num_days, int day_index,
                            const uint8_t *hourly_types, size_t hourly_count, bool animated) {
  prv_clock_face_push(days, num_days, day_index, hourly_types, hourly_count, animated, false);
}

void clock_face_dismiss(bool animated) {
  if (!s_cf || !s_cf->window)
    return;
  window_stack_remove(s_cf->window, animated);
}

void clock_face_update_hourly_temps_for_day(int day_index, const int8_t *temps, size_t count) {
  if (!s_cf || day_index != s_cf->day_index)
    return;
  size_t n = count < 24 ? count : 24;
  for (size_t i = 0; i < n; i++)
    s_cf->hourly_temps[i] = temps[i];
  s_cf->hourly_temps_valid = (n >= 24);
  if (s_cf->canvas)
    layer_mark_dirty(s_cf->canvas);
}

void clock_face_update_hourly_precip_for_day(int day_index, const uint8_t *precip, size_t count) {
  if (!s_cf || day_index != s_cf->day_index)
    return;
  // A series with no known hour leaves the mode unavailable
  s_cf->hourly_precip_hours = weather_store_hourly_precip(s_cf->hourly_precip, precip, count);
  // A ring showing the mode, or flipping to it, goes to the icons
  if (!prv_mode_available(s_cf->mode))
    s_cf->mode = ClockFaceModeIcons;
  if (s_cf->reveal_active && !prv_mode_available(s_cf->mode_to))
    s_cf->mode_to = ClockFaceModeIcons;
  if (s_cf->canvas)
    layer_mark_dirty(s_cf->canvas);
}

void clock_face_set_tomorrow_hourly(const uint8_t *types, const int8_t *temps, size_t count) {
  if (!s_cf || !types || !temps || count < 24)
    return;
  for (size_t i = 0; i < 24; i++) {
    s_cf->tomorrow_types[i] = types[i];
    s_cf->tomorrow_temps[i] = temps[i];
  }
  s_cf->tomorrow_hourly_valid = true;
  if (s_cf->canvas)
    layer_mark_dirty(s_cf->canvas);
}
