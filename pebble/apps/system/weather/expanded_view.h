/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "weather_types.h"

// 144x168 displays run the same layout at a smaller scale.
#define EV_SMALL_RECT (!PBL_ROUND && PBL_DISPLAY_HEIGHT < 200)
// The weather icon's box in the top card's right column, also forecast_list.c's fly_dest
#define EV_ICON_SIZE (EV_SMALL_RECT ? 40 : 64)
// The cards' side margin and inner padding. On round the margin is the gap between the display's
// edge and the cards' outer arc.
#define EV_CARD_X   (EV_SMALL_RECT ? 2 : 4)
#define EV_CARD_PAD (EV_SMALL_RECT ? 5 : 8)
// On round the icon column's right edge is EV_CARD_PAD inside the cards' inner arc at the icon's
// top row. EV_ICON_HALF_CHORD is that arc's half-chord there, which expanded_view.c checks
// against the display size.
#define EV_ICON_Y          PBL_IF_ROUND_ELSE(44, (EV_SMALL_RECT ? 20 : 30))
#define EV_ICON_HALF_CHORD 89
#define EV_ICON_X(W)                                                             \
  PBL_IF_ROUND_ELSE(((W) / 2 + EV_ICON_HALF_CHORD - EV_CARD_PAD - EV_ICON_SIZE), \
                    ((W) - EV_CARD_X - EV_CARD_PAD - EV_ICON_SIZE))

// How the card animates in when pushed.
typedef enum {
  ExpandedViewEntranceStatic = 0,   // no entrance — the forecast's hero icon-fly already placed it
  ExpandedViewEntranceTextFromLeft, // the glance text slides in from the left (arriving from globe
                                    // DOWN)
  ExpandedViewEntranceCardFromLeft, // the whole card slides in from the left (arriving from globe
                                    // BACK)
} ExpandedViewEntrance;

// Full-screen weather card for today, modelled on the timeline weather pin
// (pebble/services/timeline/weather_layout.c): the status time over three outlined cards for the
// conditions, a row of readings and the sunset.
//
// It sits above the forecast list. DOWN and BACK fire on_down. SELECT slides the card out to the
// left, then fires on_select. UP does nothing.
//
// `today` is borrowed only for the duration of this call; the fields are copied, so
// the caller may free/reuse its forecast afterward. lat_e2/lon_e2 are the active
// location's coordinates (degrees * 100, INT16_MIN if unknown) — used to compute the
// local sunset time (there is no sunset field in the synced weather data).
// utc_off_min: the LOCATION's timezone (minutes east of UTC, v4.1) so the sunset reads in
// city-local time for saved cities; INT16_MIN falls back to the watch's timezone.
// entrance: how the card animates in (see ExpandedViewEntrance). Use ...Static when arriving via
// the forecast's hero icon-fly (which already animated the text in, synced to the icon landing),
// ...TextFromLeft from globe DOWN, ...CardFromLeft from globe BACK (mirrors the SELECT slide-out).
//! Clear the launch-persistent statics; call once from the app's init (a crashed
//! run never reaches unload, so the next launch would read stale pointers).
void expanded_view_reset(void);

void expanded_view_push(const WeatherLocationForecast *today, int16_t lat_e2, int16_t lon_e2,
                        int16_t utc_off_min, ExpandedViewEntrance entrance,
                        void (*on_down)(void *ctx), void *on_down_ctx, void (*on_select)(void *ctx),
                        void *on_select_ctx);

// The card body's text, formatted once so the card and the forecast's hero icon-fly draw the same
// strings. Every field a display has is filled, drawn there or not.
typedef struct {
#if PBL_ROUND
  char sunset[40]; // "Sunset at 9:30 PM"
  char golden[40]; // "Golden hour: 8:36 PM", when the evening golden hour starts
#else
  // The sunset box's two rows, a label and its time
  char sunset_label[32]; // "Sunset"
  char sunset_time[16];  // "9:30 PM", "--:--" if unknown
  char golden_label[32]; // "Golden hour", when the evening golden hour starts
  char golden_short[32]; // "Golden hr", for where the full label would reach the time
  char golden_time[16];  // "8:36 PM", "--:--" on a day without one
#endif
  char feels[40];    // "Feels like 16°", "Feels like --" if unknown
  char temp[16];     // the day's high/low and a degree sign
  char now[12];      // the current temperature's digits, no unit ("18", "-3", "--")
  char unit[4];      // "C" or "F", the user's temperature unit
  char wind[16];     // "16km/h" in the user's wind unit, "--" if unknown
  char humidity[12]; // "64%", "--" if unknown
  char uv_text[32];  // "UV 4"
  char uv_value[8];  // "4", shown without the label when the readings are tight
} ExpandedViewGlance;

// Format the glance as the card renders it. `owner` scopes the i18n lookups. The caller frees
// them with pbl_i18n_free_all(owner).
void expanded_view_format_glance(const WeatherLocationForecast *today, int16_t lat_e2,
                                 int16_t lon_e2, int16_t utc_off_min, ExpandedViewGlance *out,
                                 const void *owner);

// Draw the card body without the icon, shifted right by `tdx`. Shared by the card and the
// forecast's hero icon-fly. `status` is the status bar's text, the time or "Last updated ...".
// NULL skips the status bar, which the card draws itself while swapping the two.
void expanded_view_draw_glance_content(GContext *ctx, int W, int tdx, const char *status,
                                       const ExpandedViewGlance *glance);

// Capture the card body's mask for `glance`, unless it is held. Call this before the rest of a
// frame that repaints the whole screen: the capture draws the body, then clears its rows. No-op
// on black and white displays.
void expanded_view_prime_body(GContext *ctx, int W, const ExpandedViewGlance *glance);

// Free the body mask and the sunset box's icon.
void expanded_view_drop_body(void);

// Format "Last updated H:MM" from the forecast's time_updated_utc, exactly as the card shows it.
void expanded_view_format_updated(const WeatherLocationForecast *today, char *out, size_t out_sz);

// Remove the card if it is currently on the stack.
void expanded_view_dismiss(bool animated);

bool expanded_view_is_showing(void);

// Refresh the displayed data when a new record arrives while the card is up.
// No-op if the card is not currently showing.
void expanded_view_update_data(const WeatherLocationForecast *today, int16_t lat_e2, int16_t lon_e2,
                               int16_t utc_off_min);
