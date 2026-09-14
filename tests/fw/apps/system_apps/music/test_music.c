/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/fonts/fonts.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/window_private.h"
#include "pbl/services/imaging.h"
#include "pbl/services/music.h"
#include "pbl/util/size.h"
#include "shell/system_theme.h"

#include "clar.h"

#include <stdio.h>
#include <string.h>

// Fakes
/////////////////////

#include "fake_spi_flash.h"
#include "fixtures/load_test_resources.h"

// Stubs
/////////////////////

#include "stubs_analytics.h"
#include "stubs_animation_timing.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_state.h"
#include "stubs_app_timer.h"
#include "stubs_app_window_stack.h"
#include "stubs_bootbits.h"
#include "stubs_click.h"
#include "stubs_event_service_client.h"
#include "stubs_i18n.h"
#include "stubs_layer.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_process_manager.h"
#include "stubs_prompt.h"
#include "stubs_serial.h"
#include "stubs_sleep.h"
#include "stubs_syscalls.h"
#include "stubs_task_watchdog.h"
#include "stubs_vibe_score.h"
#include "stubs_window_manager.h"
#include "stubs_window_stack.h"

// Music service fake
/////////////////////

static char s_music_title[MUSIC_BUFFER_LENGTH];
static char s_music_artist[MUSIC_BUFFER_LENGTH];
static MusicPlayState s_music_play_state;
static uint32_t s_music_track_pos_ms;
static uint32_t s_music_track_length_ms;
static bool s_music_needs_user_to_start_playback;
static bool s_music_progress_supported;
static bool s_music_output_routing_supported;
static MusicOutputRouteStatus s_music_output_route_status;
static MusicOutputRoute s_music_output_routes[MUSIC_OUTPUT_ROUTE_MAX_COUNT];
static uint8_t s_music_output_route_count;
static unsigned int s_music_output_route_request_count;
static uint8_t s_selected_output_route_generation;
static uint8_t s_selected_output_route_id;

void music_get_now_playing(char *title, char *artist, char *album) {
  if (title) {
    strcpy(title, s_music_title);
  }
  if (artist) {
    strcpy(artist, s_music_artist);
  }
  if (album) {
    album[0] = '\0';
  }
}

MusicPlayState music_get_playback_state(void) {
  return s_music_play_state;
}

void music_get_pos(uint32_t *track_pos_ms, uint32_t *track_length_ms) {
  *track_pos_ms = s_music_track_pos_ms;
  *track_length_ms = s_music_track_length_ms;
}

bool music_is_progress_reporting_supported(void) {
  return s_music_progress_supported;
}

bool music_is_output_routing_supported(void) {
  return s_music_output_routing_supported;
}

MusicOutputRouteStatus music_get_output_route_status(void) {
  return s_music_output_route_status;
}

uint8_t music_get_output_route_count(void) {
  return s_music_output_route_count;
}

uint8_t music_get_output_routes(MusicOutputRouteStatus *status_out, MusicOutputRoute *routes_out,
                                uint8_t max_count) {
  *status_out = s_music_output_route_status;
  const uint8_t count = MIN(s_music_output_route_count, max_count);
  if (count) {
    memcpy(routes_out, s_music_output_routes, count * sizeof(*routes_out));
  }
  return count;
}

bool music_get_output_route(uint8_t index, MusicOutputRoute *route_out) {
  if (index >= s_music_output_route_count) {
    return false;
  }
  *route_out = s_music_output_routes[index];
  return true;
}

void music_request_output_routes(void) {
  s_music_output_route_request_count++;
  s_music_output_route_status = MusicOutputRouteStatusLoading;
  s_music_output_route_count = 0;
}

void music_select_output_route(uint8_t generation, uint8_t route_id) {
  s_selected_output_route_generation = generation;
  s_selected_output_route_id = route_id;
}

bool music_needs_user_to_start_playback_on_phone(void) {
  return s_music_needs_user_to_start_playback;
}

bool music_is_command_supported(MusicCommand command) {
  return true;
}

bool music_skip_seeks_within_track(void) {
  return false;
}

void music_command_send(MusicCommand command) {}

void music_request_reduced_latency(bool reduced_latency) {}

void music_request_low_latency_for_period(uint32_t period_ms) {}

bool music_has_now_playing(void) {
  return s_music_title[0] != '\0' || s_music_artist[0] != '\0';
}

static uint8_t s_music_now_playing_generation;

uint8_t music_get_now_playing_generation(void) {
  return s_music_now_playing_generation;
}

// Album art fake: the tests hand the "service-owned" cover bitmap straight to the app.
static GBitmap *s_album_art;
static bool s_album_art_current;
static int s_album_art_lock_depth;

const struct GBitmap *music_album_art_lock(void) {
  s_album_art_lock_depth++;
  return s_album_art;
}

void music_album_art_unlock(void) {
  cl_assert(s_album_art_lock_depth > 0);
  s_album_art_lock_depth--;
}

bool music_album_art_is_current(void) {
  return s_album_art_current;
}

// Imaging service fake: records the album art requests the app makes.
static bool s_imaging_supported;
static int s_imaging_request_count;
static uint8_t s_imaging_request_token;
static ImagingFormat s_imaging_request_format;
static uint16_t s_imaging_request_width;
static uint16_t s_imaging_request_height;
static char s_imaging_request_title[MUSIC_BUFFER_LENGTH];
static char s_imaging_request_artist[MUSIC_BUFFER_LENGTH];

bool imaging_is_type_supported(ImagingImageType image_type) {
  return s_imaging_supported;
}

bool imaging_request_album_art(uint8_t token, ImagingFormat format, uint16_t width, uint16_t height,
                               const char *title, const char *artist) {
  if (!s_imaging_supported) {
    return false;
  }
  s_imaging_request_count++;
  s_imaging_request_token = token;
  s_imaging_request_format = format;
  s_imaging_request_width = width;
  s_imaging_request_height = height;
  strncpy(s_imaging_request_title, title, sizeof(s_imaging_request_title) - 1);
  strncpy(s_imaging_request_artist, artist, sizeof(s_imaging_request_artist) - 1);
  return true;
}

// Shell prefs fake
/////////////////////

static bool s_prefs_music_show_volume_controls;
static bool s_prefs_music_show_progress_bar;
static bool s_prefs_music_show_album_art;

bool shell_prefs_get_music_show_volume_controls(void) {
  return s_prefs_music_show_volume_controls;
}

bool shell_prefs_get_music_show_progress_bar(void) {
  return s_prefs_music_show_progress_bar;
}

bool shell_prefs_get_music_show_album_art(void) {
  return s_prefs_music_show_album_art;
}

GColor shell_prefs_get_theme_highlight_color(void) {
  return GColorWhite;
}

// Misc stubs
/////////////////////

static const char *s_menu_cell_title;
static const char *s_menu_cell_subtitle;
static unsigned int s_menu_reload_count;

void app_event_loop(void) {}

void menu_layer_init(MenuLayer *menu_layer, const GRect *frame) {}

void menu_layer_deinit(MenuLayer *menu_layer) {}

Layer *menu_layer_get_layer(const MenuLayer *menu_layer) {
  return (Layer *)menu_layer;
}

void menu_layer_set_callbacks(MenuLayer *menu_layer, void *callback_context,
                              const MenuLayerCallbacks *callbacks) {}

void menu_layer_set_click_config_onto_window(MenuLayer *menu_layer, Window *window) {}

void menu_layer_reload_data(MenuLayer *menu_layer) {
  s_menu_reload_count++;
}

void menu_layer_set_normal_colors(MenuLayer *menu_layer, GColor background, GColor foreground) {}

void menu_layer_set_highlight_colors(MenuLayer *menu_layer, GColor background, GColor foreground) {}

void menu_cell_basic_draw(GContext *ctx, const Layer *cell_layer, const char *title,
                          const char *subtitle, GBitmap *icon) {
  s_menu_cell_title = title;
  s_menu_cell_subtitle = subtitle;
}

void tick_timer_service_subscribe(TimeUnits tick_units, TickHandler handler) {}

void tick_timer_service_unsubscribe(void) {}

void accel_tap_service_subscribe(AccelTapHandler handler) {}

void accel_tap_service_unsubscribe(void) {}

VibeScore *vibe_score_create_with_resource(uint32_t resource_id) {
  return NULL;
}

PropertyAnimation *property_animation_create_bounds_origin(struct Layer *layer, GPoint *from,
                                                           GPoint *to) {
  return NULL;
}

void property_animation_update_grect(PropertyAnimation *property_animation,
                                     const uint32_t distance_normalized) {}

bool scroll_layer_is_instance(const Layer *layer) {
  return false;
}

uint16_t time_ms(time_t *tloc, uint16_t *out_ms) {
  return 0;
}

void clock_copy_time_string(char *buffer, uint8_t size) {
  strncpy(buffer, "12:00 PM", size);
}

// Mirror the real system theme font table for the default content size, so
// text renders exactly as it does on the target platform.
GFont system_theme_get_font_for_default_size(TextStyleFont font) {
  const bool large = (PreferredContentSizeDefault == PreferredContentSizeLarge);
  const char *key;
  switch (font) {
    case TextStyleFont_Header:
      key = large ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD;
      break;
    case TextStyleFont_Subtitle:
      key = large ? FONT_KEY_GOTHIC_28 : FONT_KEY_GOTHIC_24_BOLD;
      break;
    default:
      key = FONT_KEY_GOTHIC_18_BOLD;
      break;
  }
  return fonts_get_system_font(key);
}

// Helper Functions
/////////////////////

#include "fw/graphics/util.h"

// App under test
/////////////////////

#include "apps/system/music.c"

// Setup and Teardown
////////////////////////////////////

static GContext s_ctx;
static FrameBuffer s_fb;

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

void test_music__initialize(void) {
  s_music_title[0] = '\0';
  s_music_artist[0] = '\0';
  s_music_play_state = MusicPlayStateUnknown;
  s_music_track_pos_ms = 0;
  s_music_track_length_ms = 0;
  s_music_needs_user_to_start_playback = false;
  s_music_progress_supported = false;
  s_music_now_playing_generation = 0;
  s_music_output_routing_supported = false;
  s_music_output_route_status = MusicOutputRouteStatusUnsupported;
  s_music_output_route_count = 0;
  s_music_output_route_request_count = 0;
  s_selected_output_route_generation = 0;
  s_selected_output_route_id = 0;
  s_menu_cell_title = NULL;
  s_menu_cell_subtitle = NULL;
  s_menu_reload_count = 0;

  s_album_art = NULL;
  s_album_art_current = false;
  s_album_art_lock_depth = 0;

  s_imaging_supported = false;
  s_imaging_request_count = 0;

  // Firmware defaults
  s_prefs_music_show_volume_controls = true;
  s_prefs_music_show_progress_bar = true;
  s_prefs_music_show_album_art = false;

  framebuffer_init(&s_fb, &(GSize){DISP_COLS, DISP_ROWS});
  framebuffer_clear(&s_fb);
  graphics_context_init(&s_ctx, &s_fb, GContextInitializationMode_App);
  s_app_state_get_graphics_context = &s_ctx;

  fake_spi_flash_init(0 /* offset */, 0x1000000 /* length */);
  pfs_init(false /* run filesystem check */);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME,
                                 false /* is_next */);
  resource_init();
}

void test_music__cleanup(void) {
  // The app must never leave the service's art bitmap locked.
  cl_assert_equal_i(s_album_art_lock_depth, 0);
  if (s_album_art) {
    free(s_album_art->addr);
    free(s_album_art->palette);
    free(s_album_art);
    s_album_art = NULL;
  }
}

// Helpers
//////////////////////

static void prv_set_now_playing(const char *title, const char *artist) {
  strncpy(s_music_title, title, sizeof(s_music_title));
  s_music_title[sizeof(s_music_title) - 1] = '\0';
  strncpy(s_music_artist, artist, sizeof(s_music_artist));
  s_music_artist[sizeof(s_music_artist) - 1] = '\0';
}

static void prv_launch_app_and_render(void) {
  prv_handle_init();
  MusicAppData *data = app_state_get_user_data();
  Window *window = data->no_music_window ? &data->no_music_window->window : &data->window;
  window_set_on_screen(window, true, true);
  window_render(window, &s_ctx);
}

static void prv_render(void) {
  MusicAppData *data = app_state_get_user_data();
  window_render(&data->window, &s_ctx);
}

#if MUSIC_ALBUM_ART_SUPPORTED
// Hand the app the cover the imaging service would have delivered: art-square-sized (the size the
// app requests for this platform), 4-bit palette.
static void prv_receive_album_art(void) {
  s_album_art = get_gbitmap_from_pbi("test_music__art_cover_" PLATFORM_NAME ".pbi");
  cl_assert(s_album_art != NULL);
  s_album_art_current = true;
}
#endif

// Tests
//////////////////////

void test_music__playing(void) {
  prv_set_now_playing("Just Like Heaven", "The Cure");
  s_music_play_state = MusicPlayStatePlaying;
  s_music_track_pos_ms = 75 * 1000;
  s_music_track_length_ms = 245 * 1000;
  s_music_progress_supported = true;

  prv_launch_app_and_render();
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_music__playing_long_text(void) {
  prv_set_now_playing("It Could Be The First Day Of Springtime", "Godspeed You! Black Emperor");
  s_music_play_state = MusicPlayStatePlaying;
  s_music_track_pos_ms = 754 * 1000;
  s_music_track_length_ms = 3945 * 1000;
  s_music_progress_supported = true;

  prv_launch_app_and_render();
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_music__paused(void) {
  prv_set_now_playing("Just Like Heaven", "The Cure");
  s_music_play_state = MusicPlayStatePaused;
  s_music_track_pos_ms = 75 * 1000;
  s_music_track_length_ms = 245 * 1000;
  s_music_progress_supported = true;

  prv_launch_app_and_render();
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_music__no_progress_reporting(void) {
  prv_set_now_playing("Just Like Heaven", "The Cure");
  s_music_play_state = MusicPlayStatePlaying;

  prv_launch_app_and_render();
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_music__no_music(void) {
  s_music_needs_user_to_start_playback = true;

  prv_launch_app_and_render();
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

// Album art
//////////////////////

// On launch with art enabled but none fetched yet, the app asks the imaging service for a cover
// sized to its art square, keyed to the current track. Platforms without art support never ask.
void test_music__album_art_requested_on_launch(void) {
  prv_set_now_playing("Crumbling Castle", "King Gizzard & The Lizard Wizard");
  s_music_play_state = MusicPlayStatePlaying;
  s_music_now_playing_generation = 1;
  s_imaging_supported = true;
  s_prefs_music_show_album_art = true;

  prv_launch_app_and_render();

#if MUSIC_ALBUM_ART_SUPPORTED
  cl_assert_equal_i(s_imaging_request_count, 1);
  cl_assert_equal_i(s_imaging_request_token, 1);
  cl_assert_equal_i(s_imaging_request_format, ImagingFormat4BitPalette);
  const int16_t side = PBL_IF_RECT_ELSE(DISP_COLS - ACTION_BAR_WIDTH, DISP_COLS);
  cl_assert_equal_i(s_imaging_request_width, side);
  cl_assert_equal_i(s_imaging_request_height, side);
  cl_assert_equal_s(s_imaging_request_title, "Crumbling Castle");
  cl_assert_equal_s(s_imaging_request_artist, "King Gizzard & The Lizard Wizard");
#else
  cl_assert_equal_i(s_imaging_request_count, 0);
#endif
}

void test_music__album_art_not_requested_when_pref_off(void) {
  prv_set_now_playing("Crumbling Castle", "King Gizzard & The Lizard Wizard");
  s_music_play_state = MusicPlayStatePlaying;
  s_music_now_playing_generation = 1;
  s_imaging_supported = true;
  s_prefs_music_show_album_art = false;

  prv_launch_app_and_render();
  cl_assert_equal_i(s_imaging_request_count, 0);
}

void test_music__playing_album_art(void) {
#if MUSIC_ALBUM_ART_SUPPORTED
  prv_set_now_playing("Crumbling Castle", "King Gizzard & The Lizard Wizard");
  s_music_play_state = MusicPlayStatePlaying;
  s_music_track_pos_ms = 75 * 1000;
  s_music_track_length_ms = 644 * 1000;
  s_music_progress_supported = true;
  s_music_now_playing_generation = 1;
  s_imaging_supported = true;
  s_prefs_music_show_album_art = true;
  prv_receive_album_art();

  prv_launch_app_and_render();
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
#endif
}

// Art that arrives after launch (the normal fetch flow) must produce the same screen as art that
// was already cached at launch.
void test_music__album_art_shown_when_received(void) {
#if MUSIC_ALBUM_ART_SUPPORTED
  prv_set_now_playing("Crumbling Castle", "King Gizzard & The Lizard Wizard");
  s_music_play_state = MusicPlayStatePlaying;
  s_music_track_pos_ms = 75 * 1000;
  s_music_track_length_ms = 644 * 1000;
  s_music_progress_supported = true;
  s_music_now_playing_generation = 1;
  s_imaging_supported = true;
  s_prefs_music_show_album_art = true;

  prv_launch_app_and_render();
  cl_assert_equal_i(s_imaging_request_count, 1);

  prv_receive_album_art();
  PebbleEvent event = {
      .type = PEBBLE_MEDIA_EVENT,
      .media = {.type = PebbleMediaEventTypeAlbumArtUpdated},
  };
  prv_music_event_handler(&event, NULL);

  prv_render();
  cl_check(
      gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_NAMED_PBI_FILE("test_music__playing_album_art")));
#endif
}

// Toggling the pref off from the phone while the app is open must land exactly on the stock
// no-art screen — the test_music__playing baseline, hence that test's now-playing metadata.
void test_music__album_art_pref_toggled_off(void) {
#if MUSIC_ALBUM_ART_SUPPORTED
  prv_set_now_playing("Just Like Heaven", "The Cure");
  s_music_play_state = MusicPlayStatePlaying;
  s_music_track_pos_ms = 75 * 1000;
  s_music_track_length_ms = 245 * 1000;
  s_music_progress_supported = true;
  s_music_now_playing_generation = 1;
  s_imaging_supported = true;
  s_prefs_music_show_album_art = true;
  prv_receive_album_art();

  prv_launch_app_and_render();

  s_prefs_music_show_album_art = false;
  PebbleEvent event = {
      .type = PEBBLE_PREF_CHANGE_EVENT,
      .pref_change =
          {
              .key = MUSIC_SHOW_ALBUM_ART_PREF_KEY,
              .key_len = sizeof(MUSIC_SHOW_ALBUM_ART_PREF_KEY),
          },
  };
  prv_pref_change_handler(&event, NULL);

  prv_render();
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_NAMED_PBI_FILE("test_music__playing")));
#endif
}

void test_music__output_route_rows_and_selection(void) {
  s_music_output_route_status = MusicOutputRouteStatusAvailable;
  s_music_output_route_count = 2;
  s_music_output_routes[0] = (MusicOutputRoute){
      .id = 2,
      .generation = 7,
      .selected = true,
      .name = "Phone",
  };
  s_music_output_routes[1] = (MusicOutputRoute){
      .id = 5,
      .generation = 7,
      .name = "Nest Mini",
  };

  MusicOutputWindow output_window = {};
  cl_assert(prv_output_update_snapshot(&output_window));
  cl_assert_equal_i(prv_output_get_num_rows(NULL, 0, &output_window), 2);

  MenuIndex selected_index = {.row = 0};
  prv_output_draw_row(NULL, NULL, &selected_index, &output_window);
  cl_assert_equal_s(s_menu_cell_title, "Phone");
  cl_assert_equal_s(s_menu_cell_subtitle, "Current");

  s_music_output_routes[1].generation = 8;
  s_music_output_routes[1].id = 6;
  cl_assert(!prv_output_update_snapshot(&output_window));

  MenuIndex nest_index = {.row = 1};
  prv_output_select(NULL, &nest_index, &output_window);
  cl_assert_equal_i(s_selected_output_route_generation, 7);
  cl_assert_equal_i(s_selected_output_route_id, 5);
}

void test_music__output_route_snapshot_resets_when_unavailable(void) {
  s_music_output_route_status = MusicOutputRouteStatusAvailable;
  s_music_output_route_count = 1;
  s_music_output_routes[0] = (MusicOutputRoute){
      .id = 2,
      .generation = 7,
      .name = "Phone",
  };

  MusicOutputWindow output_window = {};
  cl_assert(prv_output_update_snapshot(&output_window));

  s_music_output_route_status = MusicOutputRouteStatusUnsupported;
  s_music_output_route_count = 0;
  cl_assert(prv_output_update_snapshot(&output_window));
  cl_assert_equal_i(output_window.status, MusicOutputRouteStatusUnsupported);
  cl_assert_equal_i(output_window.route_count, 0);
  cl_assert(!output_window.has_available_snapshot);
  cl_assert_equal_i(prv_output_get_num_rows(NULL, 0, &output_window), 1);
}

void test_music__output_route_snapshot_resets_on_event(void) {
  s_music_output_route_status = MusicOutputRouteStatusAvailable;
  s_music_output_route_count = 1;
  s_music_output_routes[0] = (MusicOutputRoute){
      .id = 2,
      .generation = 7,
      .name = "Phone",
  };

  MusicOutputWindow output_window = {};
  MusicAppData data = {
      .output_window = &output_window,
  };
  app_state_set_user_data(&data);
  cl_assert(prv_output_update_snapshot(&output_window));

  s_music_output_route_status = MusicOutputRouteStatusUnsupported;
  s_music_output_route_count = 0;
  PebbleEvent event = {
      .type = PEBBLE_MEDIA_EVENT,
      .media = {.type = PebbleMediaEventTypeOutputRoutesChanged},
  };
  prv_music_event_handler(&event, NULL);

  cl_assert_equal_i(output_window.status, MusicOutputRouteStatusUnsupported);
  cl_assert_equal_i(output_window.route_count, 0);
  cl_assert(!output_window.has_available_snapshot);
  cl_assert_equal_i(s_menu_reload_count, 1);
  app_state_set_user_data(NULL);
}

void test_music__output_route_status_row_retries(void) {
  s_music_output_route_status = MusicOutputRouteStatusPermissionRequired;

  MusicOutputWindow output_window = {};
  cl_assert(prv_output_update_snapshot(&output_window));
  cl_assert_equal_i(prv_output_get_num_rows(NULL, 0, &output_window), 1);

  MenuIndex index = {};
  prv_output_draw_row(NULL, NULL, &index, &output_window);
  cl_assert_equal_s(s_menu_cell_title, "Permission required");
  cl_assert_equal_s(s_menu_cell_subtitle, "Enable in Pebble app");

  prv_output_select(NULL, &index, &output_window);
  cl_assert_equal_i(s_music_output_route_request_count, 1);
  cl_assert_equal_i(output_window.status, MusicOutputRouteStatusLoading);
}

void test_music__output_picker_requires_phone_capability(void) {
  MusicAppData data = {};
  prv_push_output_window(&data);

  cl_assert(data.output_window == NULL);
  cl_assert_equal_i(s_music_output_route_request_count, 0);
}

void test_music__output_picker_lifecycle(void) {
  s_music_output_routing_supported = true;
  MusicAppData data = {};
  prv_push_output_window(&data);

  cl_assert(data.output_window != NULL);
  cl_assert_equal_i(s_music_output_route_request_count, 1);

  prv_output_window_unload(&data.output_window->window);
  cl_assert(data.output_window == NULL);
}

void test_music__output_picker_long_press_modes(void) {
  cl_assert_equal_i(prv_select_long_press_action(false), SelectLongPressActionNone);
  cl_assert_equal_i(prv_select_long_press_action(true), SelectLongPressActionPlayPause);

  s_music_output_routing_supported = true;
  cl_assert_equal_i(prv_select_long_press_action(false), SelectLongPressActionOutput);
  cl_assert_equal_i(prv_select_long_press_action(true), SelectLongPressActionOutput);
}
