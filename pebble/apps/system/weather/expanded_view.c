/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "expanded_view.h"

#include "pebble_compat.h"
#include "weather_app_layout.h"
#include "weather_math.h"
#include "weather_sun.h"
#include "weather_types.h"

#include <stdint.h>
#include <string.h>
#include <time.h>

#include <pbl/i18n/i18n.h>
#include <pbl/kernel/time.h>
#include <pbl/util/math.h>

#include <applib/app_timer.h>
#include <applib/graphics/gdraw_command_image.h>
#include <applib/graphics/gdraw_command_transforms.h>
#include <applib/ui/animation.h>
#include <applib/ui/animation_interpolate.h>
#include <applib/ui/content_indicator.h> // DOWN arrow on round
#include <applib/ui/property_animation.h>
#include <shell/prefs.h>

// The sunset card, reached by UP from the forecast: three outlined cards (see prv_draw_cards)
// under the status time.
//
// SELECT slides the card out and opens the globe, and DOWN or BACK returns to the forecast. Taps
// and swipes map onto the same actions.
typedef struct {
  Window *window;
  Layer *canvas;
  GDrawCommandImage *icon;   // the weather icon, scaled to EV_ICON_SIZE (owned)
  ExpandedViewGlance glance; // the body's strings and readings
  int16_t lat_e2;            // degrees*100, INT16_MIN unknown (for sunset)
  int16_t lon_e2;
  int16_t utc_off_min; // location tz, minutes east of UTC; INT16_MIN = watch tz
  char time_str[10];   // status bar time
  // Content entrance: the card content slides in from the left + bounces (Timeline card entrance).
  AnimationProgress text_p;
  Animation *text_anim;
  bool text_active;
  ExpandedViewEntrance entrance; // how the card animates in on first appear (latched off after)
  // Status bar: show "Last updated ..." for 2s, then swap it out to the right while the time swoops
  // in from the left.
  char updated_str[40]; // "Last updated 8:23 PM"
  bool show_updated;    // true until the swap completes
  bool swap_active;     // the last-updated -> time slide is playing
  AnimationProgress swap_p;
  Animation *swap_anim;
  AppTimer *updated_timer; // fires 3s after appear to start the swap
  // Whole-card horizontal slide (moook bounce), shared by the SELECT slide-out-left (then on_select
  // fires -> globe slides in from right) and the globe-BACK slide-in-from-left entrance.
  Animation *slide_anim;
#ifdef CONFIG_TOUCH
  int16_t touch_start_x, touch_start_y; // Touchdown origin for swipe detection
  bool touch_active;
#endif
  void (*on_down)(void *ctx);
  void *on_down_ctx; // DOWN/BACK -> forecast
  void (*on_select)(void *ctx);
  void *on_select_ctx; // SELECT -> globe (slide-in from right)
} ExpandedViewData;

static ExpandedViewData *s_ev;

// Colour screens paint the body from masks drawn once (see prv_mask_capture). Black and white
// screens draw it live: they have no antialiasing to keep and little heap to spare.
#define EV_BODY_MASK PBL_COLOR

#if EV_BODY_MASK
// A page's body mask (see prv_mask_capture), EV_BODY_STRIDE(W) * EV_BODY_H bytes or NULL, with
// the glance it was drawn from.
typedef struct {
  uint8_t *bits;
  int w;
  ExpandedViewGlance glance;
} BodyMask;
// One mask per page, allocated by the first capture and freed by expanded_view_drop_body. They
// are not part of s_ev: the forecast's hero fly captures the body before the card exists.
#define EV_MASKS 1
static BodyMask *s_masks;
#endif

// The sunset box's icon, loaded by the first body draw and freed by expanded_view_drop_body. Like
// the masks it is not part of s_ev, as the hero fly draws the body before the card exists.
static GDrawCommandImage *s_sunbox_icon;
static bool s_sunbox_icon_dark; // the theme it was recoloured for

// System-app statics persist across launches (the fw image is not reloaded), and a
// crashed run never reaches unload — clear them before the next run reads them.
void expanded_view_reset(void) {
  s_ev = nullptr;
  s_sunbox_icon = nullptr; // freed with the previous run's heap
#if EV_BODY_MASK
  s_masks = nullptr; // freed with the previous run's heap
#endif
}

static void prv_fmt_hhmm(char *out, size_t n, int h24, int mm) {
  if (clock_is_24h_style()) {
    snprintf(out, n, "%d:%02d", h24, mm);
  } else {
    int h12 = h24 % 12;
    if (h12 == 0)
      h12 = 12;
    char ampm[8];
    pbl_i18n_get_with_buffer(h24 < 12 ? pbl_i18n_noop("AM") : pbl_i18n_noop("PM"), ampm,
                             sizeof(ampm));
    snprintf(out, n, "%d:%02d %s", h12, mm, ampm);
  }
}

// "<label> 9:30 PM", with the label's translation deciding where the time goes.
static void prv_fmt_labelled_time(char *out, size_t n, const char *fmt, int h24, int mm) {
  char hhmm[16];
  char localized_fmt[40];
  prv_fmt_hhmm(hhmm, sizeof(hhmm), h24, mm);
  pbl_i18n_get_with_buffer(fmt, localized_fmt, sizeof(localized_fmt));
  snprintf(out, n, localized_fmt, hhmm);
}

#if PBL_ROUND
static void prv_build_sunset(char *out, size_t n, int16_t lat_e2, int16_t lon_e2,
                             int16_t utc_off_min) {
  int m = weather_sun_sunset_minutes(lat_e2, lon_e2, utc_off_min);
  if (m < 0) {
    pbl_i18n_get_with_buffer(pbl_i18n_noop("Sunset at --:--"), out, n);
    return;
  }
  prv_fmt_labelled_time(out, n, pbl_i18n_noop("Sunset at %s"), (m / 60) % 24, m % 60);
}

static void prv_build_golden(char *out, size_t n, int16_t lat_e2, int16_t lon_e2,
                             int16_t utc_off_min) {
  const int m = weather_sun_golden_minutes(lat_e2, lon_e2, utc_off_min);
  if (m < 0) {
    /// When the evening golden hour starts, unknown
    pbl_i18n_get_with_buffer(pbl_i18n_noop("Golden hour: --:--"), out, n);
    return;
  }
  /// When the evening golden hour starts ("Golden hour: 5:41 PM")
  prv_fmt_labelled_time(out, n, pbl_i18n_noop("Golden hour: %s"), (m / 60) % 24, m % 60);
}
#else
// "9:30 PM" for minute m of the day, "--:--" for a negative one
static void prv_fmt_minutes(char *out, size_t n, int m) {
  if (m < 0) {
    snprintf(out, n, "--:--");
    return;
  }
  prv_fmt_hhmm(out, n, (m / 60) % 24, m % 60);
}
#endif

// Feels-like is already in the user's unit, like every temperature in the record
static void prv_build_feels(char *out, size_t n, const WeatherLocationForecast *f) {
  if (!f || f->today_feels == WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP) {
    /// The feels-like temperature when it is unknown
    pbl_i18n_get_with_buffer(pbl_i18n_noop("Feels like --"), out, n);
    return;
  }
  char localized_fmt[40];
  /// The feels-like temperature, a label of its own: no full stop
  pbl_i18n_get_with_buffer(pbl_i18n_noop("Feels like %d\xC2\xB0"), localized_fmt,
                           sizeof(localized_fmt));
  snprintf(out, n, localized_fmt, f->today_feels);
}

static void prv_build_time(char *out, size_t n) {
  time_t now = pbl_realtime();
  struct pbl_tm *lt = localtime(&now);
  if (!lt) {
    snprintf(out, n, "--:--");
    return;
  }
  prv_fmt_hhmm(out, n, lt->tm_hour, lt->tm_min);
}

// Round's status bar is a chord, not a full-width strip, so the rect wording overruns it
// and clips. "Updated, 8:23 PM" says the same thing inside the space there actually is.
#define EV_STATUS_Y    PBL_IF_ROUND_ELSE(4, 0) // round: clear of the curved top edge
#define EV_STATUS_H    (EV_SMALL_RECT ? 16 : 20)
#define EV_STATUS_FONT (EV_SMALL_RECT ? FONT_KEY_GOTHIC_14_BOLD : FONT_KEY_GOTHIC_18_BOLD)

void expanded_view_format_updated(const WeatherLocationForecast *f, char *out, size_t n) {
  struct pbl_tm *lt = nullptr;
  if (f && f->time_updated_utc > 0) {
    time_t t = f->time_updated_utc;
    lt = localtime(&t);
  }
  if (!lt) {
    pbl_i18n_get_with_buffer(pbl_i18n_noop("Last updated --:--"), out, n);
    return;
  }
  prv_fmt_labelled_time(out, n, pbl_i18n_noop("Last updated %s"), lt->tm_hour, lt->tm_min);
}

void expanded_view_format_glance(const WeatherLocationForecast *f, int16_t lat_e2, int16_t lon_e2,
                                 int16_t utc_off_min, ExpandedViewGlance *g, const void *owner) {
  memset(g, 0, sizeof(*g));
#if PBL_ROUND
  prv_build_sunset(g->sunset, sizeof(g->sunset), lat_e2, lon_e2, utc_off_min);
  prv_build_golden(g->golden, sizeof(g->golden), lat_e2, lon_e2, utc_off_min);
#else
  /// Label of the sunset's time, in a narrow row with the time: keep it short
  pbl_i18n_ctx_get_with_buffer("SunsetCard", "Sunset", g->sunset_label, sizeof(g->sunset_label));
  prv_fmt_minutes(g->sunset_time, sizeof(g->sunset_time),
                  weather_sun_sunset_minutes(lat_e2, lon_e2, utc_off_min));
  /// Label of the time the evening golden hour starts
  pbl_i18n_get_with_buffer(pbl_i18n_noop("Golden hour"), g->golden_label, sizeof(g->golden_label));
  /// "Golden hour" abbreviated, for where the full label does not fit
  pbl_i18n_get_with_buffer(pbl_i18n_noop("Golden hr"), g->golden_short, sizeof(g->golden_short));
  prv_fmt_minutes(g->golden_time, sizeof(g->golden_time),
                  weather_sun_golden_minutes(lat_e2, lon_e2, utc_off_min));
#endif
  prv_build_feels(g->feels, sizeof(g->feels), f);
  // The phone sends temperatures in the user's unit already
  snprintf(g->unit, sizeof(g->unit), "%s",
           shell_prefs_get_units_temperature() == UnitsTemperature_Fahrenheit ? "F" : "C");
  if (!f) {
    snprintf(g->temp, sizeof(g->temp), "--");
    snprintf(g->now, sizeof(g->now), "--");
    snprintf(g->wind, sizeof(g->wind), "--");
    snprintf(g->humidity, sizeof(g->humidity), "--");
    weather_app_layout_format_uv(-1, owner, g->uv_text, sizeof(g->uv_text), g->uv_value,
                                 sizeof(g->uv_value));
    return;
  }
  const bool hi = f->today_high != WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP;
  const bool lo = f->today_low != WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP;
  if (hi && lo)
    snprintf(g->temp, sizeof(g->temp), "%d/%d\xC2\xB0", f->today_high, f->today_low);
  else if (hi)
    snprintf(g->temp, sizeof(g->temp), "%d\xC2\xB0", f->today_high);
  else
    snprintf(g->temp, sizeof(g->temp), "--");
  // The current hour's temperature, as the mainscreen header shows it
  if (f->current_temp_now != WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP)
    snprintf(g->now, sizeof(g->now), "%d", f->current_temp_now);
  else
    snprintf(g->now, sizeof(g->now), "--");
  weather_app_layout_format_wind(f->today_wind_mph, owner, g->wind, sizeof(g->wind));
  if (f->today_humidity >= 0)
    snprintf(g->humidity, sizeof(g->humidity), "%d%%", f->today_humidity);
  else
    snprintf(g->humidity, sizeof(g->humidity), "--");
  // The current hour's UV once the phone sends hourly UV, else the day's figure
  weather_app_layout_format_uv(f->today_uv_now, owner, g->uv_text, sizeof(g->uv_text), g->uv_value,
                               sizeof(g->uv_value));
}

static void prv_set_from_forecast(const WeatherLocationForecast *f, int16_t lat_e2, int16_t lon_e2,
                                  int16_t utc_off_min) {
  if (!s_ev)
    return;
  s_ev->lat_e2 = lat_e2;
  s_ev->lon_e2 = lon_e2;
  s_ev->utc_off_min = utc_off_min;
  prv_build_time(s_ev->time_str, sizeof(s_ev->time_str));
  expanded_view_format_updated(f, s_ev->updated_str, sizeof(s_ev->updated_str));
  expanded_view_format_glance(f, lat_e2, lon_e2, utc_off_min, &s_ev->glance, s_ev);

  if (s_ev->icon) {
    gdraw_command_image_destroy(s_ev->icon);
    s_ev->icon = nullptr;
  }
  if (!f) {
    return;
  }
  // BOTH SHAPES: the card's icon is typed by the CURRENT HOUR
  // (current_type_now) — the same field the mainscreen header and the hero fly use — so the
  // flown icon and the card icon can never disagree when the phone's synced 'current' and
  // its hourly slot diverge (seen on the round watch as a rain->overcast repaint at the
  // landing instant; rect's header reads the same field now, so it needs the same seam fix).
  GDrawCommandImage *raw = gdraw_command_image_create_with_resource(
      weather_type_icon_large_resource(f->current_type_now));
  if (raw) {
    s_ev->icon = gdraw_command_image_clone(raw); // writable copy so we can scale it
    gdraw_command_image_destroy(raw);
    if (s_ev->icon) {
      gdraw_command_image_scale(s_ev->icon, GSize(EV_ICON_SIZE, EV_ICON_SIZE));
      weather_recolor_pdc_image_for_dark_mode(s_ev->icon);
    }
  }
}

// ---- Draw -----------------------------------------------------------------

// ---- The cards layout ---------------------------------------------------------------------
// Three outlined cards under the status time: the conditions, the readings and the sunset.
// On round their sides are arcs concentric with the display, and each element is inset from the
// arc at its own rows.
#define EV_CARD_STROKE PBL_IF_COLOR_ELSE(2, 1)
#if PBL_ROUND
#define EV_GLASS_R (PBL_DISPLAY_WIDTH / 2)  // the display circle is centred on the screen
#define EV_CARD_R  (EV_GLASS_R - EV_CARD_X) // the cards' outer arc
#define EV_CARD1_Y 28
#define EV_CARD1_H 104
#define EV_CARD2_Y 137
#define EV_CARD2_H 36
#define EV_CARD3_Y 178
#define EV_CARD3_H 64                // leaves the forecast's 18-row arrow band under it
#define EV_NOW_Y   (EV_CARD1_Y + 12) // the big temperature's text box top
#else
#define EV_CARD_W(W)   ((W) - 2 * EV_CARD_X)
#define EV_CARD_CORNER (EV_SMALL_RECT ? 6 : 8)
#define EV_CARD1_Y     (EV_SMALL_RECT ? 16 : 22)
#define EV_CARD1_H     (EV_SMALL_RECT ? 64 : 96)
#define EV_CARD2_Y     (EV_SMALL_RECT ? 84 : 122)
#define EV_CARD2_H     (EV_SMALL_RECT ? 26 : 36)
#define EV_CARD3_Y     (EV_SMALL_RECT ? 114 : 162)
#define EV_CARD3_H     (EV_SMALL_RECT ? 52 : 64)
#define EV_NOW_Y       (EV_CARD1_Y + (EV_SMALL_RECT ? 2 : 4)) // the big temperature's box top
#endif
// The temperature switches to EV_NOW_FONT_NARROW when it and its unit would reach the icon
// column. EV_NOW_NARROW_DY keeps the smaller digits on the same baseline.
#define EV_NOW_FONT \
  (EV_SMALL_RECT ? FONT_KEY_LECO_38_BOLD_NUMBERS : FONT_KEY_LECO_60_BOLD_NUMBERS_AM_PM)
#define EV_NOW_FONT_NARROW \
  (EV_SMALL_RECT ? FONT_KEY_LECO_32_BOLD_NUMBERS : FONT_KEY_LECO_42_NUMBERS)
#define EV_NOW_NARROW_DY (EV_SMALL_RECT ? 4 : 16)
#define EV_UNIT_FONT     (EV_SMALL_RECT ? FONT_KEY_GOTHIC_14_BOLD : FONT_KEY_GOTHIC_18_BOLD)
// The smallest system text style font: Gothic 18 at the large content size, Gothic 14 at the
// small rect's medium size
#define EV_TEXT_FONT  (EV_SMALL_RECT ? FONT_KEY_GOTHIC_14 : FONT_KEY_GOTHIC_18)
#define EV_NOW_INK_DY (EV_SMALL_RECT ? 6 : 10)  // the digits' visible top, below their box top
#define EV_UNIT_DY    (EV_SMALL_RECT ? 21 : 34) // the unit letter's box, below the digits'
#define EV_HILO_Y     (EV_CARD1_Y + EV_CARD1_H - (EV_SMALL_RECT ? 18 : 26)) // card 1's bottom row
#define EV_HILO_W     (EV_SMALL_RECT ? 44 : 80)
#define EV_FOOT_GAP   4
#define EV_STATS_CY   (EV_CARD2_Y + EV_CARD2_H / 2) // the readings' glyph centre row
// Text box top above EV_STATS_CY, set by where the capitals sit in the box: rows 7 to 17 in
// Gothic 18, rows 5 to 13 in Gothic 14
#define EV_STATS_TEXT_DY (EV_SMALL_RECT ? 10 : 12)
#define EV_STATS_MIN_GAP 5 // minimum space between readings
// Bold readings on round only, where the row has room
#define EV_STATS_FONT PBL_IF_ROUND_ELSE(FONT_KEY_GOTHIC_18_BOLD, EV_TEXT_FONT)
// The sunset box: Pebble's sunset icon, with the sunset and the golden hour beside it
#if PBL_ROUND
#define EV_SUNBOX_ICON     25
#define EV_SUNBOX_ICON_RES RESOURCE_ID_SUNSET_TINY
// The sunset is in the header font of the large content size, the golden hour in its caption
// font
#define EV_SUNBOX_TITLE_FONT FONT_KEY_GOTHIC_24_BOLD
#define EV_SUNBOX_GAP        6 // between the icon and the sunset
#else
#define EV_SUNBOX_ICON       (EV_SMALL_RECT ? 25 : 50)
#define EV_SUNBOX_ICON_RES   (EV_SMALL_RECT ? RESOURCE_ID_SUNSET_TINY : RESOURCE_ID_SUNSET_SMALL)
// Two rows, each a bold label with its time flush right in the regular weight
#define EV_SUNBOX_LABEL_FONT (EV_SMALL_RECT ? FONT_KEY_GOTHIC_14_BOLD : FONT_KEY_GOTHIC_18_BOLD)
#define EV_SUNBOX_INSET      4 // the icon's box, right of the card's edge
// Between the icon's box and the labels. The larger icon's art stops short of its box.
#define EV_SUNBOX_TEXT_GAP   (EV_SMALL_RECT ? 2 : 1)
#define EV_SUNBOX_MIN_GAP    5 // least space between the full golden hour label and its time
// The rows' text boxes, below the card's top. The larger display's golden hour has its
// baseline on the icon's bottom row.
#define EV_SUNBOX_ROW1_DY    (EV_SMALL_RECT ? 8 : 13)
#define EV_SUNBOX_ROW2_DY    (EV_SUNBOX_ROW1_DY + (EV_SMALL_RECT ? 16 : 20))
#endif
#define EV_SUNBOX_FILL GColorChromeYellow
// Reading glyph widths, and the gap between a glyph and its value
#define EV_GLYPH_GAP (EV_SMALL_RECT ? 3 : 4)
#define EV_WIND_W    (EV_SMALL_RECT ? 12 : 18)
#define EV_DROP_W    (EV_SMALL_RECT ? 7 : 12)
#define EV_SUN_W     (EV_SMALL_RECT ? 11 : 20)

#if PBL_ROUND
// Half the chord of the cards' inner arc at row y (0 beyond it).
static int prv_half_chord(int y) {
  const int dy = y - EV_GLASS_R;
  const int r = EV_CARD_R - EV_CARD_STROKE;
  const int q = r * r - dy * dy;
  return q > 0 ? (int)weather_isqrt(q) : 0;
}

// EV_ICON_X is a constant for the forecast's hero fly, so its half-chord is checked here
#define EV_ICON_Q                                                \
  ((EV_CARD_R - EV_CARD_STROKE) * (EV_CARD_R - EV_CARD_STROKE) - \
   (EV_ICON_Y - EV_GLASS_R) * (EV_ICON_Y - EV_GLASS_R))
static_assert(EV_ICON_HALF_CHORD * EV_ICON_HALF_CHORD <= EV_ICON_Q &&
                  EV_ICON_Q < (EV_ICON_HALF_CHORD + 1) * (EV_ICON_HALF_CHORD + 1),
              "EV_ICON_HALF_CHORD is not the inner arc's half-chord at EV_ICON_Y");
#endif

// Inner left and right edges for an element spanning rows y0 to y1. On round the chord narrows
// away from the centre, so one of the end rows is the narrowest.
static int prv_inner_left(int W, int y0, int y1) {
#if PBL_ROUND
  return W / 2 - MIN(prv_half_chord(y0), prv_half_chord(y1)) + EV_CARD_PAD;
#else
  (void)W;
  (void)y0;
  (void)y1;
  return EV_CARD_X + EV_CARD_PAD;
#endif
}

static int prv_inner_right(int W, int y0, int y1) {
#if PBL_ROUND
  return W / 2 + MIN(prv_half_chord(y0), prv_half_chord(y1)) - EV_CARD_PAD;
#else
  (void)y0;
  (void)y1;
  return W - EV_CARD_X - EV_CARD_PAD;
#endif
}

#if !PBL_ROUND
// One card's outline over rows y to y+h-1, shifted right by dx. On round prv_draw_frames_live
// draws them instead.
static void prv_draw_card(GContext *ctx, int W, int dx, int y, int h) {
  graphics_draw_round_rect(ctx, GRect(EV_CARD_X + dx, y, EV_CARD_W(W), h), EV_CARD_CORNER);
}
#endif

static GSize prv_measure(const char *text, GFont font, int W) {
  return graphics_text_layout_get_content_size(text, font, GRect(0, 0, W, 64),
                                               GTextOverflowModeFill, GTextAlignmentLeft);
}

// Draw a w by h bitmap glyph centred on `c`, one bit pattern per row, most significant bit on the
// left.
static void prv_draw_pixel_glyph(GContext *ctx, GPoint c, const uint32_t *rows, int w, int h) {
  const int x0 = c.x - w / 2, y0 = c.y - h / 2;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      if (!(rows[y] & (1u << (w - 1 - x))))
        continue;
      int run = 1;
      while (x + run < w && (rows[y] & (1u << (w - 1 - x - run))))
        run++;
      graphics_fill_rect(ctx, GRect(x0 + x, y0 + y, run, 1), 0, GCornerNone);
      x += run; // past the run and the gap that ended it
    }
  }
}

#if EV_SMALL_RECT
static void prv_draw_wind_glyph(GContext *ctx, GPoint c) {
  static const uint32_t rows[9] = {
    0x00C, // ........##..
    0x002, // ..........#.
    0xFFC, // ##########..
    0x000, // ............
    0xFFF, // ############
    0x000, // ............
    0xFF0, // ########....
    0x008, // ........#...
    0x030, // ......##....
  };
  prv_draw_pixel_glyph(ctx, c, rows, 12, 9);
}

static void prv_draw_drop_glyph(GContext *ctx, GPoint c) {
  static const uint32_t rows[10] = {
    0x08, // ...#...
    0x08, // ...#...
    0x14, // ..#.#..
    0x14, // ..#.#..
    0x22, // .#...#.
    0x41, // #.....#
    0x41, // #.....#
    0x41, // #.....#
    0x22, // .#...#.
    0x1C, // ..###..
  };
  prv_draw_pixel_glyph(ctx, c, rows, 7, 10);
}

static void prv_draw_sun_glyph(GContext *ctx, GPoint c) {
  static const uint32_t rows[11] = {
    0x020, // .....#.....
    0x222, // .#...#...#.
    0x104, // ..#.....#..
    0x070, // ....###....
    0x088, // ...#...#...
    0x68B, // ##.#...#.##
    0x088, // ...#...#...
    0x070, // ....###....
    0x104, // ..#.....#..
    0x222, // .#...#...#.
    0x020, // .....#.....
  };
  prv_draw_pixel_glyph(ctx, c, rows, 11, 11);
}
#else
static void prv_draw_wind_glyph(GContext *ctx, GPoint c) {
  static const uint32_t rows[20] = {
    0x00070, // ...........###....
    0x000F8, // ..........#####...
    0x001DC, // .........###.###..
    0x0000C, // ..............##..
    0x0001C, // .............###..
    0x3FFF8, // ###############...
    0x3FFF0, // ##############....
    0x00000, // ..................
    0x00000, // ..................
    0x3FFFF, // ##################
    0x3FFFF, // ##################
    0x00000, // ..................
    0x00000, // ..................
    0x3FFF0, // ##############....
    0x3FFF8, // ###############...
    0x0001C, // .............###..
    0x0000C, // ..............##..
    0x001DC, // .........###.###..
    0x000F8, // ..........#####...
    0x00070, // ...........###....
  };
  prv_draw_pixel_glyph(ctx, c, rows, 18, 20);
}

static void prv_draw_drop_glyph(GContext *ctx, GPoint c) {
  static const uint32_t rows[17] = {
    0x00060, // .....##.....
    0x00060, // .....##.....
    0x000F0, // ....####....
    0x000F0, // ....####....
    0x00198, // ...##..##...
    0x00198, // ...##..##...
    0x0030C, // ..##....##..
    0x0030C, // ..##....##..
    0x00606, // .##......##.
    0x00606, // .##......##.
    0x00C03, // ##........##
    0x00C03, // ##........##
    0x00C03, // ##........##
    0x00E07, // ###......###
    0x0070E, // .###....###.
    0x003FC, // ..########..
    0x001F8, // ...######...
  };
  prv_draw_pixel_glyph(ctx, c, rows, 12, 17);
}

static void prv_draw_sun_glyph(GContext *ctx, GPoint c) {
  static const uint32_t rows[20] = {
    0x00600, // .........##.........
    0x00600, // .........##.........
    0x3060C, // ..##.....##.....##..
    0x3801C, // ..###..........###..
    0x18018, // ...##..........##...
    0x00F00, // ........####........
    0x03FC0, // ......########......
    0x030C0, // ......##....##......
    0x06060, // .....##......##.....
    0xE6067, // ###..##......##..###
    0xE6067, // ###..##......##..###
    0x06060, // .....##......##.....
    0x030C0, // ......##....##......
    0x03FC0, // ......########......
    0x00F00, // ........####........
    0x18018, // ...##..........##...
    0x3801C, // ..###..........###..
    0x3060C, // ..##.....##.....##..
    0x00600, // .........##.........
    0x00600, // .........##.........
  };
  prv_draw_pixel_glyph(ctx, c, rows, 20, 20);
}
#endif // EV_SMALL_RECT

static void prv_draw_stat_text(GContext *ctx, GFont font, const char *text, int x, int w,
                               GTextAlignment align) {
  graphics_draw_text(ctx, text, font, GRect(x, EV_STATS_CY - EV_STATS_TEXT_DY, w, 24),
                     GTextOverflowModeTrailingEllipsis, align, nullptr);
}

// Fill the sunset box, dy rows below its resting place, before its outline is drawn. Black and
// white has no fill.
static void prv_fill_sunbox(GContext *ctx, int W, int dx, int dy) {
  GColor fill = PBL_IF_COLOR_ELSE(EV_SUNBOX_FILL, GColorClear);
  if (gcolor_equal(fill, GColorClear))
    return;
  if (system_theme_is_dark_mode()) {
    // Halved per channel, so white text stays legible
    fill.r /= 2;
    fill.g /= 2;
    fill.b /= 2;
  }
  graphics_context_set_fill_color(ctx, fill);
#if PBL_ROUND
  for (int y = EV_CARD3_Y + EV_CARD_STROKE; y < EV_CARD3_Y + EV_CARD3_H - EV_CARD_STROKE; y++) {
    const int h = prv_half_chord(y + dy);
    if (h > 0)
      graphics_fill_rect(ctx, GRect(W / 2 - h + dx, y + dy, 2 * h + 1, 1), 0, GCornerNone);
  }
#else
  graphics_fill_rect(ctx, GRect(EV_CARD_X + dx, EV_CARD3_Y + dy, EV_CARD_W(W), EV_CARD3_H),
                     EV_CARD_CORNER, GCornersAll);
#endif
}

typedef struct {
  GPoint icon; // the icon's top left
#if PBL_ROUND
  GRect line1; // the sunset
  GRect line2; // the golden hour
#else
  int text_x; // the labels' left edge
  int right;  // the times' right edge
#endif
} SunBoxLayout;

static SunBoxLayout prv_sunbox_layout(int W, int tdx, const ExpandedViewGlance *g) {
  SunBoxLayout l;
#if PBL_ROUND
  // The box narrows towards its bottom edge, so the icon leads the sunset as a centred pair
  // where it is wide, and the golden hour is centred beneath. The icon is as tall as the
  // title's line and sits on its baseline.
  const int y1 = EV_CARD3_Y + 3;
  // No wider than the box at the icon's bottom row, so a longer translation ends in an ellipsis
  const int max_w = 2 * (prv_half_chord(y1 + 3 + EV_SUNBOX_ICON - 1) - EV_CARD_PAD) -
                    EV_SUNBOX_ICON - EV_SUNBOX_GAP;
  const int title_w = prv_measure(g->sunset, fonts_get_system_font(EV_SUNBOX_TITLE_FONT), W).w;
  const int text_w = MIN(title_w, max_w);
  const int x = W / 2 - (EV_SUNBOX_ICON + EV_SUNBOX_GAP + text_w) / 2 + tdx;
  l.icon = GPoint(x, y1 + 3);
  l.line1 = GRect(x + EV_SUNBOX_ICON + EV_SUNBOX_GAP, y1, text_w + 4, 30);
  const int y2 = y1 + 26;
  const int left = prv_inner_left(W, y2 + 6, y2 + 22) + tdx;
  l.line2 = GRect(left, y2, prv_inner_right(W, y2 + 6, y2 + 22) + tdx - left, 24);
#else
  (void)g;
  l.icon =
      GPoint(EV_CARD_X + EV_SUNBOX_INSET + tdx, EV_CARD3_Y + (EV_CARD3_H - EV_SUNBOX_ICON) / 2);
  l.text_x = l.icon.x + EV_SUNBOX_ICON + EV_SUNBOX_TEXT_GAP;
  l.right = prv_inner_right(W, 0, 0) + tdx; // where the readings' row ends
#endif
  return l;
}

#if !PBL_ROUND
// One row of the sunset box, its text box top at y. The short label stands in where the full
// one would come within EV_SUNBOX_MIN_GAP of the time.
static void prv_draw_sunbox_row(GContext *ctx, int W, const SunBoxLayout *box, int y,
                                const char *label, const char *label_short, const char *time) {
  GFont label_font = fonts_get_system_font(EV_SUNBOX_LABEL_FONT);
  GFont time_font = fonts_get_system_font(EV_TEXT_FONT);
  const int time_w = prv_measure(time, time_font, W).w;
  // The box the readings' right-aligned text has, so the times end where the UV reading does
  const GRect time_box = GRect(box->right - time_w - 2, y, time_w + 4, 24);
  const int room = time_box.origin.x + 4 - box->text_x; // up to the time's text
  int label_w = prv_measure(label, label_font, W).w;
  if (label_short && label_w > room - EV_SUNBOX_MIN_GAP) {
    label = label_short;
    label_w = prv_measure(label, label_font, W).w;
  }
  // No wider than the label: right-to-left text is set against its box's right edge. One too
  // long for the row ends in an ellipsis 2px short of the time.
  graphics_draw_text(ctx, label, label_font, GRect(box->text_x, y, MIN(label_w, room - 2), 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, nullptr);
  graphics_draw_text(ctx, time, time_font, time_box, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight, nullptr);
}
#endif

// The sunset box's icon, dy rows below its resting place. It is drawn live, over the fill.
static void prv_draw_sunbox_icon(GContext *ctx, int W, int tdx, int dy,
                                 const ExpandedViewGlance *g) {
  const bool dark = system_theme_is_dark_mode();
  if (s_sunbox_icon && s_sunbox_icon_dark != dark) {
    gdraw_command_image_destroy(s_sunbox_icon); // recoloured for the other theme
    s_sunbox_icon = nullptr;
  }
  if (!s_sunbox_icon) {
    GDrawCommandImage *raw = gdraw_command_image_create_with_resource(EV_SUNBOX_ICON_RES);
    if (!raw)
      return;
    s_sunbox_icon = gdraw_command_image_clone(raw); // a writable copy to recolour
    gdraw_command_image_destroy(raw);
    if (!s_sunbox_icon)
      return;
    weather_recolor_pdc_image_for_dark_mode(s_sunbox_icon);
    s_sunbox_icon_dark = dark;
  }
  const SunBoxLayout l = prv_sunbox_layout(W, tdx, g);
  gdraw_command_image_draw(ctx, s_sunbox_icon, GPoint(l.icon.x, l.icon.y + dy));
}

static void prv_set_ink(GContext *ctx, GColor fg) {
  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_fill_color(ctx, fg);
  graphics_context_set_text_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, EV_CARD_STROKE);
}

// Card 1, dy rows below its resting place. On round the insets follow the rows it is drawn at.
static void prv_draw_card1(GContext *ctx, int W, int tdx, int dy, const ExpandedViewGlance *g) {
  GFont unit_font = fonts_get_system_font(EV_UNIT_FONT); // the unit letter and the high/low
  GFont text_font = fonts_get_system_font(EV_TEXT_FONT); // the feels-like label
#if !PBL_ROUND
  prv_draw_card(ctx, W, tdx, EV_CARD1_Y + dy, EV_CARD1_H);
#endif
  const int now_ink_y = EV_NOW_Y + EV_NOW_INK_DY;
  const int now_x = prv_inner_left(W, now_ink_y + dy, now_ink_y + 44 + dy) + tdx;
  const GSize unit_sz = prv_measure(g->unit, unit_font, W);
  GFont now_font = fonts_get_system_font(EV_NOW_FONT);
  GSize now_sz = prv_measure(g->now, now_font, W);
  int now_y = EV_NOW_Y + dy;
  if (now_x + now_sz.w + 3 + unit_sz.w + 4 > EV_ICON_X(W) + tdx) {
    now_font = fonts_get_system_font(EV_NOW_FONT_NARROW);
    now_sz = prv_measure(g->now, now_font, W);
    now_y += EV_NOW_NARROW_DY;
  }
  graphics_draw_text(ctx, g->now, now_font, GRect(now_x, now_y, now_sz.w + 2, 64),
                     GTextOverflowModeFill, GTextAlignmentLeft, nullptr);
  graphics_draw_text(ctx, g->unit, unit_font,
                     GRect(now_x + now_sz.w + 3, EV_NOW_Y + EV_UNIT_DY + dy, unit_sz.w + 4, 24),
                     GTextOverflowModeFill, GTextAlignmentLeft, nullptr);
  const int foot_l = prv_inner_left(W, EV_HILO_Y + 4 + dy, EV_HILO_Y + 18 + dy) + tdx;
  const int foot_r = prv_inner_right(W, EV_HILO_Y + 4 + dy, EV_HILO_Y + 18 + dy) + tdx;
  graphics_draw_text(ctx, g->temp, unit_font, GRect(foot_l, EV_HILO_Y + dy, EV_HILO_W, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, nullptr);
  // The feels-like label takes the rest of the row
  const int feels_x = foot_l + MIN(prv_measure(g->temp, unit_font, W).w, EV_HILO_W) + EV_FOOT_GAP;
  graphics_draw_text(ctx, g->feels, text_font, GRect(feels_x, EV_HILO_Y + dy, foot_r - feels_x, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, nullptr);
}

static void prv_draw_cards(GContext *ctx, int W, int tdx, const ExpandedViewGlance *g, GColor fg) {
  prv_set_ink(ctx, fg);
  prv_draw_card1(ctx, W, tdx, 0, g);
#if !PBL_ROUND
  prv_draw_card(ctx, W, tdx, EV_CARD2_Y, EV_CARD2_H);
  prv_draw_card(ctx, W, tdx, EV_CARD3_Y, EV_CARD3_H);
#endif
  GFont stat_font = fonts_get_system_font(EV_STATS_FONT); // the readings

  // The readings: wind flush left, UV flush right, humidity centred between them
  const int row_l = prv_inner_left(W, EV_STATS_CY - 10, EV_STATS_CY + 10) + tdx;
  const int row_r = prv_inner_right(W, EV_STATS_CY - 10, EV_STATS_CY + 10) + tdx;
  const GSize wind_sz = prv_measure(g->wind, stat_font, W);
  const GSize hum_sz = prv_measure(g->humidity, stat_font, W);
  // When the readings do not fit, the "UV" label is dropped and the sun glyph labels the value.
  // The small rect's readings normally sit closer than EV_STATS_MIN_GAP, so it drops the label
  // only when they would overlap.
  const int fixed_w = EV_WIND_W + EV_DROP_W + EV_SUN_W + 3 * EV_GLYPH_GAP;
  const int min_gaps = EV_SMALL_RECT ? 0 : 2 * EV_STATS_MIN_GAP;
  const char *uv_text = g->uv_text;
  GSize uv_sz = prv_measure(uv_text, stat_font, W);
  if (fixed_w + wind_sz.w + hum_sz.w + uv_sz.w + min_gaps > row_r - row_l) {
    uv_text = g->uv_value;
    uv_sz = prv_measure(uv_text, stat_font, W);
  }
  const int wind_end = row_l + EV_WIND_W + EV_GLYPH_GAP + wind_sz.w;
  const int uv_start = row_r - uv_sz.w - EV_GLYPH_GAP - EV_SUN_W;
  const int hum_w = EV_DROP_W + EV_GLYPH_GAP + hum_sz.w;
  const int hum_x = wind_end + (uv_start - wind_end - hum_w) / 2;
  prv_draw_wind_glyph(ctx, GPoint(row_l + EV_WIND_W / 2, EV_STATS_CY));
  prv_draw_stat_text(ctx, stat_font, g->wind, row_l + EV_WIND_W + EV_GLYPH_GAP, wind_sz.w + 4,
                     GTextAlignmentLeft);
  prv_draw_drop_glyph(ctx, GPoint(hum_x + EV_DROP_W / 2, EV_STATS_CY));
  prv_draw_stat_text(ctx, stat_font, g->humidity, hum_x + EV_DROP_W + EV_GLYPH_GAP, hum_sz.w + 4,
                     GTextAlignmentLeft);
  prv_draw_sun_glyph(ctx, GPoint(uv_start + EV_SUN_W / 2, EV_STATS_CY));
  prv_draw_stat_text(ctx, stat_font, uv_text, row_r - uv_sz.w - 2, uv_sz.w + 4,
                     GTextAlignmentRight);

  // The sunset box: the sunset and the golden hour. The icon beside them is drawn live.
  const SunBoxLayout box = prv_sunbox_layout(W, tdx, g);
#if PBL_ROUND
  graphics_draw_text(ctx, g->sunset, fonts_get_system_font(EV_SUNBOX_TITLE_FONT), box.line1,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, nullptr);
  graphics_draw_text(ctx, g->golden, fonts_get_system_font(EV_TEXT_FONT), box.line2,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, nullptr);
#else
  prv_draw_sunbox_row(ctx, W, &box, EV_CARD3_Y + EV_SUNBOX_ROW1_DY, g->sunset_label, nullptr,
                      g->sunset_time);
  prv_draw_sunbox_row(ctx, W, &box, EV_CARD3_Y + EV_SUNBOX_ROW2_DY, g->golden_label,
                      g->golden_short, g->golden_time);
#endif
}

#if EV_BODY_MASK
// ---- Body caches ------------------------------------------------------------------------
// Each page is drawn once at rest and read back from the framebuffer as 2-bit levels: 0 is the
// background, 1 and 2 are antialiasing blends, 3 is the foreground colour. Painting blends each
// level with the pixel beneath. The status text, the icons and the sunset box's fill are drawn
// live.
#define EV_BODY_Y         PBL_IF_ROUND_ELSE(24, 20) // first row under the status text
#define EV_BODY_H         (PBL_DISPLAY_HEIGHT - EV_BODY_Y)
#define EV_BODY_STRIDE(W) (((W) + 3) / 4) // four 2-bit levels per byte

typedef void (*BodyDrawFn)(GContext *ctx, int W, int tdx, const ExpandedViewGlance *g, GColor fg);

// Page `page`'s mask, NULL until a capture has allocated the masks
static BodyMask *prv_mask(int page) {
  return s_masks ? &s_masks[page] : nullptr;
}

static bool prv_mask_holds(const BodyMask *m, const ExpandedViewGlance *g, int W) {
  return m && m->bits && m->w == W && memcmp(&m->glance, g, sizeof(*g)) == 0;
}

static void prv_mask_free(BodyMask *m) {
  free(m->bits);
  m->bits = nullptr;
}

// Draw page `page` at rest with `draw` and read it back into its mask. It is drawn at the screen
// origin, whatever the canvas position, and those rows are cleared to the background after.
// Returns false if there is no memory or no framebuffer.
static bool prv_mask_capture(int page, BodyDrawFn draw, GContext *ctx, int W,
                             const ExpandedViewGlance *g) {
  if (!s_masks) {
    s_masks = malloc_try(EV_MASKS * sizeof(BodyMask));
    if (!s_masks)
      return false;
    memset(s_masks, 0, EV_MASKS * sizeof(BodyMask));
  }
  BodyMask *m = &s_masks[page];
  const int stride = EV_BODY_STRIDE(W);
  if (!m->bits || m->w != W) {
    prv_mask_free(m);
    m->bits = malloc_try((size_t)stride * EV_BODY_H);
    if (!m->bits)
      return false;
    m->w = W;
  }
  const GColor fg = system_theme_get_fg_color();
  const GColor bg = system_theme_get_bg_color();
  const GDrawState saved = ctx->draw_state;
  ctx->draw_state.drawing_box = GRect(0, 0, W, PBL_DISPLAY_HEIGHT);
  ctx->draw_state.clip_box = ctx->draw_state.drawing_box;
  const GRect rows = GRect(0, EV_BODY_Y, W, EV_BODY_H);
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, rows, 0, GCornerNone);
  draw(ctx, W, 0, g, fg);

  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (fb) {
    const int fb_h = gbitmap_get_bounds(fb).size.h;
    for (int y = 0; y < EV_BODY_H; y++) {
      uint8_t *row = m->bits + y * stride;
      memset(row, 0, stride);
      if (EV_BODY_Y + y >= fb_h)
        continue;
      const GBitmapDataRowInfo ri = gbitmap_get_data_row_info(fb, (uint16_t)(EV_BODY_Y + y));
      const int lo = MAX(0, (int)ri.min_x), hi = MIN(W - 1, (int)ri.max_x);
      for (int x = lo; x <= hi; x++) {
        const GColor px = {.argb = weather_fb_row_get(ri.data, x)};
        // Distance from the background, rounded to a level from 0 to 3
        const int d = abs(px.r - bg.r) + abs(px.g - bg.g) + abs(px.b - bg.b); // 0..9
        row[x / 4] |= (uint8_t)(((d * 3 + 4) / 9) << (2 * (x % 4)));
      }
    }
    graphics_release_frame_buffer(ctx, fb);
  }
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, rows, 0, GCornerNone);
  ctx->draw_state = saved;
  if (!fb) {
    prv_mask_free(m);
    return false;
  }
  m->glance = *g;
  return true;
}

// Paint `m` shifted by (dx, dy), skipping rows above `top`. Level 3 takes the foreground colour,
// levels 1 and 2 blend it with the pixel beneath.
static void prv_mask_draw(GContext *ctx, const BodyMask *m, int W, int dx, int dy, int top) {
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb)
    return;
  const GColor fg = system_theme_get_fg_color();
  const GPoint o = ctx->draw_state.drawing_box.origin;
  const GRect clip = ctx->draw_state.clip_box;
  const int fb_h = gbitmap_get_bounds(fb).size.h;
  const int stride = EV_BODY_STRIDE(W);
  for (int j = 0; j < EV_BODY_H; j++) {
    const int ly = EV_BODY_Y + j + dy; // the row in the canvas
    if (ly < top)
      continue; // above the body, or off the top in the backdrop
    const int y = o.y + ly;
    if (y < 0 || y >= fb_h || y < clip.origin.y || y >= clip.origin.y + clip.size.h)
      continue;
    const GBitmapDataRowInfo ri = gbitmap_get_data_row_info(fb, (uint16_t)y);
    const int lo = MAX((int)clip.origin.x, (int)ri.min_x);
    const int hi = MIN(clip.origin.x + clip.size.w - 1, (int)ri.max_x);
    const uint8_t *row = m->bits + j * stride;
    for (int i = 0; i < W; i += 4) {
      uint8_t levels = row[i / 4];
      if (!levels)
        continue; // four background pixels
      for (int k = 0; k < 4 && i + k < W; k++, levels >>= 2) {
        const int level = levels & 3;
        if (!level)
          continue;
        const int lx = dx + i + k;
        const int x = o.x + lx;
        if (x < lo || x > hi)
          continue;
        if (level == 3) {
          weather_fb_row_set(ri.data, x, fg.argb);
          continue;
        }
        const GColor under = {.argb = weather_fb_row_get(ri.data, x)};
        GColor c = {.argb = 0};
        c.a = 3;
        c.r = (uint8_t)((under.r * (3 - level) + fg.r * level + 1) / 3);
        c.g = (uint8_t)((under.g * (3 - level) + fg.g * level + 1) / 3);
        c.b = (uint8_t)((under.b * (3 - level) + fg.b * level + 1) / 3);
        weather_fb_row_set(ri.data, x, c.argb);
      }
    }
  }
  graphics_release_frame_buffer(ctx, fb);
}

#if PBL_ROUND
// One round card outline over rows y to y+h-1, shifted right by dx, skipping rows above `top`
static void prv_draw_round_frame(GContext *ctx, int W, int dx, int y, int h, GColor color,
                                 int top) {
  graphics_context_set_fill_color(ctx, color);
  const int r_out = EV_CARD_R, r_in = EV_CARD_R - EV_CARD_STROKE;
  for (int row = MAX(y, top); row < MIN(y + h, PBL_DISPLAY_HEIGHT); row++) {
    const int dy = row - EV_GLASS_R;
    const int qo = r_out * r_out - dy * dy;
    if (qo <= 0)
      continue;
    const int ho = (int)weather_isqrt(qo);
    const int qi = r_in * r_in - dy * dy;
    const int hi = qi > 0 ? (int)weather_isqrt(qi) : 0;
    if (row < y + EV_CARD_STROKE || row >= y + h - EV_CARD_STROKE) {
      graphics_fill_rect(ctx, GRect(W / 2 - ho + dx, row, 2 * ho + 1, 1), 0, GCornerNone);
    } else {
      graphics_fill_rect(ctx, GRect(W / 2 - ho + dx, row, ho - hi, 1), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(W / 2 + hi + 1 + dx, row, ho - hi, 1), 0, GCornerNone);
    }
  }
}

// The three card outlines on round, drawn live over the body
static void prv_draw_frames_live(GContext *ctx, int W, int tdx, GColor fg) {
  const int frames[][2] = {
    {EV_CARD1_Y, EV_CARD1_H},
    {EV_CARD2_Y, EV_CARD2_H},
    {EV_CARD3_Y, EV_CARD3_H},
  };
  for (int i = 0; i < (int)(sizeof(frames) / sizeof(frames[0])); i++) {
    prv_draw_round_frame(ctx, W, tdx, frames[i][0], frames[i][1], fg, EV_BODY_Y);
  }
}
#endif

// Clip to the body rows below the status text. The caller saves and restores the draw state.
static void prv_clip_to_body(GContext *ctx) {
  const int body_top = ctx->draw_state.drawing_box.origin.y + EV_BODY_Y;
  if (ctx->draw_state.clip_box.origin.y < body_top) {
    const int bottom = ctx->draw_state.clip_box.origin.y + ctx->draw_state.clip_box.size.h;
    ctx->draw_state.clip_box.origin.y = (int16_t)body_top;
    ctx->draw_state.clip_box.size.h = (int16_t)MAX(0, bottom - body_top);
  }
}

// Draw the cards live, dy rows down and clipped to the body, when there is no memory for a mask
static void prv_draw_live_at(GContext *ctx, BodyDrawFn draw, int W, int tdx,
                             const ExpandedViewGlance *g, GColor fg, int dy) {
  const GDrawState saved = ctx->draw_state;
  prv_clip_to_body(ctx);
  ctx->draw_state.drawing_box.origin.y += dy;
  draw(ctx, W, tdx, g, fg);
  ctx->draw_state = saved;
}

// The body at `tdx`: the sunset box's fill, then the mask. The body is drawn live when
// there is no memory for a mask.
static void prv_draw_body(GContext *ctx, int W, int tdx, const ExpandedViewGlance *g, GColor fg) {
  // Capture the mask before painting anything else: a capture clears its rows to the background.
  if (!prv_mask_holds(prv_mask(0), g, W))
    prv_mask_capture(0, prv_draw_cards, ctx, W, g);
  // The fill goes under the mask
  prv_fill_sunbox(ctx, W, tdx, 0);
#if PBL_ROUND
  prv_draw_frames_live(ctx, W, tdx, fg);
#endif
  if (prv_mask_holds(prv_mask(0), g, W))
    prv_mask_draw(ctx, prv_mask(0), W, tdx, 0, EV_BODY_Y);
  else
    prv_draw_live_at(ctx, prv_draw_cards, W, tdx, g, fg, 0); // no memory for a mask
  prv_draw_sunbox_icon(ctx, W, tdx, 0, g);
}
#else
static void prv_draw_body(GContext *ctx, int W, int tdx, const ExpandedViewGlance *g, GColor fg) {
  prv_fill_sunbox(ctx, W, tdx, 0);
  prv_draw_cards(ctx, W, tdx, g, fg);
  prv_draw_sunbox_icon(ctx, W, tdx, 0, g);
}
#endif // EV_BODY_MASK

void expanded_view_prime_body(GContext *ctx, int W, const ExpandedViewGlance *g) {
#if EV_BODY_MASK
  if (!prv_mask_holds(prv_mask(0), g, W))
    prv_mask_capture(0, prv_draw_cards, ctx, W, g);
#else
  (void)ctx;
  (void)W;
  (void)g;
#endif
}

void expanded_view_drop_body(void) {
  if (s_sunbox_icon) {
    gdraw_command_image_destroy(s_sunbox_icon);
    s_sunbox_icon = nullptr;
  }
#if EV_BODY_MASK
  if (s_masks) {
    for (int i = 0; i < EV_MASKS; i++)
      prv_mask_free(&s_masks[i]);
    free(s_masks);
    s_masks = nullptr;
  }
#endif
}

void expanded_view_draw_glance_content(GContext *ctx, int W, int tdx, const char *status,
                                       const ExpandedViewGlance *g) {
  const GColor fg = system_theme_get_fg_color();
  graphics_context_set_text_color(ctx, fg);
  // The time or "Last updated ...", NULL while the card draws the swap
  if (status) {
    graphics_draw_text(ctx, status, fonts_get_system_font(EV_STATUS_FONT),
                       GRect(tdx, EV_STATUS_Y, W, EV_STATUS_H), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, nullptr);
  }
  prv_draw_body(ctx, W, tdx, g, fg);
}

static void prv_canvas_draw(Layer *layer, GContext *ctx) {
  if (!s_ev)
    return;
  GRect b = layer_get_bounds(layer);
  const int W = b.size.w;

  graphics_context_set_fill_color(ctx, system_theme_get_bg_color());
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  const int tdx = s_ev->text_active ? (int)interpolate_moook_soft(s_ev->text_p, -W, 0, 3) : 0;

  if (s_ev->swap_active) {
    // Body without the status bar; the last-updated -> time swap is drawn on top: both slide RIGHT,
    // "Last updated" exiting off the right while the time swoops in from the left, trailing it.
    expanded_view_draw_glance_content(ctx, W, tdx, nullptr, &s_ev->glance);
    char time_str[10];
    prv_build_time(time_str, sizeof(time_str));
    const int sx = (int)interpolate_moook_soft(s_ev->swap_p, 0, W, 3);
    GFont f = fonts_get_system_font(EV_STATUS_FONT);
    graphics_context_set_text_color(ctx, system_theme_get_fg_color());
    graphics_draw_text(ctx, s_ev->updated_str, f, GRect(sx, EV_STATUS_Y, W, EV_STATUS_H),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, nullptr);
    graphics_draw_text(ctx, time_str, f, GRect(sx - W, EV_STATUS_Y, W, EV_STATUS_H),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, nullptr);
  } else {
    char time_str[10];
    const char *status = s_ev->updated_str;
    if (!s_ev->show_updated) {
      prv_build_time(time_str, sizeof(time_str));
      status = time_str;
    }
    expanded_view_draw_glance_content(ctx, W, tdx, status, &s_ev->glance);
  }

  // Drawn over the body. EV_ICON_X, EV_ICON_Y and EV_ICON_SIZE match fly_dest in
  // forecast_list.c.
  if (s_ev->icon) {
    gdraw_command_image_draw(ctx, s_ev->icon, GPoint(EV_ICON_X(W), EV_ICON_Y));
  }

#if PBL_ROUND
  // DOWN arrow below the cards, in the same 18-row band as the forecast's
  {
    const int arrow_band = 18;
    const GRect down_frame = GRect(0, b.size.h - arrow_band, W, arrow_band);
    content_indicator_draw_arrow(ctx, &down_frame, ContentIndicatorDirectionDown,
                                 system_theme_get_fg_color(), system_theme_get_bg_color(),
                                 GAlignCenter);
  }
#endif
}

// ---- SELECT -> globe: slide the whole card out to the left ----------------
// The card's canvas layer slides one screen-width left with the same moook bounce the globe uses
// for its entrance; the vacated area is the window's white background (no base flash). On
// completion on_select fires and weather.c pushes the globe sliding in from the right.

// Slide duration + interpolation are shared with globe_view via weather_math.h
// (WEATHER_HSLIDE_MS / weather_interpolate_moook_soft1) so the pair can't drift.

// BOTH shapes.
static void prv_slide_out_stopped(Animation *anim, bool finished, void *context) {
  if (!s_ev)
    return;
  s_ev->slide_anim = nullptr; // property animation auto-destroys after a normal stop
  if (finished && s_ev->on_select) {
    s_ev->on_select(s_ev->on_select_ctx); // push globe (slide-in-right) + dismiss this card
  }
}

static void prv_start_slide_out_left(void) {
  if (!s_ev || !s_ev->canvas || s_ev->slide_anim)
    return;
  GRect from = layer_get_frame(s_ev->canvas);
  GRect to = from;
  to.origin.x -= from.size.w; // one screen-width to the left, off screen
  PropertyAnimation *pa = property_animation_create_layer_frame(s_ev->canvas, &from, &to);
  if (!pa) {
    if (s_ev->on_select)
      s_ev->on_select(s_ev->on_select_ctx);
    return;
  }
  Animation *a = (Animation *)pa;
  animation_set_duration(a, WEATHER_HSLIDE_MS);
  animation_set_custom_interpolation(a, weather_interpolate_moook_soft1);
  animation_set_handlers(a, (AnimationHandlers){.stopped = prv_slide_out_stopped}, nullptr);
  s_ev->slide_anim = a;
  animation_schedule(a);
}

// Globe-BACK entrance: the whole card starts one screen-width to the left (set in prv_window_load)
// and slides home with the same moook bounce — the mirror of the SELECT slide-out-left.
static void prv_slide_in_stopped(Animation *anim, bool finished, void *context) {
  if (s_ev)
    s_ev->slide_anim = nullptr; // property animation auto-destroys after a normal stop
}

static void prv_start_slide_in_left(void) {
  if (!s_ev || !s_ev->canvas || s_ev->slide_anim)
    return;
  GRect from = layer_get_frame(s_ev->canvas); // off-left (-W), set in prv_window_load
  GRect to = from;
  to.origin.x += from.size.w; // bring it home to x = 0
  PropertyAnimation *pa = property_animation_create_layer_frame(s_ev->canvas, &from, &to);
  if (!pa) {
    layer_set_frame(s_ev->canvas, to);
    return;
  }
  Animation *a = (Animation *)pa;
  animation_set_duration(a, WEATHER_HSLIDE_MS);
  animation_set_custom_interpolation(a, weather_interpolate_moook_soft1);
  animation_set_handlers(a, (AnimationHandlers){.stopped = prv_slide_in_stopped}, nullptr);
  s_ev->slide_anim = a;
  animation_schedule(a);
}

// ---- Navigation -----------------------------------------------------------

static void prv_click_down(ClickRecognizerRef r, void *ctx) {
  if (s_ev && s_ev->on_down)
    s_ev->on_down(s_ev->on_down_ctx); // -> forecast
}
static void prv_click_select(ClickRecognizerRef r, void *ctx) {
  if (s_ev && s_ev->on_select && !s_ev->slide_anim)
    prv_start_slide_out_left(); // -> globe
}

// ---- Touch input (touch colour platforms) ----
#ifdef CONFIG_TOUCH
#define SWIPE_THRESHOLD 20 // px; same tap/swipe split as the other screens

static void prv_touch_handler(const TouchEvent *event, void *context) {
  (void)context;
  if (!s_ev)
    return;
  if (event->type == TouchEvent_Touchdown) {
    s_ev->touch_start_x = event->x;
    s_ev->touch_start_y = event->y;
    s_ev->touch_active = true;
  } else if (event->type == TouchEvent_Liftoff && s_ev->touch_active) {
    s_ev->touch_active = false;
    if (s_ev->slide_anim)
      return; // card is mid-slide — same guard as SELECT
    int16_t dx = event->x - s_ev->touch_start_x;
    int16_t dy = event->y - s_ev->touch_start_y;
    int16_t adx = dx < 0 ? -dx : dx;
    int16_t ady = dy < 0 ? -dy : dy;
    if (adx <= SWIPE_THRESHOLD && ady <= SWIPE_THRESHOLD)
      return; // tap — no action
    if (adx > ady) {
      if (dx < 0 && s_ev->on_select) {
        // Swipe left -> globe (the card slides out left, mirroring SELECT).
        prv_start_slide_out_left();
      }
    } else if (dy < 0 && s_ev->on_down) {
      s_ev->on_down(s_ev->on_down_ctx); // swipe up -> back DOWN to the forecast (mirrors DOWN)
    }
  }
}
#endif

static void prv_click_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_click_down);     // -> forecast
  window_single_click_subscribe(BUTTON_ID_BACK, prv_click_down);     // -> forecast
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_click_select); // -> globe (slide-in-right)
}

// ---- Content entrance: slide in from the left + bounce --------------------

static void prv_text_in_update(Animation *anim, AnimationProgress progress) {
  if (!s_ev)
    return;
  s_ev->text_p = progress;
  if (s_ev->canvas)
    layer_mark_dirty(s_ev->canvas);
}
static void prv_text_in_stopped(Animation *anim, bool finished, void *context) {
  if (s_ev) {
    s_ev->text_active = false;
    s_ev->text_anim = nullptr;
    if (s_ev->canvas)
      layer_mark_dirty(s_ev->canvas);
  }
  animation_destroy(anim);
}
static const AnimationImplementation s_text_in_impl = {.update = prv_text_in_update};

static void prv_start_text_in(void) {
  if (!s_ev || s_ev->text_anim)
    return;
  s_ev->text_active = true;
  s_ev->text_p = 0;
  s_ev->text_anim = animation_create();
  animation_set_implementation(s_ev->text_anim, &s_text_in_impl);
  animation_set_duration(s_ev->text_anim, interpolate_moook_soft_duration(3));
  animation_set_curve(s_ev->text_anim, AnimationCurveLinear); // interpolate_moook_soft shapes it
  animation_set_handlers(s_ev->text_anim, (AnimationHandlers){.stopped = prv_text_in_stopped},
                         nullptr);
  animation_schedule(s_ev->text_anim);
}

// ---- Status bar: "Last updated ..." -> time swap (2s after appear) --------

static void prv_swap_update(Animation *anim, AnimationProgress progress) {
  if (!s_ev)
    return;
  s_ev->swap_p = progress;
  if (s_ev->canvas)
    layer_mark_dirty(s_ev->canvas);
}
static void prv_swap_stopped(Animation *anim, bool finished, void *context) {
  if (s_ev) {
    s_ev->swap_active = false;
    s_ev->show_updated = false; // the status bar now rests on the time
    s_ev->swap_anim = nullptr;
    if (s_ev->canvas)
      layer_mark_dirty(s_ev->canvas);
  }
  animation_destroy(anim);
}
static const AnimationImplementation s_swap_impl = {.update = prv_swap_update};

static void prv_start_swap(void) {
  if (!s_ev || s_ev->swap_anim || !s_ev->show_updated)
    return;
  s_ev->swap_active = true;
  s_ev->swap_p = 0;
  s_ev->swap_anim = animation_create();
  animation_set_implementation(s_ev->swap_anim, &s_swap_impl);
  animation_set_duration(s_ev->swap_anim, interpolate_moook_soft_duration(3));
  animation_set_curve(s_ev->swap_anim, AnimationCurveLinear); // interpolate_moook_soft shapes it
  animation_set_handlers(s_ev->swap_anim, (AnimationHandlers){.stopped = prv_swap_stopped},
                         nullptr);
  animation_schedule(s_ev->swap_anim);
}

static void prv_updated_timer_cb(void *ctx) {
  if (!s_ev)
    return;
  s_ev->updated_timer = nullptr;
  prv_start_swap();
}

// ---- Window lifecycle -----------------------------------------------------

static void prv_window_appear(Window *window) {
  if (!s_ev)
    return;
#ifdef CONFIG_TOUCH
  touch_service_subscribe(prv_touch_handler, s_ev);
#endif
  // Hold "Last updated ..." for 2s, then swap it out for the time (once).
  if (!s_ev->updated_timer && s_ev->show_updated && !s_ev->swap_active) {
    s_ev->updated_timer = app_timer_register(2000, prv_updated_timer_cb, nullptr);
  }
  // Play the entrance animation (latched off after the first appearance so button-nav re-appears
  // don't replay it). Static = no entrance (the forecast's hero icon-fly already animated it in).
  switch (s_ev->entrance) {
    case ExpandedViewEntranceTextFromLeft:
      prv_start_text_in();
      break;
    case ExpandedViewEntranceCardFromLeft:
      prv_start_slide_in_left();
      break;
    case ExpandedViewEntranceStatic:
    default:
      break;
  }
  s_ev->entrance = ExpandedViewEntranceStatic;
}

static void prv_window_load(Window *window) {
  if (!s_ev)
    return;
  window_set_background_color_follows_theme(window, false /* dimmed */);
  GRect bounds = layer_get_bounds(window_get_root_layer(window));
  GRect frame = bounds;
  // Globe-BACK entrance (BOTH shapes): start the whole card off-screen to the left so its first
  // painted frame is already off-left; prv_window_appear then slides it home. (layer_create
  // zeroes the bounds origin, so the draw proc is unaffected.)
  if (s_ev->entrance == ExpandedViewEntranceCardFromLeft) {
    frame.origin.x -= bounds.size.w;
  }
  s_ev->canvas = layer_create(frame);
  layer_set_update_proc(s_ev->canvas, prv_canvas_draw);
  layer_add_child(window_get_root_layer(window), s_ev->canvas);
}

#ifdef CONFIG_TOUCH
// Unsubscribe on DISAPPEAR, not unload: when SELECT/swipe-left pushes the globe, weather.c
// then REMOVES this card from under it — the card's late unload would clobber the touch
// subscription the globe just made in its own appear (touch_service is a single slot).
static void prv_window_disappear(Window *window) {
  (void)window;
  touch_service_unsubscribe();
}
#endif

static void prv_window_unload(Window *window) {
  if (s_ev && s_ev->updated_timer) {
    app_timer_cancel(s_ev->updated_timer);
    s_ev->updated_timer = nullptr;
  }
  if (s_ev && s_ev->swap_anim) {
    animation_unschedule(s_ev->swap_anim); // fires prv_swap_stopped -> destroys + NULLs it
  }
  if (s_ev && s_ev->text_anim) {
    animation_unschedule(s_ev->text_anim); // fires prv_text_in_stopped -> destroys + NULLs it
  }
  if (s_ev && s_ev->slide_anim) {
    animation_unschedule(s_ev->slide_anim); // fires prv_slide_*_stopped -> NULLs it
  }
  if (s_ev) {
    if (s_ev->icon) {
      gdraw_command_image_destroy(s_ev->icon);
      s_ev->icon = nullptr;
    }
    if (s_ev->canvas) {
      layer_destroy(s_ev->canvas);
      s_ev->canvas = nullptr;
    }
  }
  expanded_view_drop_body(); // recaptured by the next hero fly
  window_destroy(window);
  if (s_ev) {
    pbl_i18n_free_all(s_ev);
    free(s_ev);
    s_ev = nullptr;
  }
}

// ---- Public API -----------------------------------------------------------

void expanded_view_push(const WeatherLocationForecast *today, int16_t lat_e2, int16_t lon_e2,
                        int16_t utc_off_min, ExpandedViewEntrance entrance,
                        void (*on_down)(void *ctx), void *on_down_ctx, void (*on_select)(void *ctx),
                        void *on_select_ctx) {
  if (s_ev)
    return;
  s_ev = calloc(1, sizeof(ExpandedViewData));
  if (!s_ev)
    return;
  s_ev->on_down = on_down;
  s_ev->on_down_ctx = on_down_ctx;
  s_ev->on_select = on_select;
  s_ev->on_select_ctx = on_select_ctx;
  s_ev->entrance = entrance;
  // Round's status band is a narrow chord — no room for "Last updated H:MM" at this size,
  // and shrinking or moving it was rejected. It rests on the TIME from the outset, so the
  // 2s hold + swap never arm (they would only slide the time out and back in).
  s_ev->show_updated = PBL_IF_ROUND_ELSE(false, true);
  prv_set_from_forecast(today, lat_e2, lon_e2, utc_off_min);

  s_ev->window = window_create();
  if (!s_ev->window) {
    free(s_ev);
    s_ev = nullptr;
    return;
  }
  window_set_window_handlers(s_ev->window, (WindowHandlers){
                                             .load = prv_window_load,
                                             .appear = prv_window_appear,
#ifdef CONFIG_TOUCH
                                             .disappear = prv_window_disappear,
#endif
                                             .unload = prv_window_unload,
                                           });
  window_set_click_config_provider(s_ev->window, prv_click_provider);
  // Un-animated: the forecast's hero icon-fly is the entrance; the card's static icon takes over
  // exactly where the flown icon landed (a system slide would fight that hand-off).
  window_stack_push(s_ev->window, false);
}

void expanded_view_dismiss(bool animated) {
  if (s_ev && s_ev->window) {
    window_stack_remove(s_ev->window, animated);
  }
}

bool expanded_view_is_showing(void) {
  return s_ev && s_ev->window;
}

void expanded_view_update_data(const WeatherLocationForecast *today, int16_t lat_e2, int16_t lon_e2,
                               int16_t utc_off_min) {
  if (!s_ev)
    return;
  prv_set_from_forecast(today, lat_e2, lon_e2, utc_off_min);
  if (s_ev->canvas)
    layer_mark_dirty(s_ev->canvas);
}
