/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Calls from the watch over the Zephyr hands-free link: a number pad, the phone's contacts (by
// initial) and call history (by day), favorites, and redial. A call shows on the phone call
// screen, as an incoming one does.

#include "call_indicator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/bluetooth/hci_router.h>
#include <pbl/services/settings/settings_file.h>
#include <pbl/services/time.h>

#include <applib/app.h>
#include <applib/app_timer.h>
#include <applib/fonts/fonts.h>
#include <applib/touch_service.h>
#include <applib/ui/app_window_stack.h>
#include <applib/ui/ui.h>
#include <applib/ui/vibes.h>
#include <kernel/pbl_malloc.h>
#include <process_management/pebble_process_md.h>
#include <process_state/app_state/app_state.h>
#include <resource/resource_ids.auto.h>
#include <system/status_codes.h>
#include <zbt/zbt.h>

#define FAV_FILE      "dialer_favorites"
#define FAV_FILE_SIZE 1024
#define FAV_MAX       10
// Phone book rows read at a time.
#define CACHE_ENTRIES 8

static const char FAV_KEY[] = "FAVS";

// The pad: 1-9, *, 0, #, then delete and call on the last row.
#define PAD_COLS   3
#define PAD_ROWS   5
#define PAD_DIGITS 12
#define KEY_DELETE 12
#define KEY_CALL   13
#define KEY_COUNT  14
#define NUMBER_H   44
// A list row: a name on a line (a long one scrolls on the selected row), and a line under it.
#define LIST_CELL_H 52
// The selected row's long name: it scrolls MARQUEE_STEP px a tick, after MARQUEE_PAUSE ticks
// at each end.
#define MARQUEE_TICK_MS 50
#define MARQUEE_STEP    2
#define MARQUEE_PAUSE   20
#define PICKS_MAX       40

static const char PAD_CHARS[PAD_DIGITS] = {'1', '2', '3', '4', '5', '6',
                                           '7', '8', '9', '*', '0', '#'};

enum {
  MAIN_ROW_KEYPAD,
  MAIN_ROW_CONTACTS,
  MAIN_ROW_HISTORY,
  MAIN_ROW_FAVORITES,
  MAIN_ROW_REDIAL,
  MAIN_ROW_SYNC,
  MAIN_ROW_COUNT,
};

enum {
  LIST_CONTACTS,
  LIST_HISTORY,
  LIST_FAVORITES,
};

typedef struct {
  Window main_window;
  MenuLayer main_menu;
  AppTimer *main_timer;
  GBitmap main_icons[MAIN_ROW_COUNT];

  Window pad_window;
  Layer pad_layer;
  char number[ZBT_CLIP_MAX];
  int selected;
  // The key under the finger, -1 for none.
  int touched;
  const char *pad_note;

  // Contacts and history open on an index (initials, days) first.
  Window pick_window;
  MenuLayer pick_menu;
  struct {
    uint8_t group;
    uint8_t month;
    uint8_t day;
    uint16_t year;
    uint16_t first;
    uint16_t count;
  } picks[PICKS_MAX];
  int pick_count;
  //! The book's (or the watch log's) rows, in the index's order; app heap.
  uint16_t *order;
  int order_count;

  Window list_window;
  MenuLayer list_menu;
  int list_kind;
  //! The rows the list shows: order[list_first] on, list_count of them.
  int list_first;
  int list_count;
  //! The selected row's name scrolling: how far, how far it can, and the pause left.
  AppTimer *marquee_timer;
  int16_t marquee_offset;
  int16_t marquee_max;
  int16_t marquee_pause;
  struct zbt_call_record log[ZBT_CALL_LOG_MAX];
  int log_count;
  int book_count[ZBT_BOOK_COUNT];
  struct zbt_book_entry cache[CACHE_ENTRIES];
  int cache_book;
  int cache_start;
  int cache_count;

  char favorites[FAV_MAX][ZBT_CLIP_MAX];
  int fav_count;
} DialerData;

static DialerData *prv_data(void) {
  return app_state_get_user_data();
}

/* Favorites, in a settings file ------------------------------------------------------------*/

static void prv_favorites_load(DialerData *data) {
  data->fav_count = 0;
  SettingsFile file;
  if (settings_file_open(&file, FAV_FILE, FAV_FILE_SIZE) != S_SUCCESS) {
    return;
  }
  const int len = settings_file_get_len(&file, FAV_KEY, sizeof(FAV_KEY));
  if (len > 0 && len % ZBT_CLIP_MAX == 0 && len <= (int)sizeof(data->favorites) &&
      settings_file_get(&file, FAV_KEY, sizeof(FAV_KEY), data->favorites, len) == S_SUCCESS) {
    data->fav_count = len / ZBT_CLIP_MAX;
  }
  settings_file_close(&file);
}

static void prv_favorites_save(DialerData *data) {
  SettingsFile file;
  if (settings_file_open(&file, FAV_FILE, FAV_FILE_SIZE) != S_SUCCESS) {
    return;
  }
  if (data->fav_count > 0) {
    (void)settings_file_set(&file, FAV_KEY, sizeof(FAV_KEY), data->favorites,
                            data->fav_count * ZBT_CLIP_MAX);
  } else {
    (void)settings_file_delete(&file, FAV_KEY, sizeof(FAV_KEY));
  }
  settings_file_close(&file);
}

//! Adds @p number in front (moving it there if already in); false if empty or full.
static bool prv_favorites_add(DialerData *data, const char *number) {
  if (number[0] == '\0') {
    return false;
  }
  int at = data->fav_count;
  for (int i = 0; i < data->fav_count; i++) {
    if (strcmp(data->favorites[i], number) == 0) {
      at = i;
      break;
    }
  }
  if (at == FAV_MAX) {
    return false;
  }
  if (at == data->fav_count) {
    data->fav_count++;
  }
  memmove(data->favorites[1], data->favorites[0], at * ZBT_CLIP_MAX);
  strncpy(data->favorites[0], number, ZBT_CLIP_MAX - 1);
  data->favorites[0][ZBT_CLIP_MAX - 1] = '\0';
  prv_favorites_save(data);
  return true;
}

static void prv_favorites_remove(DialerData *data, int index) {
  if (index < 0 || index >= data->fav_count) {
    return;
  }
  memmove(data->favorites[index], data->favorites[index + 1],
          (data->fav_count - index - 1) * ZBT_CLIP_MAX);
  data->fav_count--;
  prv_favorites_save(data);
}

/* Calling ---------------------------------------------------------------------------------*/

// A call starts on its screen at once, and is dialed DIAL_DELAY_MS on: Back or the X before
// then takes it back, nothing sent to the phone.
#define DIAL_DELAY_MS 2000

//! Calls @p number (NULL: redial), the call then on the phone call screen; vibrates when the
//! host refuses.
static void prv_call(const char *number) {
  const int err = number != NULL ? zbt_hfp_dial_after(number, DIAL_DELAY_MS) : zbt_hfp_redial();
  if (err != 0) {
    vibes_double_pulse();
    return;
  }
  call_indicator_open_call_screen(number != NULL ? number : "");
}

/* The number pad ----------------------------------------------------------------------------*/

static GRect prv_key_rect(const GRect *bounds, int key) {
  const int16_t row_h = (bounds->size.h - NUMBER_H) / PAD_ROWS;
  const int16_t col_w = bounds->size.w / PAD_COLS;
  if (key == KEY_DELETE) {
    return GRect(0, NUMBER_H + 4 * row_h, col_w, row_h);
  }
  if (key == KEY_CALL) {
    return GRect(col_w, NUMBER_H + 4 * row_h, bounds->size.w - col_w, row_h);
  }
  return GRect((key % PAD_COLS) * col_w, NUMBER_H + (key / PAD_COLS) * row_h, col_w, row_h);
}

static int prv_key_at(const GRect *bounds, GPoint point) {
  for (int key = 0; key < KEY_COUNT; key++) {
    const GRect r = prv_key_rect(bounds, key);
    if (grect_contains_point(&r, &point)) {
      return key;
    }
  }
  return -1;
}

static void prv_pad_draw(Layer *layer, GContext *ctx) {
  DialerData *data = prv_data();
  const GRect bounds = layer->bounds;

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, &bounds);

  const char *top = data->pad_note ? data->pad_note : data->number;
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, top, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     GRect(4, 4, bounds.size.w - 8, NUMBER_H - 4), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);

  for (int key = 0; key < KEY_COUNT; key++) {
    const GRect r = grect_inset(prv_key_rect(&bounds, key), GEdgeInsets(2));
    const bool lit = key == data->selected || key == data->touched;
    GColor bg = key == KEY_CALL ? GColorIslamicGreen : GColorLightGray;
    if (lit) {
      bg = key == KEY_CALL ? GColorJaegerGreen : GColorCobaltBlue;
    }
    graphics_context_set_fill_color(ctx, bg);
    graphics_fill_round_rect(ctx, &r, 6, GCornersAll);

    char label[2] = {key < PAD_DIGITS ? PAD_CHARS[key] : '\0', '\0'};
    const char *text = key == KEY_DELETE ? "Del" : key == KEY_CALL ? "Call" : label;
    graphics_context_set_text_color(ctx, lit || key == KEY_CALL ? GColorWhite : GColorBlack);
    GRect tr = r;
    tr.origin.y += (r.size.h - 30) / 2;
    tr.size.h = 30;
    graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), tr,
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

static void prv_pad_press(DialerData *data, int key) {
  data->pad_note = NULL;
  const size_t len = strlen(data->number);
  if (key == KEY_CALL) {
    if (len > 0) {
      prv_call(data->number);
    }
  } else if (key == KEY_DELETE) {
    if (len > 0) {
      data->number[len - 1] = '\0';
    }
  } else if (key >= 0 && key < PAD_DIGITS && len < sizeof(data->number) - 1) {
    data->number[len] = PAD_CHARS[key];
    data->number[len + 1] = '\0';
  }
  layer_mark_dirty(&data->pad_layer);
}

static void prv_pad_up(ClickRecognizerRef recognizer, void *context) {
  DialerData *data = prv_data();
  data->selected = (data->selected + KEY_COUNT - 1) % KEY_COUNT;
  layer_mark_dirty(&data->pad_layer);
}

static void prv_pad_down(ClickRecognizerRef recognizer, void *context) {
  DialerData *data = prv_data();
  data->selected = (data->selected + 1) % KEY_COUNT;
  layer_mark_dirty(&data->pad_layer);
}

static void prv_pad_select(ClickRecognizerRef recognizer, void *context) {
  DialerData *data = prv_data();
  prv_pad_press(data, data->selected);
}

static void prv_pad_select_long(ClickRecognizerRef recognizer, void *context) {
  DialerData *data = prv_data();
  prv_pad_press(data, KEY_CALL);
}

static void prv_pad_down_long(ClickRecognizerRef recognizer, void *context) {
  DialerData *data = prv_data();
  if (prv_favorites_add(data, data->number)) {
    data->pad_note = "Saved";
    vibes_short_pulse();
  }
  layer_mark_dirty(&data->pad_layer);
}

static void prv_pad_up_long(ClickRecognizerRef recognizer, void *context) {
  prv_pad_press(prv_data(), KEY_DELETE);
}

static void prv_pad_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_pad_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, prv_pad_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_pad_select);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, prv_pad_select_long, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 0, prv_pad_down_long, NULL);
  // Back leaves (the number stays for next time); a long Up, or Del, deletes a digit.
  window_long_click_subscribe(BUTTON_ID_UP, 0, prv_pad_up_long, NULL);
}

#ifdef CONFIG_TOUCH
static void prv_pad_touch(const TouchEvent *event, void *context) {
  DialerData *data = context;
  if (event->non_navigational) {
    return;
  }
  const GRect bounds = data->pad_layer.bounds;
  const int key = prv_key_at(&bounds, GPoint(event->x, event->y));
  switch (event->type) {
    case TouchEvent_Touchdown:
      data->touched = key;
      break;
    case TouchEvent_PositionUpdate:
      // Sliding off the key cancels it.
      if (key != data->touched) {
        data->touched = -1;
      }
      break;
    case TouchEvent_Liftoff:
      if (data->touched >= 0 && key == data->touched) {
        data->selected = key;
        prv_pad_press(data, key);
      }
      data->touched = -1;
      break;
  }
  layer_mark_dirty(&data->pad_layer);
}
#endif

static void prv_pad_window_load(Window *window) {
  DialerData *data = prv_data();
  Layer *root = window_get_root_layer(window);
  layer_init(&data->pad_layer, &root->bounds);
  layer_set_update_proc(&data->pad_layer, prv_pad_draw);
  layer_add_child(root, &data->pad_layer);
  data->touched = -1;
}

static void prv_pad_window_unload(Window *window) {
  DialerData *data = prv_data();
  layer_deinit(&data->pad_layer);
}

static void prv_pad_window_appear(Window *window) {
#ifdef CONFIG_TOUCH
  touch_service_subscribe(prv_pad_touch, prv_data());
#endif
}

static void prv_pad_window_disappear(Window *window) {
#ifdef CONFIG_TOUCH
  touch_service_unsubscribe();
#endif
  prv_data()->touched = -1;
}

/* Contacts, history and favorites -----------------------------------------------------------*/

// A row of a phone book, read through a small cache; NULL past its end.
static const struct zbt_book_entry *prv_book_entry(DialerData *data, enum zbt_book book, int row) {
  if (data->cache_book != (int)book || row < data->cache_start ||
      row >= data->cache_start + data->cache_count) {
    data->cache_book = book;
    data->cache_start = row > CACHE_ENTRIES / 2 ? row - CACHE_ENTRIES / 2 : 0;
    data->cache_count = zbt_book_read(book, data->cache_start, data->cache, CACHE_ENTRIES);
  }
  const int i = row - data->cache_start;
  return i >= 0 && i < data->cache_count ? &data->cache[i] : NULL;
}

// History: the phone's once read, else the calls the watch logged.
static bool prv_phone_history(DialerData *data) {
  return data->book_count[ZBT_BOOK_HISTORY] > 0;
}

static int prv_list_count(DialerData *data) {
  if (data->list_kind == LIST_FAVORITES) {
    return data->book_count[ZBT_BOOK_FAVORITES] + data->fav_count;
  }
  return data->list_count;
}

// The book index (or the watch log's) of a list row.
static int prv_list_index(DialerData *data, int row) {
  if (data->list_kind == LIST_FAVORITES) {
    return row;
  }
  const int i = data->list_first + row;
  return i < data->order_count ? data->order[i] : -1;
}

//! The row's number and name ("" when none); false past the end.
static bool prv_list_row(DialerData *data, int row, const char **number, const char **name) {
  *name = "";
  const int index = prv_list_index(data, row);
  if (index < 0) {
    return false;
  }
  if (data->list_kind == LIST_HISTORY && !prv_phone_history(data)) {
    if (index >= data->log_count) {
      return false;
    }
    *number = data->log[index].number;
    return true;
  }
  if (data->list_kind == LIST_FAVORITES && index >= data->book_count[ZBT_BOOK_FAVORITES]) {
    const int fav = index - data->book_count[ZBT_BOOK_FAVORITES];
    if (fav >= data->fav_count) {
      return false;
    }
    *number = data->favorites[fav];
    return true;
  }
  const enum zbt_book book = data->list_kind == LIST_CONTACTS  ? ZBT_BOOK_CONTACTS
                             : data->list_kind == LIST_HISTORY ? ZBT_BOOK_HISTORY
                                                               : ZBT_BOOK_FAVORITES;
  const struct zbt_book_entry *entry = prv_book_entry(data, book, index);
  if (entry == NULL) {
    return false;
  }
  *number = entry->number;
  *name = entry->name;
  return true;
}

static uint16_t prv_list_rows(MenuLayer *menu, uint16_t section, void *context) {
  DialerData *data = context;
  const int n = prv_list_count(data);
  // An empty list still shows a row saying so.
  return n > 0 ? n : 1;
}

static int16_t prv_list_cell_height(MenuLayer *menu, MenuIndex *index, void *context) {
  return LIST_CELL_H;
}

// A name on a line, and a line under it. On the selected row, a name too long for the line
// scrolls (marquee_offset); elsewhere it ends in an ellipsis.
static void prv_draw_cell(GContext *ctx, const Layer *cell, const char *title, const char *sub,
                          bool selected) {
  DialerData *data = prv_data();
  const GRect b = cell->bounds;
  const GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  const int16_t width = b.size.w - 12;
  if (selected) {
    const GSize size = app_graphics_text_layout_get_content_size(
        title, font, GRect(0, 0, 2000, 30), GTextOverflowModeFill, GTextAlignmentLeft);
    data->marquee_max = size.w > width ? size.w - width : 0;
  }
  if (selected && data->marquee_max > 0) {
    graphics_draw_text(ctx, title, font, GRect(6 - data->marquee_offset, -2, width + 2000, 30),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  } else {
    graphics_draw_text(ctx, title, font, GRect(6, -2, width, 30), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, NULL);
  }
  if (sub != NULL && sub[0] != '\0') {
    graphics_draw_text(ctx, sub, fonts_get_system_font(FONT_KEY_GOTHIC_18), GRect(6, 26, width, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void prv_marquee_tick(void *context) {
  DialerData *data = prv_data();
  data->marquee_timer = app_timer_register(MARQUEE_TICK_MS, prv_marquee_tick, NULL);
  if (data->marquee_max <= 0) {
    return;
  }
  if (data->marquee_pause > 0) {
    data->marquee_pause--;
    if (data->marquee_pause == 0 && data->marquee_offset >= data->marquee_max) {
      // Paused at the end: back to the start, to pause there.
      data->marquee_offset = 0;
      data->marquee_pause = MARQUEE_PAUSE;
    } else {
      return;
    }
  } else {
    data->marquee_offset += MARQUEE_STEP;
    if (data->marquee_offset >= data->marquee_max) {
      data->marquee_offset = data->marquee_max;
      data->marquee_pause = MARQUEE_PAUSE;
    }
  }
  layer_mark_dirty(menu_layer_get_layer(&data->list_menu));
}

static void prv_list_selection_changed(MenuLayer *menu, MenuIndex new_index, MenuIndex old_index,
                                       void *context) {
  DialerData *data = context;
  data->marquee_offset = 0;
  data->marquee_max = 0;
  data->marquee_pause = MARQUEE_PAUSE;
}

static void prv_list_draw(GContext *ctx, const Layer *cell, MenuIndex *index, void *context) {
  DialerData *data = context;
  static const char *const KINDS[] = {"In", "Out", "Missed"};
  if (prv_list_count(data) == 0) {
    static const char *const EMPTY[] = {"No contacts", "No calls", "No favorites"};
    prv_draw_cell(ctx, cell, EMPTY[data->list_kind],
                  data->list_kind == LIST_HISTORY ? NULL : "Sync, or hold Down on the pad", false);
    return;
  }
  const char *number;
  const char *name;
  if (!prv_list_row(data, index->row, &number, &name)) {
    return;
  }
  const char *title = name[0] != '\0' ? name : number[0] != '\0' ? number : "Unknown";
  char sub[40];
  if (data->list_kind != LIST_HISTORY) {
    const bool on_watch =
        data->list_kind == LIST_FAVORITES && index->row >= data->book_count[ZBT_BOOK_FAVORITES];
    snprintf(sub, sizeof(sub), "%s%s", name[0] != '\0' ? number : "", on_watch ? " (watch)" : "");
  } else if (prv_phone_history(data)) {
    const struct zbt_book_entry *e =
        prv_book_entry(data, ZBT_BOOK_HISTORY, prv_list_index(data, index->row));
    if (e != NULL && e->year != 0) {
      snprintf(sub, sizeof(sub), "%s %02u:%02u  %s", e->kind < 3 ? KINDS[e->kind] : "?", e->hour,
               e->minute, name[0] != '\0' ? number : "");
    } else {
      snprintf(sub, sizeof(sub), "%s", e != NULL && e->kind < 3 ? KINDS[e->kind] : "?");
    }
  } else {
    const struct zbt_call_record *rec = &data->log[prv_list_index(data, index->row)];
    struct pbl_tm tm;
    const time_t t = rec->time;
    pbl_localtime_r(&t, &tm);
    snprintf(sub, sizeof(sub), "%s %02d:%02d  %u:%02u", rec->kind < 3 ? KINDS[rec->kind] : "?",
             tm.tm_hour, tm.tm_min, rec->duration_s / 60, rec->duration_s % 60);
  }
  prv_draw_cell(ctx, cell, title, sub,
                menu_layer_get_selected_index(&data->list_menu).row == index->row);
}

static void prv_list_select(MenuLayer *menu, MenuIndex *index, void *context) {
  DialerData *data = context;
  const char *number;
  const char *name;
  if (prv_list_row(data, index->row, &number, &name) && number[0] != '\0') {
    char copy[ZBT_CLIP_MAX];
    strncpy(copy, number, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';
    prv_call(copy);
  }
}

static void prv_list_select_long(MenuLayer *menu, MenuIndex *index, void *context) {
  DialerData *data = context;
  const int phone_favorites = data->book_count[ZBT_BOOK_FAVORITES];
  if (data->list_kind == LIST_FAVORITES) {
    // Only the watch's own; the phone's change on the phone.
    if (index->row >= phone_favorites) {
      prv_favorites_remove(data, index->row - phone_favorites);
      menu_layer_reload_data(menu);
    }
    return;
  }
  const char *number;
  const char *name;
  if (prv_list_row(data, index->row, &number, &name)) {
    char copy[ZBT_CLIP_MAX];
    strncpy(copy, number, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';
    if (prv_favorites_add(data, copy)) {
      vibes_short_pulse();
    }
  }
}

static void prv_list_window_load(Window *window) {
  DialerData *data = prv_data();
  Layer *root = window_get_root_layer(window);
  MenuLayer *menu = &data->list_menu;
  menu_layer_init(menu, &root->bounds);
  menu_layer_set_callbacks(menu, data,
                           &(MenuLayerCallbacks){
                             .get_num_rows = prv_list_rows,
                             .get_cell_height = prv_list_cell_height,
                             .draw_row = prv_list_draw,
                             .select_click = prv_list_select,
                             .select_long_click = prv_list_select_long,
                             .selection_changed = prv_list_selection_changed,
                           });
  menu_layer_set_click_config_onto_window(menu, window);
  layer_add_child(root, menu_layer_get_layer(menu));
}

static void prv_list_window_unload(Window *window) {
  DialerData *data = prv_data();
  if (data->marquee_timer) {
    app_timer_cancel(data->marquee_timer);
    data->marquee_timer = NULL;
  }
  menu_layer_deinit(&data->list_menu);
}

static void prv_books_reload(DialerData *data) {
  for (int book = 0; book < ZBT_BOOK_COUNT; book++) {
    data->book_count[book] = zbt_book_count((enum zbt_book)book);
  }
  data->cache_book = -1;
}

static void prv_list_window_appear(Window *window) {
  DialerData *data = prv_data();
  data->cache_book = -1;
  data->marquee_offset = 0;
  data->marquee_pause = MARQUEE_PAUSE;
  if (data->marquee_timer == NULL) {
    data->marquee_timer = app_timer_register(MARQUEE_TICK_MS, prv_marquee_tick, NULL);
  }
  menu_layer_reload_data(&data->list_menu);
}

/* The index: contacts by initial, history by day ---------------------------------------------*/

#define READ_CHUNK 16

struct contact_key {
  uint16_t index;
  uint8_t group;
  //! The reading (or the name), the start of it, for the order within a group.
  char key[9];
};

static int prv_compare_keys(const void *a, const void *b) {
  const struct contact_key *x = a;
  const struct contact_key *y = b;
  if (x->group != y->group) {
    return x->group - y->group;
  }
  const int c = strcmp(x->key, y->key);
  return c != 0 ? c : x->index - y->index;
}

static void prv_order_free(DialerData *data) {
  app_free(data->order);
  data->order = NULL;
  data->order_count = 0;
  data->pick_count = 0;
}

// Contacts sorted by group, then reading; one pick a group.
static void prv_index_contacts(DialerData *data) {
  const int n = data->book_count[ZBT_BOOK_CONTACTS];
  struct contact_key *keys = n > 0 ? app_malloc(n * sizeof(*keys)) : NULL;
  struct zbt_book_entry *chunk = app_malloc(READ_CHUNK * sizeof(*chunk));
  data->order = n > 0 ? app_malloc(n * sizeof(*data->order)) : NULL;
  if (keys == NULL || chunk == NULL || data->order == NULL) {
    app_free(keys);
    app_free(chunk);
    prv_order_free(data);
    return;
  }
  int got = 0;
  while (got < n) {
    const int read = zbt_book_read(ZBT_BOOK_CONTACTS, got, chunk, READ_CHUNK);
    if (read <= 0) {
      break;
    }
    for (int i = 0; i < read && got < n; i++, got++) {
      const char *k = chunk[i].sort[0] != '\0' ? chunk[i].sort : chunk[i].name;
      keys[got] = (struct contact_key){.index = (uint16_t)got, .group = chunk[i].group};
      strncpy(keys[got].key, k, sizeof(keys[got].key) - 1);
      for (char *c = keys[got].key; *c != '\0'; c++) {
        if (*c >= 'a' && *c <= 'z') {
          *c = (char)(*c - 'a' + 'A');
        }
      }
    }
  }
  app_free(chunk);
  qsort(keys, got, sizeof(*keys), prv_compare_keys);
  data->order_count = got;
  data->pick_count = 0;
  for (int i = 0; i < got; i++) {
    data->order[i] = keys[i].index;
    if (data->pick_count == 0 || data->picks[data->pick_count - 1].group != keys[i].group) {
      if (data->pick_count == PICKS_MAX) {
        break;
      }
      data->picks[data->pick_count++] = (typeof(data->picks[0])){
        .group = keys[i].group,
        .first = (uint16_t)i,
      };
    }
    data->picks[data->pick_count - 1].count++;
  }
  app_free(keys);
}

// History (the phone's, else the watch's log), newest first, one pick a day.
static void prv_index_history(DialerData *data) {
  const bool phone = prv_phone_history(data);
  const int n = phone ? data->book_count[ZBT_BOOK_HISTORY] : data->log_count;
  data->order = n > 0 ? app_malloc(n * sizeof(*data->order)) : NULL;
  if (n > 0 && data->order == NULL) {
    return;
  }
  data->order_count = n;
  data->pick_count = 0;
  for (int i = 0; i < n; i++) {
    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    if (phone) {
      const struct zbt_book_entry *e = prv_book_entry(data, ZBT_BOOK_HISTORY, i);
      if (e != NULL) {
        year = e->year;
        month = e->month;
        day = e->day;
      }
    } else {
      struct pbl_tm tm;
      const time_t t = data->log[i].time;
      pbl_localtime_r(&t, &tm);
      year = (uint16_t)(tm.tm_year + 1900);
      month = (uint8_t)(tm.tm_mon + 1);
      day = (uint8_t)tm.tm_mday;
    }
    data->order[i] = (uint16_t)i;
    if (data->pick_count == 0 || data->picks[data->pick_count - 1].year != year ||
        data->picks[data->pick_count - 1].month != month ||
        data->picks[data->pick_count - 1].day != day) {
      if (data->pick_count == PICKS_MAX) {
        data->order_count = i;
        break;
      }
      data->picks[data->pick_count++] = (typeof(data->picks[0])){
        .year = year,
        .month = month,
        .day = day,
        .first = (uint16_t)i,
      };
    }
    data->picks[data->pick_count - 1].count++;
  }
}

static void prv_group_label(uint8_t group, char *out, size_t size) {
  static const char *const KANA[] = {"あ (a)",  "か (ka)", "さ (sa)", "た (ta)", "な (na)",
                                     "は (ha)", "ま (ma)", "や (ya)", "ら (ra)", "わ (wa)"};
  if (group <= ZBT_GROUP_Z) {
    snprintf(out, size, "%c", 'A' + group);
  } else if (group >= ZBT_GROUP_KANA_A && group <= ZBT_GROUP_KANA_WA) {
    snprintf(out, size, "%s", KANA[group - ZBT_GROUP_KANA_A]);
  } else {
    snprintf(out, size, "# / other");
  }
}

static uint16_t prv_pick_rows(MenuLayer *menu, uint16_t section, void *context) {
  DialerData *data = context;
  return data->pick_count > 0 ? data->pick_count : 1;
}

static void prv_pick_draw(GContext *ctx, const Layer *cell, MenuIndex *index, void *context) {
  DialerData *data = context;
  if (data->pick_count == 0) {
    menu_cell_basic_draw(ctx, cell, data->list_kind == LIST_CONTACTS ? "No contacts" : "No calls",
                         data->list_kind == LIST_CONTACTS ? "Sync with phone first" : NULL, NULL);
    return;
  }
  const typeof(data->picks[0]) *pick = &data->picks[index->row];
  char title[24];
  char sub[24];
  if (data->list_kind == LIST_CONTACTS) {
    prv_group_label(pick->group, title, sizeof(title));
    snprintf(sub, sizeof(sub), "%u contact%s", pick->count, pick->count == 1 ? "" : "s");
  } else {
    if (pick->year != 0) {
      static const char *const WDAY[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
      // Zeller's congruence, for the weekday.
      int y = pick->year;
      int m = pick->month;
      if (m < 3) {
        m += 12;
        y--;
      }
      const int w = (pick->day + 13 * (m + 1) / 5 + y + y / 4 - y / 100 + y / 400 + 6) % 7;
      snprintf(title, sizeof(title), "%u/%u %s", pick->month, pick->day, WDAY[w]);
    } else {
      snprintf(title, sizeof(title), "Unknown day");
    }
    snprintf(sub, sizeof(sub), "%u call%s", pick->count, pick->count == 1 ? "" : "s");
  }
  menu_cell_basic_draw(ctx, cell, title, sub, NULL);
}

static void prv_pick_select(MenuLayer *menu, MenuIndex *index, void *context) {
  DialerData *data = context;
  if (data->pick_count == 0) {
    return;
  }
  data->list_first = data->picks[index->row].first;
  data->list_count = data->picks[index->row].count;
  app_window_stack_push(&data->list_window, true);
}

static void prv_pick_window_load(Window *window) {
  DialerData *data = prv_data();
  Layer *root = window_get_root_layer(window);
  MenuLayer *menu = &data->pick_menu;
  menu_layer_init(menu, &root->bounds);
  menu_layer_set_callbacks(menu, data,
                           &(MenuLayerCallbacks){
                             .get_num_rows = prv_pick_rows,
                             .draw_row = prv_pick_draw,
                             .select_click = prv_pick_select,
                           });
  menu_layer_set_click_config_onto_window(menu, window);
  layer_add_child(root, menu_layer_get_layer(menu));

  prv_books_reload(data);
  data->log_count = zbt_call_log_read(data->log, ZBT_CALL_LOG_MAX);
  if (data->list_kind == LIST_CONTACTS) {
    prv_index_contacts(data);
  } else {
    prv_index_history(data);
  }
  menu_layer_reload_data(menu);
}

static void prv_pick_window_unload(Window *window) {
  DialerData *data = prv_data();
  menu_layer_deinit(&data->pick_menu);
  prv_order_free(data);
}

/* The main menu -----------------------------------------------------------------------------*/

static uint16_t prv_main_rows(MenuLayer *menu, uint16_t section, void *context) {
  return MAIN_ROW_COUNT;
}

static void prv_sync_text(const struct zbt_status *st, char *out, size_t size) {
  static const char *const BOOKS[] = {"contacts", "history", "favorites"};
  switch (st->pbap) {
    case ZBT_PBAP_SEARCHING:
      snprintf(out, size, "Looking up");
      break;
    case ZBT_PBAP_CONNECTING:
      snprintf(out, size, "Connecting");
      break;
    case ZBT_PBAP_PULLING:
      snprintf(out, size, "Reading %s", st->pbap_book < 3 ? BOOKS[st->pbap_book] : "?");
      break;
    case ZBT_PBAP_FAILED:
      snprintf(out, size, "Failed %d", st->pbap_err);
      break;
    case ZBT_PBAP_DONE:
    case ZBT_PBAP_IDLE:
    default: {
      // Per book (contacts/history/favorites): the entries kept, or "e" and why not.
      size_t len = 0;
      for (int book = 0; book < ZBT_BOOK_COUNT && len < size; book++) {
        const int result = st->pbap_result[book];
        len += snprintf(out + len, size - len, result == 0 ? "%s%u" : "%se%d", book == 0 ? "" : "/",
                        result == 0 ? st->pbap_entries[book] : result);
      }
      break;
    }
  }
}

static void prv_main_draw(GContext *ctx, const Layer *cell, MenuIndex *index, void *context) {
  DialerData *data = context;
  GBitmap *icon = index->row < MAIN_ROW_COUNT ? &data->main_icons[index->row] : NULL;
  struct zbt_status st;
  zbt_get_status(&st);
  const bool ready = st.state == ZBT_STATE_READY && st.hfp == ZBT_HFP_CONNECTED;
  char sub[40];
  switch (index->row) {
    case MAIN_ROW_KEYPAD:
      // No routing this boot (a routed boot did not come up): no calls until it is again.
      menu_cell_basic_draw(ctx, cell, "Keypad",
                           ready                     ? "Phone connected"
                           : !hci_router_is_active() ? "Off: Probe > Router next boot"
                                                     : "Not connected",
                           icon);
      break;
    case MAIN_ROW_CONTACTS:
      snprintf(sub, sizeof(sub), "%u", data->book_count[ZBT_BOOK_CONTACTS]);
      menu_cell_basic_draw(ctx, cell, "Contacts", sub, icon);
      break;
    case MAIN_ROW_HISTORY:
      menu_cell_basic_draw(ctx, cell, "History", NULL, icon);
      break;
    case MAIN_ROW_FAVORITES:
      menu_cell_basic_draw(ctx, cell, "Favorites", NULL, icon);
      break;
    case MAIN_ROW_REDIAL:
      menu_cell_basic_draw(ctx, cell, "Redial", "Last number again", icon);
      break;
    case MAIN_ROW_SYNC:
      prv_sync_text(&st, sub, sizeof(sub));
      menu_cell_basic_draw(ctx, cell, "Sync with phone", sub, icon);
      break;
  }
}

static void prv_open_redial(DialerData *data);

static void prv_main_select(MenuLayer *menu, MenuIndex *index, void *context) {
  DialerData *data = context;
  switch (index->row) {
    case MAIN_ROW_KEYPAD:
      data->pad_note = NULL;
      app_window_stack_push(&data->pad_window, true);
      break;
    case MAIN_ROW_CONTACTS:
    case MAIN_ROW_HISTORY:
      data->list_kind = index->row == MAIN_ROW_CONTACTS ? LIST_CONTACTS : LIST_HISTORY;
      app_window_stack_push(&data->pick_window, true);
      break;
    case MAIN_ROW_FAVORITES:
      data->list_kind = LIST_FAVORITES;
      prv_books_reload(data);
      app_window_stack_push(&data->list_window, true);
      break;
    case MAIN_ROW_REDIAL:
      prv_open_redial(data);
      break;
    case MAIN_ROW_SYNC:
      if (zbt_pbap_sync(ZBT_BOOK_ALL) != 0) {
        vibes_double_pulse();
      }
      menu_layer_reload_data(menu);
      break;
  }
}

// The last number called: the phone's history's, else the watch's log's, else none.
static bool prv_last_dialed(DialerData *data, char *out, size_t size) {
  const int n = zbt_book_count(ZBT_BOOK_HISTORY);
  for (int i = 0; i < n; i++) {
    struct zbt_book_entry e;
    if (zbt_book_read(ZBT_BOOK_HISTORY, i, &e, 1) == 1 && e.kind == ZBT_CALL_KIND_OUTGOING &&
        e.number[0] != '\0') {
      strncpy(out, e.number, size - 1);
      out[size - 1] = '\0';
      return true;
    }
  }
  data->log_count = zbt_call_log_read(data->log, ZBT_CALL_LOG_MAX);
  for (int i = 0; i < data->log_count; i++) {
    if (data->log[i].kind == ZBT_CALL_KIND_OUTGOING && data->log[i].number[0] != '\0') {
      strncpy(out, data->log[i].number, size - 1);
      out[size - 1] = '\0';
      return true;
    }
  }
  return false;
}

// Redial: the pad with the last number and Call selected, to call with Select.
static void prv_open_redial(DialerData *data) {
  if (!prv_last_dialed(data, data->number, sizeof(data->number))) {
    data->number[0] = '\0';
    data->pad_note = "No last number";
  } else {
    data->pad_note = NULL;
  }
  data->selected = KEY_CALL;
  app_window_stack_push(&data->pad_window, true);
}

// While the main menu shows: the connection and the sync's progress.
static void prv_main_refresh(void *context) {
  DialerData *data = prv_data();
  menu_layer_reload_data(&data->main_menu);
  data->main_timer = app_timer_register(1000, prv_main_refresh, NULL);
}

static void prv_main_window_load(Window *window) {
  DialerData *data = prv_data();
  Layer *root = window_get_root_layer(window);
  MenuLayer *menu = &data->main_menu;
  menu_layer_init(menu, &root->bounds);
  menu_layer_set_callbacks(menu, data,
                           &(MenuLayerCallbacks){
                             .get_num_rows = prv_main_rows,
                             .draw_row = prv_main_draw,
                             .select_click = prv_main_select,
                           });
  menu_layer_set_click_config_onto_window(menu, window);
  layer_add_child(root, menu_layer_get_layer(menu));
}

static void prv_main_window_unload(Window *window) {
  menu_layer_deinit(&prv_data()->main_menu);
}

static void prv_main_window_appear(Window *window) {
  DialerData *data = prv_data();
  prv_books_reload(data);
  menu_layer_reload_data(&data->main_menu);
  data->main_timer = app_timer_register(1000, prv_main_refresh, NULL);
}

static void prv_main_window_disappear(Window *window) {
  DialerData *data = prv_data();
  if (data->main_timer) {
    app_timer_cancel(data->main_timer);
    data->main_timer = NULL;
  }
}

/* App ---------------------------------------------------------------------------------------*/

static void prv_init(void) {
  DialerData *data = app_zalloc_check(sizeof(*data));
  app_state_set_user_data(data);
  prv_favorites_load(data);
  // Menus and lists by touch too, whatever the touch navigation setting.
  app_touch_navigation_enable(true);
  static const uint32_t ICONS[MAIN_ROW_COUNT] = {
    [MAIN_ROW_KEYPAD] = RESOURCE_ID_DIALER_ICON_KEYPAD,
    [MAIN_ROW_CONTACTS] = RESOURCE_ID_DIALER_ICON_CONTACTS,
    [MAIN_ROW_HISTORY] = RESOURCE_ID_DIALER_ICON_HISTORY,
    [MAIN_ROW_FAVORITES] = RESOURCE_ID_DIALER_ICON_FAVORITES,
    [MAIN_ROW_REDIAL] = RESOURCE_ID_DIALER_ICON_REDIAL,
    [MAIN_ROW_SYNC] = RESOURCE_ID_DIALER_ICON_SYNC,
  };
  for (int i = 0; i < MAIN_ROW_COUNT; i++) {
    gbitmap_init_with_resource_system(&data->main_icons[i], SYSTEM_APP, ICONS[i]);
  }

  window_init(&data->main_window, "Dialer");
  window_set_window_handlers(&data->main_window, &(WindowHandlers){
                                                   .load = prv_main_window_load,
                                                   .unload = prv_main_window_unload,
                                                   .appear = prv_main_window_appear,
                                                   .disappear = prv_main_window_disappear,
                                                 });

  window_init(&data->pad_window, "Dialer pad");
  window_set_click_config_provider(&data->pad_window, prv_pad_click_config);
  window_set_window_handlers(&data->pad_window, &(WindowHandlers){
                                                  .load = prv_pad_window_load,
                                                  .unload = prv_pad_window_unload,
                                                  .appear = prv_pad_window_appear,
                                                  .disappear = prv_pad_window_disappear,
                                                });

  window_init(&data->list_window, "Dialer list");
  window_set_window_handlers(&data->list_window, &(WindowHandlers){
                                                   .load = prv_list_window_load,
                                                   .unload = prv_list_window_unload,
                                                   .appear = prv_list_window_appear,
                                                 });

  window_init(&data->pick_window, "Dialer index");
  window_set_window_handlers(&data->pick_window, &(WindowHandlers){
                                                   .load = prv_pick_window_load,
                                                   .unload = prv_pick_window_unload,
                                                 });

  app_window_stack_push(&data->main_window, true);
}

static void prv_main(void) {
  prv_init();
  app_event_loop();
  for (int i = 0; i < MAIN_ROW_COUNT; i++) {
    gbitmap_deinit(&prv_data()->main_icons[i]);
  }
  app_free(prv_data());
}

const PebbleProcessMd *dialer_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = prv_main,
    .name = "Dialer",
    .icon_resource_id = RESOURCE_ID_DIALER_MENU_ICON,
  };
  return (const PebbleProcessMd *)&s_app_info;
}
