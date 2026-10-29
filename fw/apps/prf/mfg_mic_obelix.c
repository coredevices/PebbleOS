/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_audio.h"

#include <stdio.h>

#include <pbl/drivers/flash.h>
#include <pbl/logging/logging.h>

#include <applib/app.h>
#include <applib/ui/app_window_stack.h>
#include <applib/ui/dialogs/confirmation_dialog.h>
#include <applib/ui/text_layer.h>
#include <applib/ui/window.h>
#include <applib/ui/window_private.h>
#include <apps/prf/mfg_test_result.h>
#include <board/board.h>
#include <flash_region/flash_region.h>
#include <kernel/pbl_malloc.h>
#include <process_management/pebble_process_md.h>
#include <process_state/app_state/app_state.h>

// Samples read back from flash at once
#define PCM_BUFFER_SIZE 1024

#define RECORDING_MS      5000
#define SAMPLE_RATE_HZ    16000
#define SAMPLE_BITS       16
#define CAPTURE_MS        100
#define N_CHANNELS        2
#define N_SAMPLES         (N_CHANNELS * ((SAMPLE_RATE_HZ * RECORDING_MS) / 1000))
#define SAMPLE_SIZE_BYTES (SAMPLE_BITS / 8)
#define BLOCK_SIZE        (N_SAMPLES * SAMPLE_SIZE_BYTES)

#define FLASH_START FLASH_REGION_FIRMWARE_DEST_BEGIN
#define FLASH_END \
  (FLASH_REGION_FIRMWARE_DEST_BEGIN + ROUND_TO_MOD_CEIL(BLOCK_SIZE, SUBSECTOR_SIZE_BYTES))

#define PROCESS_STATUS_STR_LEN 64

typedef struct {
  Window window;

  TextLayer title;
  TextLayer status;
  char status_text[PROCESS_STATUS_STR_LEN];
  uint32_t flash_addr;
  int16_t pcm[PCM_BUFFER_SIZE];
  bool in_testing;
  bool audio_playing;
  bool mic_recording;
  bool show_result_pending;
} AppData;

static size_t prv_audio_fill(int16_t *samples, size_t count) {
  AppData *app_data = app_state_get_user_data();
  static uint8_t s_mic = 0;
  const size_t bytes = count * N_CHANNELS * sizeof(int16_t);

  if (app_data->flash_addr - FLASH_START + bytes > BLOCK_SIZE) {
    app_data->flash_addr = FLASH_START;
    if (++s_mic >= N_CHANNELS) {
      mfg_audio_playback_stop();
      app_data->audio_playing = false;
      s_mic = 0;
      app_data->show_result_pending = true;
      return 0;
    }

    snprintf(app_data->status_text, PROCESS_STATUS_STR_LEN, "Playing MIC2");
  }

  flash_read_bytes((uint8_t *)app_data->pcm, app_data->flash_addr, bytes);
  app_data->flash_addr += bytes;
  for (size_t i = 0; i < count; i++) {
    samples[i] = app_data->pcm[N_CHANNELS * i + s_mic];
  }

  return count;
}

static void prv_start_playback(void) {
  AppData *app_data = app_state_get_user_data();

  app_data->audio_playing = mfg_audio_playback_start(0, prv_audio_fill) == 0;
}

static void prv_mic_data_handler(int16_t *samples, size_t sample_count) {
  AppData *app_data = app_state_get_user_data();

  if (app_data->flash_addr - FLASH_START > BLOCK_SIZE) {
    mfg_audio_capture_stop();
    app_data->mic_recording = false;
    app_data->flash_addr = FLASH_START;
    snprintf(app_data->status_text, PROCESS_STATUS_STR_LEN, "Playing MIC1");
    prv_start_playback();
    return;
  }
  flash_write_bytes((uint8_t *)samples, app_data->flash_addr, sample_count * sizeof(int16_t));
  app_data->flash_addr += sample_count * sizeof(int16_t);
}

static void prv_recording_start(void) {
  AppData *app_data = app_state_get_user_data();
  app_data->flash_addr = FLASH_START;
  flash_region_erase_optimal_range(FLASH_START, FLASH_START, FLASH_END, FLASH_END);

  // Disable back button during recording/playback
  window_set_overrides_back_button(&app_data->window, true);

  snprintf(app_data->status_text, PROCESS_STATUS_STR_LEN, "Pls Speak");
  // Highest gain, to make it more audible in testing
  app_data->mic_recording =
      mfg_audio_capture_start(N_CHANNELS, INT32_MAX, prv_mic_data_handler) == 0;
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();
  if (!app_data->in_testing) {
    app_data->in_testing = true;
    prv_recording_start();
  }
}

static void prv_config_provider(void *data) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
}

static void prv_result_confirmed(ClickRecognizerRef recognizer, void *context) {
  ConfirmationDialog *confirmation_dialog = (ConfirmationDialog *)context;
  confirmation_dialog_pop(confirmation_dialog);

  bool passed = (click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP);
  mfg_test_result_report(MfgTestId_Mic, passed, 0);
  app_window_stack_pop(false);
}

static void prv_result_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_result_confirmed);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_result_confirmed);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_result_confirmed);
}

static void prv_show_result_dialog(void) {
  ConfirmationDialog *confirmation_dialog = confirmation_dialog_create("Microphone Result");
  Dialog *dialog = confirmation_dialog_get_dialog(confirmation_dialog);

  dialog_set_text(dialog, "Microphone OK?");

  confirmation_dialog_set_click_config_provider(confirmation_dialog, prv_result_click_config);

  ActionBarLayer *action_bar = confirmation_dialog_get_action_bar(confirmation_dialog);
  action_bar_layer_set_context(action_bar, confirmation_dialog);

  app_confirmation_dialog_push(confirmation_dialog);
}

static void prv_timer_callback(void *cb_data) {
  AppData *data = app_state_get_user_data();

  layer_mark_dirty(window_get_root_layer(&data->window));

  if (data->show_result_pending) {
    data->show_result_pending = false;
    prv_show_result_dialog();
    return;
  }

  app_timer_register(500, prv_timer_callback, nullptr);
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);

  Window *window = &data->window;
  Layer *window_layer = window_get_root_layer(window);
  window_init(window, "");
  window_set_fullscreen(window, true);
  window_set_click_config_provider(window, prv_config_provider);

  TextLayer *title = &data->title;
  text_layer_init(title, &window->layer.bounds);
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "MICROPHONE TEST");
  layer_add_child(&window->layer, &title->layer);

  TextLayer *status = &data->status;
  GRect bounds = window_layer->bounds;
  bounds.origin.y += 80;
  text_layer_init(status, &window->layer.bounds);
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(status, GTextAlignmentCenter);
  snprintf(data->status_text, PROCESS_STATUS_STR_LEN, "Press Sel to start");
  text_layer_set_text(status, data->status_text);
  layer_set_frame((Layer *)status, &bounds);
  layer_add_child(&window->layer, &status->layer);

  app_window_stack_push(window, true /* Animated */);
  app_timer_register(500, prv_timer_callback, nullptr);
}

static void prv_handler_deinit(void) {
  AppData *data = app_state_get_user_data();
  if (data->mic_recording) {
    mfg_audio_capture_stop();
  }
  if (data->audio_playing) {
    mfg_audio_playback_stop();
  }
}

static void s_main(void) {
  prv_handle_init();
  app_event_loop();
  prv_handler_deinit();
}

const PebbleProcessMd *mfg_mic_obelix_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: 6b064482-dc20-45a9-804c-5002746b49f9
    .common.uuid =
        {0x6b, 0x06, 0x44, 0x82, 0xdc, 0x20, 0x45, 0xa9, 0x80, 0x4c, 0x50, 0x02, 0x74, 0x6b, 0x49,
         0xf9},
    .name = "MfgMicObelix",
  };
  return (const PebbleProcessMd *)&s_app_info;
}
