/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "emoji_test.h"

#include <stdio.h>

#include "applib/app.h"
#include "applib/fonts/fonts.h"
#include "applib/graphics/utf8.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "pbl/util/math.h"
#include "pbl/util/size.h"
#include "process_state/app_state/app_state.h"

#define EMOJIS_PER_PAGE 48
#define EMOJI_BUFFER_SIZE (EMOJIS_PER_PAGE * 5 + 1)

typedef struct {
  Codepoint first;
  Codepoint last;
} CodepointRange;

static const CodepointRange s_ranges[] = {
    {0x2192, 0x2192},   {0x231A, 0x231B},   {0x2328, 0x2328},   {0x23CF, 0x23CF},
    {0x23E9, 0x23F3},   {0x23F8, 0x23FA},   {0x25AA, 0x25AB},   {0x25AF, 0x25AF},
    {0x25B6, 0x25B6},   {0x25BA, 0x25BA},   {0x25C0, 0x25C0},   {0x25FB, 0x25FE},
    {0x2600, 0x2605},   {0x260E, 0x260E},   {0x2611, 0x2611},   {0x2614, 0x2615},
    {0x2618, 0x2618},   {0x261D, 0x261D},   {0x2620, 0x2620},   {0x2622, 0x2623},
    {0x2626, 0x2626},   {0x262A, 0x262A},   {0x262E, 0x262F},   {0x2638, 0x263A},
    {0x2640, 0x2640},   {0x2642, 0x2642},   {0x2648, 0x2653},   {0x265F, 0x2660},
    {0x2663, 0x2663},   {0x2665, 0x2666},   {0x2668, 0x2668},   {0x267B, 0x267B},
    {0x267E, 0x267F},   {0x2692, 0x2697},   {0x2699, 0x2699},   {0x269B, 0x269C},
    {0x26A0, 0x26A1},   {0x26A7, 0x26A7},   {0x26AA, 0x26AB},   {0x26B0, 0x26B1},
    {0x26BD, 0x26BE},   {0x26C4, 0x26C5},   {0x26C8, 0x26C8},   {0x26CE, 0x26CF},
    {0x26D1, 0x26D1},   {0x26D3, 0x26D4},   {0x26E9, 0x26EA},   {0x26F0, 0x26F5},
    {0x26F7, 0x26FA},   {0x26FD, 0x26FD},   {0x2702, 0x2702},   {0x2705, 0x2705},
    {0x2708, 0x270D},   {0x270F, 0x270F},   {0x2712, 0x2712},   {0x2714, 0x2714},
    {0x2716, 0x2716},   {0x271D, 0x271D},   {0x2721, 0x2721},   {0x2728, 0x2728},
    {0x2733, 0x2734},   {0x2744, 0x2744},   {0x2747, 0x2747},   {0x274C, 0x274C},
    {0x274E, 0x274E},   {0x2753, 0x2755},   {0x2757, 0x2757},   {0x2763, 0x2764},
    {0x2795, 0x2797},   {0x27A1, 0x27A1},   {0x27B0, 0x27B0},   {0x27BF, 0x27BF},
    {0x2B05, 0x2B07},   {0x2B1B, 0x2B1C},   {0x2B50, 0x2B50},   {0x2B55, 0x2B55},
    {0x1F170, 0x1F171}, {0x1F17E, 0x1F17F}, {0x1F18E, 0x1F18E}, {0x1F191, 0x1F19A},
    {0x1F1E6, 0x1F1FF}, {0x1F201, 0x1F202}, {0x1F21A, 0x1F21A}, {0x1F22F, 0x1F22F},
    {0x1F232, 0x1F23A}, {0x1F250, 0x1F251}, {0x1F300, 0x1F321}, {0x1F324, 0x1F393},
    {0x1F396, 0x1F397}, {0x1F399, 0x1F39B}, {0x1F39E, 0x1F3F0}, {0x1F3F3, 0x1F3F5},
    {0x1F3F7, 0x1F4FD}, {0x1F4FF, 0x1F53D}, {0x1F549, 0x1F54E}, {0x1F550, 0x1F567},
    {0x1F56F, 0x1F570}, {0x1F573, 0x1F57A}, {0x1F587, 0x1F587}, {0x1F58A, 0x1F58D},
    {0x1F590, 0x1F590}, {0x1F595, 0x1F596}, {0x1F5A4, 0x1F5A5}, {0x1F5A8, 0x1F5A8},
    {0x1F5B1, 0x1F5B2}, {0x1F5BC, 0x1F5BC}, {0x1F5C2, 0x1F5C4}, {0x1F5D1, 0x1F5D3},
    {0x1F5DC, 0x1F5DE}, {0x1F5E1, 0x1F5E1}, {0x1F5E3, 0x1F5E3}, {0x1F5E8, 0x1F5E8},
    {0x1F5EF, 0x1F5EF}, {0x1F5F3, 0x1F5F3}, {0x1F5FA, 0x1F64F}, {0x1F680, 0x1F6C5},
    {0x1F6CB, 0x1F6D2}, {0x1F6D5, 0x1F6D7}, {0x1F6DC, 0x1F6E5}, {0x1F6E9, 0x1F6E9},
    {0x1F6EB, 0x1F6EC}, {0x1F6F0, 0x1F6F0}, {0x1F6F3, 0x1F6FC}, {0x1F7E0, 0x1F7EB},
    {0x1F7F0, 0x1F7F0}, {0x1F90C, 0x1F93A}, {0x1F93C, 0x1F945}, {0x1F947, 0x1F9FF},
    {0x1FA70, 0x1FA7C}, {0x1FA80, 0x1FA88}, {0x1FA90, 0x1FABD}, {0x1FABF, 0x1FAC5},
    {0x1FACE, 0x1FADB}, {0x1FAE0, 0x1FAE8}, {0x1FAF0, 0x1FAF8},
};

typedef struct {
  Window window;
  TextLayer header_layer;
  TextLayer emoji_layer;
  unsigned int page;
  unsigned int emoji_count;
  char header[24];
  char emojis[EMOJI_BUFFER_SIZE];
} AppState;

static Codepoint prv_codepoint_at(unsigned int index) {
  for (unsigned int i = 0; i < ARRAY_LENGTH(s_ranges); ++i) {
    const unsigned int count = s_ranges[i].last - s_ranges[i].first + 1;
    if (index < count) {
      return s_ranges[i].first + index;
    }
    index -= count;
  }
  return 0;
}

static void prv_refresh(AppState *data) {
  const unsigned int page_count = (data->emoji_count + EMOJIS_PER_PAGE - 1) / EMOJIS_PER_PAGE;
  sniprintf(data->header, sizeof(data->header), "Emoji %u/%u", data->page + 1, page_count);

  utf8_t *cursor = (utf8_t *)data->emojis;
  const unsigned int first = data->page * EMOJIS_PER_PAGE;
  const unsigned int last = MIN(first + EMOJIS_PER_PAGE, data->emoji_count);
  for (unsigned int i = first; i < last; ++i) {
    cursor += utf8_encode_codepoint(prv_codepoint_at(i), cursor);
    *cursor++ = ((i - first + 1) % 8) ? ' ' : '\n';
  }
  *cursor = '\0';
  layer_mark_dirty(&data->header_layer.layer);
  layer_mark_dirty(&data->emoji_layer.layer);
}

static void prv_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppState *data = context;
  const unsigned int page_count = (data->emoji_count + EMOJIS_PER_PAGE - 1) / EMOJIS_PER_PAGE;
  if (click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP) {
    data->page = data->page ? data->page - 1 : page_count - 1;
  } else {
    data->page = (data->page + 1) % page_count;
  }
  prv_refresh(data);
}

static void prv_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 150, prv_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, prv_click_handler);
}

static void prv_window_load(Window *window) {
  AppState *data = window_get_user_data(window);
  const GRect bounds = window->layer.bounds;

  text_layer_init(&data->header_layer, &GRect(0, 2, bounds.size.w, 18));
  text_layer_set_text_alignment(&data->header_layer, GTextAlignmentCenter);
  text_layer_set_font(&data->header_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text(&data->header_layer, data->header);
  layer_add_child(&window->layer, &data->header_layer.layer);

  text_layer_init(&data->emoji_layer, &GRect(0, 20, bounds.size.w, bounds.size.h - 20));
  text_layer_set_font(&data->emoji_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_EMOJI));
  text_layer_set_text_alignment(&data->emoji_layer, GTextAlignmentCenter);
  text_layer_set_text(&data->emoji_layer, data->emojis);
  layer_add_child(&window->layer, &data->emoji_layer.layer);
  prv_refresh(data);
}

static void prv_main(void) {
  AppState *data = app_zalloc_check(sizeof(*data));
  for (unsigned int i = 0; i < ARRAY_LENGTH(s_ranges); ++i) {
    data->emoji_count += s_ranges[i].last - s_ranges[i].first + 1;
  }
  app_state_set_user_data(data);

  window_init(&data->window, WINDOW_NAME("Emoji Test"));
  window_set_user_data(&data->window, data);
  window_set_background_color(&data->window, GColorWhite);
  window_set_click_config_provider_with_context(&data->window, prv_config_provider, data);
  window_set_window_handlers(&data->window, &(WindowHandlers){.load = prv_window_load});
  app_window_stack_push(&data->window, true);
  app_event_loop();
  app_free(data);
}

const PebbleProcessMd *emoji_test_app_get_info(void) {
  static const PebbleProcessMdSystem s_info = {
      .common.main_func = &prv_main,
      .name = "Emoji Test",
  };
  return (const PebbleProcessMd *)&s_info;
}
