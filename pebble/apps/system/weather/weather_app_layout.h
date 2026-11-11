/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "weather_types.h"

#include <stdint.h>

#define WEATHER_APP_LAYOUT_LOCATION_BAR_HEIGHT (18)
// Main-screen bar is taller than the forecast-list bar for readability.
// The small rects (144x168) need every row: keep the forecast-list height there.
#if !PBL_ROUND && PBL_DISPLAY_HEIGHT < 200
#define WEATHER_APP_LAYOUT_MAIN_BAR_HEIGHT (20)
#else
#define WEATHER_APP_LAYOUT_MAIN_BAR_HEIGHT (26)
#endif
// Round (gabbro) draws the large 75px vector weather icon (WX_WEATHER_ICONS_PDC)
// + the day swoop. Emery (PBL_ROUND==0) keeps the raster BitmapLayer path.
// IMPORTANT: a system app's PDC resource is mmap'd READ-ONLY into flash, so the
// sequence must be CLONED into RAM before any write (gdraw_command_sequence_set_
// bounds_size writes sequence->size and faulted on launch). See the clone in
// weather_app_layout_init.
// Round uses the LARGE (80x80) PDC art so the icon fills its quadrant; rect
// keeps the 50px raster. The 14/10 disc ratio is too fat at that size, so round
// also takes a tighter ratio -- see WEATHER_APP_LAYOUT_DISC_RATIO_NUM.
#define WEATHER_APP_LAYOUT_USE_PDC_WEATHER_ICONS PBL_ROUND

// Widest int16_t temperature followed by a UTF-8 degree sign (U+00B0)
#define WEATHER_TEMP_STR_MAX "-32768\xC2\xB0"

#define WEATHER_APP_LAYOUT_HIGH_LOW_LENGTH (2 * (sizeof(WEATHER_TEMP_STR_MAX) - 1) + sizeof(" / "))

typedef struct WeatherAppLayout {
  Layer *root_layer;
  Layer *content_layer;
  BitmapLayer *current_weather_icon_layer;
  Layer *outgoing_weather_icon_layer;
  BitmapLayer *tomorrow_weather_icon_layer;
#if WEATHER_APP_LAYOUT_USE_PDC_WEATHER_ICONS
  GDrawCommandSequence *weather_icon_pdc_sequence;
  Layer *current_weather_escape_layer;
#endif
  GBitmap *current_weather_icon;
  GBitmap *outgoing_weather_icon;
  GBitmap *tomorrow_weather_icon;
  GDrawCommandImage *fin_pdc; // real timeline 'fin' flag (END_OF_TIMELINE PDC)
  Layer *fin_layer;
  Layer *city_layer; //!< round: the crown city masthead (topmost root child)
  Animation *fin_animation;
  const WeatherLocationForecast *forecast;
  const WeatherLocationForecast *next_forecast;
  GFont location_font;
  GFont temperature_font;
  GFont high_low_phrase_font;
  GFont metrics_font;
  GFont metrics_value_font; // bold metric VALUES (Timeline label/value texture)
  GFont tomorrow_font;
  Layer *down_arrow_layer;
  Layer *location_bar_layer;
  char location_name[32];
  bool fin_allowed;
  GPoint content_layer_origin; // screen-absolute origin of content_layer
  GRect today_icon_rest_frame;
  GRect tomorrow_icon_rest_frame;
  Animation *icon_animation;
  struct {
    GPoint circle_center;
    int32_t radius;
    int32_t outgoing_start_angle;
    int32_t outgoing_end_angle;
    int32_t incoming_start_angle;
    // incoming always ends at ICON_ARC_TODAY_REST
    WeatherType outgoing_weather_type;
    WeatherType incoming_weather_type;
    WeatherType tomorrow_exit_weather_type; // weather type of tomorrow icon while it exits/enters
    // Y in root_layer coordinates where the new day's sidebar color meets the old one. It moves
    // with the incoming icon, and the new color fills the bar as the icon lands.
    int16_t sidebar_edge_y;
    bool animate_down;
    bool tomorrow_reparented; // true when tomorrow_layer lives in root_layer during animation
    bool tomorrow_incoming;   // true when tomorrow_layer is animating IN during DOWN animation
#if WEATHER_APP_LAYOUT_USE_PDC_WEATHER_ICONS
    bool current_root_overlay; // Gabbro-only: current icon exits above content clipping
#endif
  } anim_params;
  struct {
    GRect from;
    GRect to;
  } fin_anim;
  struct {
    // Pre-formatted strings snapshot of the OUTGOING text (captured before forecast pointer
    // updates)
    char top_label[24];
    char top_temp[15];
    char top_highlow[WEATHER_APP_LAYOUT_HIGH_LOW_LENGTH];
    char top_uv[12];     // UV bar numeral
    char top_phrase[32]; // condition line
    char top_desc[128];  // forecast description (warning/wind/precip)
    char bot_label[24];
    char bot_highlow[WEATHER_APP_LAYOUT_HIGH_LOW_LENGTH];
    bool bot_valid;
    AnimationProgress progress;
    bool dir_down;
    bool active;
  } text_anim;
} WeatherAppLayout;

void weather_app_layout_init(WeatherAppLayout *layout, const GRect *frame);

void weather_app_layout_set_data(WeatherAppLayout *layout,
                                 const WeatherLocationForecast *today_forecast,
                                 const WeatherLocationForecast *next_forecast);

//! Recolors the resources loaded once for the new theme. The icons that depend on the data are
//! reloaded by \ref weather_app_layout_set_data.
void weather_app_layout_handle_dark_mode_change(WeatherAppLayout *layout);

#define weather_app_layout_set_down_arrow_visible(layout, is_down_visible) \
  do {                                                                     \
    (void)(layout);                                                        \
    (void)(is_down_visible);                                               \
  } while (0)

void weather_app_layout_set_fin_allowed(WeatherAppLayout *layout, bool fin_allowed);

void weather_app_layout_set_location(WeatherAppLayout *layout, const char *name);

void weather_app_layout_deinit(WeatherAppLayout *layout);

void weather_app_layout_animate(WeatherAppLayout *layout, const WeatherLocationForecast *new_today,
                                const WeatherLocationForecast *new_next, bool animate_down);

// "16km/h": `mph` in the user's wind unit, "--" when unknown. `owner` scopes the i18n lookups.
void weather_app_layout_format_wind(int mph, const void *owner, char *buf, size_t buf_size);

// The UV reading as `text`, "UV 4" with the label wherever its translation puts it, and as the
// bare `value`, "4". "UV --" and "--" when `uv` is negative. `owner` scopes the i18n lookups.
void weather_app_layout_format_uv(int uv, const void *owner, char *text, size_t text_size,
                                  char *value, size_t value_size);
