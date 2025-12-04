/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_mic_getafix_app.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "util/math.h"
#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window.h"
#include "kernel/pbl_malloc.h"
#include "system/logging.h"
#include "board/board.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"
#include "drivers/mic.h"
#include "mfg/mfg_info.h"
#include "flash_region/flash_region.h"
#include "drivers/flash.h"

// Kiss FFT from Speex library - must include config and os_support before kiss_fft headers
// Save and undefine ABS to avoid redefinition warning with Speex's arch.h
#ifdef ABS
#undef ABS
#endif
#include "config.h"
#include "os_support_custom.h"
#include "kiss_fftr.h"

#define SAMPLE_RATE_HZ           16000
#define RECORDING_DURATION_MS    1000
#define SAMPLE_BITS              16
#define FFT_SIZE                 1024
#define MAX_CHANNELS             2
#define PCM_BUFFER_SIZE          1024

// Calculate total samples and flash requirements
#define N_SAMPLES                (MAX_CHANNELS * ((SAMPLE_RATE_HZ * RECORDING_DURATION_MS) / 1000))
#define SAMPLE_SIZE_BYTES        (SAMPLE_BITS / 8)
#define BLOCK_SIZE               (N_SAMPLES * SAMPLE_SIZE_BYTES)

#define FLASH_START              FLASH_REGION_FIRMWARE_DEST_BEGIN
#define FLASH_END                (FLASH_REGION_FIRMWARE_DEST_BEGIN + \
                                  ROUND_TO_MOD_CEIL(BLOCK_SIZE, SUBSECTOR_SIZE_BYTES))

// Target frequency: 1 kHz ± 100 Hz
#define TARGET_FREQ_HZ           1000
#define FREQ_TOLERANCE_HZ        100

// Minimum peak magnitude threshold (to reject noise)
#define MIN_PEAK_MAGNITUDE       1000

#define STATUS_STR_LEN           128

typedef enum {
  TestState_Init,
  TestState_Recording,
  TestState_Analyzing,
  TestState_Complete,
  TestState_Failed
} TestState;

typedef struct {
  Window window;
  TextLayer title;
  TextLayer status;
  TextLayer mic1_result;
  TextLayer mic2_result;
  char status_text[STATUS_STR_LEN];
  char mic1_text[STATUS_STR_LEN];
  char mic2_text[STATUS_STR_LEN];

  int16_t pcm[PCM_BUFFER_SIZE];
  uint32_t flash_addr;

  TestState state;
  bool mic1_passed;
  bool mic2_passed;
  int mic1_peak_freq;
  int mic2_peak_freq;
} AppData;

// FFT analysis function
static int prv_find_peak_frequency(int16_t *samples, size_t sample_count) {
  if (sample_count < FFT_SIZE) {
    PBL_LOG(LOG_LEVEL_WARNING, "Not enough samples for FFT: %zu", sample_count);
    return -1;
  }

  // Allocate FFT configuration
  kiss_fftr_cfg fft_cfg = kiss_fftr_alloc(FFT_SIZE, 0, NULL, NULL);
  if (!fft_cfg) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to allocate FFT configuration");
    return -1;
  }

  // Allocate frequency domain buffer (FFT_SIZE/2 + 1 complex points)
  kiss_fft_cpx *freq_data = kernel_malloc_check((FFT_SIZE / 2 + 1) * sizeof(kiss_fft_cpx));

  // Perform real FFT
  kiss_fftr(fft_cfg, samples, freq_data);

  // Find peak magnitude and its bin index
  int peak_bin = 0;
  int32_t max_magnitude = 0;

  // Search from bin 1 to FFT_SIZE/2 (skip DC component at bin 0)
  for (int i = 1; i <= FFT_SIZE / 2; i++) {
    // Calculate magnitude squared (r^2 + i^2)
    int32_t real = freq_data[i].r;
    int32_t imag = freq_data[i].i;
    int32_t magnitude = (real * real) + (imag * imag);

    if (magnitude > max_magnitude) {
      max_magnitude = magnitude;
      peak_bin = i;
    }
  }

  // Convert bin to frequency
  // Frequency = (bin * sample_rate) / FFT_SIZE
  int peak_freq = (peak_bin * SAMPLE_RATE_HZ) / FFT_SIZE;

  PBL_LOG(LOG_LEVEL_INFO, "Peak found at bin %d, frequency %d Hz, magnitude %ld",
          peak_bin, peak_freq, (long)max_magnitude);

  // Clean up
  kernel_free(freq_data);
  kiss_fftr_free(fft_cfg);

  // Check if magnitude is above threshold
  if (max_magnitude < MIN_PEAK_MAGNITUDE) {
    PBL_LOG(LOG_LEVEL_WARNING, "Peak magnitude too low: %ld", (long)max_magnitude);
    return -1;
  }

  return peak_freq;
}

// Convert interleaved stereo (L/R/L/R) to non-interleaved (all L, then all R)
static void prv_interleaved_to_non_interleaved(int16_t *audio_data, size_t frame_count) {
  if (audio_data == NULL || frame_count == 0) {
    return;
  }

  for (size_t i = 1; i < frame_count; i++) {
    int16_t temp = audio_data[2 * i];
    for (size_t j = 2 * i; j > i; j--) {
      audio_data[j] = audio_data[j - 1];
    }
    audio_data[i] = temp;
  }
}

// Separate channels for dual microphone analysis from flash
static void prv_analyze_dual_mic(AppData *data) {
  uint32_t num_channels = mic_get_channels(MIC);

  // Allocate temporary buffer for FFT analysis
  int16_t *fft_buffer = kernel_malloc_check(FFT_SIZE * 2 * sizeof(int16_t));

  // Read first portion of data from flash for FFT analysis
  flash_read_bytes((uint8_t *)fft_buffer, FLASH_START, FFT_SIZE * 2 * sizeof(int16_t));

  if (num_channels == 2) {
    // Convert interleaved stereo to non-interleaved format
    // After conversion: first half = MIC1, second half = MIC2
    prv_interleaved_to_non_interleaved(fft_buffer, FFT_SIZE);

    // Analyze each microphone from sequential blocks
    data->mic1_peak_freq = prv_find_peak_frequency(fft_buffer, FFT_SIZE);
    data->mic2_peak_freq = prv_find_peak_frequency(&fft_buffer[FFT_SIZE], FFT_SIZE);
  } else {
    // Single microphone - analyze full buffer
    data->mic1_peak_freq = prv_find_peak_frequency(fft_buffer, FFT_SIZE);
    data->mic2_peak_freq = -1;  // No second mic
  }

  kernel_free(fft_buffer);

  // Check if peaks are around target frequency
  data->mic1_passed = (data->mic1_peak_freq > 0) &&
                      (abs(data->mic1_peak_freq - TARGET_FREQ_HZ) <= FREQ_TOLERANCE_HZ);

  if (num_channels == 2) {
    data->mic2_passed = (data->mic2_peak_freq > 0) &&
                        (abs(data->mic2_peak_freq - TARGET_FREQ_HZ) <= FREQ_TOLERANCE_HZ);
  } else {
    data->mic2_passed = true;  // N/A for single mic
  }

  // Update status text
  if (data->mic1_peak_freq > 0) {
    snprintf(data->mic1_text, STATUS_STR_LEN, "Mic 1: %d Hz %s",
             data->mic1_peak_freq, data->mic1_passed ? "PASS" : "FAIL");
  } else {
    snprintf(data->mic1_text, STATUS_STR_LEN, "Mic 1: No signal");
  }

  if (num_channels == 2) {
    if (data->mic2_peak_freq > 0) {
      snprintf(data->mic2_text, STATUS_STR_LEN, "Mic 2: %d Hz %s",
               data->mic2_peak_freq, data->mic2_passed ? "PASS" : "FAIL");
    } else {
      snprintf(data->mic2_text, STATUS_STR_LEN, "Mic 2: No signal");
    }
  } else {
    snprintf(data->mic2_text, STATUS_STR_LEN, "Mic 2: N/A");
  }
}

// Microphone data callback - writes to flash
static void prv_mic_data_handler(int16_t *samples, size_t sample_count, void *context) {
  AppData *data = app_state_get_user_data();

  if (data->state != TestState_Recording) {
    return;
  }

  // Write samples to flash
  if (data->flash_addr - FLASH_START + (sample_count * sizeof(int16_t)) > BLOCK_SIZE) {
    // Recording complete
    mic_stop(MIC);

    data->state = TestState_Analyzing;
    snprintf(data->status_text, STATUS_STR_LEN, "Analyzing...");

    // Perform FFT analysis from flash
    prv_analyze_dual_mic(data);

    data->state = TestState_Complete;
    snprintf(data->status_text, STATUS_STR_LEN, "Test Complete");

    return;
  }

  // Write to flash
  flash_write_bytes((uint8_t *)samples, data->flash_addr, sample_count * sizeof(int16_t));
  data->flash_addr += sample_count * sizeof(int16_t);

  // Update progress
  uint32_t bytes_written = data->flash_addr - FLASH_START;
  uint32_t progress = (bytes_written * 100) / BLOCK_SIZE;
  snprintf(data->status_text, STATUS_STR_LEN, "Recording... %lu%%", (unsigned long)progress);
}

// Start the test
static void prv_start_test(void) {
  AppData *data = app_state_get_user_data();

  data->state = TestState_Recording;
  data->flash_addr = FLASH_START;

  // Erase flash region for recording
  flash_region_erase_optimal_range(FLASH_START, FLASH_START, FLASH_END, FLASH_END);

  uint32_t num_channels = mic_get_channels(MIC);

  snprintf(data->status_text, STATUS_STR_LEN, "Recording...");

  PBL_LOG(LOG_LEVEL_INFO, "Starting microphone test (channels=%lu, block_size=%lu)",
          (unsigned long)num_channels, (unsigned long)BLOCK_SIZE);

  mic_init(MIC);
  mic_set_volume(MIC, 512);  // Mid-range volume

  if (!mic_start(MIC, prv_mic_data_handler, NULL, data->pcm, PCM_BUFFER_SIZE)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to start microphone");
    data->state = TestState_Failed;
    snprintf(data->status_text, STATUS_STR_LEN, "Mic start failed");
  }
}

// Button click handler to start the test
static void prv_select_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();
  if (app_data->state == TestState_Init) {
    prv_start_test();
  }
}

// Configure button click providers
static void prv_config_provider(void *data) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
}

// Timer callback to update UI
static void prv_timer_callback(void *cb_data) {
  AppData *data = app_state_get_user_data();

  // Mark layer dirty to trigger redraw
  layer_mark_dirty(window_get_root_layer(&data->window));

  // Re-register timer if test is still running or waiting to start
  if (data->state != TestState_Complete && data->state != TestState_Failed) {
    app_timer_register(100, prv_timer_callback, NULL);
  }
}

// Initialize the app
static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));
  memset(data, 0, sizeof(AppData));

  app_state_set_user_data(data);

  data->state = TestState_Init;

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);
  window_set_click_config_provider(window, prv_config_provider);

  GRect bounds = window->layer.bounds;

  // Title
  TextLayer *title = &data->title;
  GRect title_frame = bounds;
  title_frame.size.h = 30;
  text_layer_init(title, &title_frame);
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "Mic Input Test");
  layer_add_child(&window->layer, &title->layer);

  // Status text
  TextLayer *status = &data->status;
  GRect status_frame = bounds;
  status_frame.origin.y = 35;
  status_frame.size.h = 25;
  text_layer_init(status, &status_frame);
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(status, GTextAlignmentCenter);
  snprintf(data->status_text, STATUS_STR_LEN, "Press SEL to start");
  text_layer_set_text(status, data->status_text);
  layer_add_child(&window->layer, &status->layer);

  // Mic 1 result
  TextLayer *mic1 = &data->mic1_result;
  GRect mic1_frame = bounds;
  mic1_frame.origin.y = 65;
  mic1_frame.size.h = 25;
  text_layer_init(mic1, &mic1_frame);
  text_layer_set_font(mic1, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(mic1, GTextAlignmentCenter);
  snprintf(data->mic1_text, STATUS_STR_LEN, "Mic 1: Waiting...");
  text_layer_set_text(mic1, data->mic1_text);
  layer_add_child(&window->layer, &mic1->layer);

  // Mic 2 result
  TextLayer *mic2 = &data->mic2_result;
  GRect mic2_frame = bounds;
  mic2_frame.origin.y = 95;
  mic2_frame.size.h = 25;
  text_layer_init(mic2, &mic2_frame);
  text_layer_set_font(mic2, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(mic2, GTextAlignmentCenter);
  snprintf(data->mic2_text, STATUS_STR_LEN, "Mic 2: Waiting...");
  text_layer_set_text(mic2, data->mic2_text);
  layer_add_child(&window->layer, &mic2->layer);

  app_window_stack_push(window, true);

  // Start UI update timer
  app_timer_register(100, prv_timer_callback, NULL);
}

static void s_main(void) {
  prv_handle_init();

  app_event_loop();

  // Cleanup
  mic_stop(MIC);
}

const PebbleProcessMd *mfg_mic_getafix_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
      .common.main_func = &s_main,
      // UUID: 3e8f9a2c-1b4d-4f5e-9c6a-7d8e0f1a2b3c
      .common.uuid = {0x3e, 0x8f, 0x9a, 0x2c, 0x1b, 0x4d, 0x4f, 0x5e,
                      0x9c, 0x6a, 0x7d, 0x8e, 0x0f, 0x1a, 0x2b, 0x3c},
      .name = "MfgMicGetafix",
  };
  return (const PebbleProcessMd *)&s_app_info;
}
