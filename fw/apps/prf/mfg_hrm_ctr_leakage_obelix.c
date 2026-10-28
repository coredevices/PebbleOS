/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_hrm_ctr_leakage_obelix.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/drivers/ppg.h>
#include <pbl/drivers/rtc.h>
#include <pbl/logging/logging.h>
#include <pbl/services/hrm/hrm_manager.h>
#include <pbl/services/hrm/hrm_ppg.h>
#include <pbl/util/math.h>
#include <pbl/util/units.h>

#include <applib/app.h>
#include <applib/ui/app_window_stack.h>
#include <applib/ui/text_layer.h>
#include <applib/ui/window.h>
#include <apps/prf/mfg_test_result.h>
#include <kernel/pbl_malloc.h>
#include <process_management/pebble_process_md.h>
#include <process_state/app_state/app_state.h>
#include <syscall/syscall.h>

#define STATUS_STRING_LEN  32
#define CTR_STRING_LEN     128
#define LEAKAGE_STRING_LEN 128
#define RESULT_DISPLAY_MS  1000
#define POLL_MS            100

#define TEST_RATE_HZ      25U
#define TEST_DROP_PERIODS 30U
#define TEST_PERIODS      80U
#define TEST_LED_UA       20000U
#define CTR_GAIN_OHM      10000U
#define LEAKAGE_GAIN_OHM  100000U
#define TEST_RX_MASK      0x3U
#define TEST_NUM_RX       2U

/* Results are photocurrent per LED current, in nA/mA, scaled by 100. */
#define RESULT_SCALE 100
#define PA_PER_NA    1000
#define UA_PER_MA    1000

enum {
  CH_GREEN,
  CH_IR,
  CH_RED,
  CH_NUM_LEDS,
};

#define NUM_CHANNELS (CH_NUM_LEDS * TEST_NUM_RX)

/* Indexed green rx0/rx1, IR rx0/rx1, red rx0/rx1. */
static const int32_t s_ctr_min[NUM_CHANNELS] = {42300, 44100, 33900, 33600, 56400, 59700};
static const int32_t s_leakage_max[NUM_CHANNELS] = {187, 240, 1020, 980, 700, 960};

static const enum pbl_ppg_led_color s_colors[CH_NUM_LEDS] = {
  [CH_GREEN] = PBL_PPG_LED_GREEN,
  [CH_IR] = PBL_PPG_LED_IR,
  [CH_RED] = PBL_PPG_LED_RED,
};

typedef enum {
  TestMode_None,
  TestMode_CTR,
  TestMode_Leakage,
  TestMode_Algo_HR,
  TestMode_Algo_SpO2,
} HRMTestMode;

static struct {
  uint8_t slot[CH_NUM_LEDS];
  uint8_t led[CH_NUM_LEDS];
  uint32_t periods;
  int64_t sum[NUM_CHANNELS];
  bool done;
} s_capture;

typedef struct {
  Window window;
  EventServiceInfo hrm_event_info;

  TextLayer title_text_layer;
  TextLayer status_text_layer;
  TextLayer ctr_text_layer;
  TextLayer leak_text_layer;
  char status_string[STATUS_STRING_LEN];
  char ctr_string[CTR_STRING_LEN];
  char leak_string[LEAKAGE_STRING_LEN];
  HRMSessionRef hrm_session;
  HRMTestMode test_mode;
  uint32_t gain_ohm;
  bool capture_pending;
  const char *capture_status;

  bool ctr_received;
  bool leak_received;
  bool ctr_passed;
  bool leak_passed;
  RtcTicks result_start_time;
} AppData;

static void prv_samples(const struct pbl_ppg *ppg, const struct pbl_ppg_batch *batch,
                        void *user_data) {
  for (uint16_t i = 0U; i < batch->num_samples; i++) {
    const struct pbl_ppg_sample *sample = &batch->samples[i];

    if (s_capture.done || (sample->rx >= TEST_NUM_RX)) {
      continue;
    }

    for (uint8_t ch = 0U; ch < CH_NUM_LEDS; ch++) {
      if (sample->slot != s_capture.slot[ch]) {
        continue;
      }

      if ((ch == CH_GREEN) && (sample->rx == 0U)) {
        s_capture.periods++;
      }

      if ((s_capture.periods > TEST_DROP_PERIODS) &&
          (s_capture.periods <= (TEST_DROP_PERIODS + TEST_PERIODS))) {
        s_capture.sum[ch * TEST_NUM_RX + sample->rx] += sample->value;
      }
    }

    if (s_capture.periods > (TEST_DROP_PERIODS + TEST_PERIODS)) {
      s_capture.done = true;
    }
  }
}

static const struct pbl_ppg_listener s_listener = {.samples = prv_samples};

static int prv_find_slot(const struct pbl_ppg_info *info, uint8_t ch) {
  for (uint8_t slot = 0U; slot < info->num_slots; slot++) {
    const struct pbl_ppg_slot *s = &info->slots[slot];

    if ((s->rx_mask & TEST_RX_MASK) != TEST_RX_MASK) {
      continue;
    }

    for (uint8_t led = 0U; led < PBL_PPG_MAX_LED_DRIVERS; led++) {
      if ((s->led_mask & (1U << led)) && (s->leds[led] == s_colors[ch])) {
        s_capture.slot[ch] = slot;
        s_capture.led[ch] = led;
        return 0;
      }
    }
  }

  return -ENODEV;
}

static int prv_setup_channel(uint8_t ch, uint32_t gain_ohm) {
  int ret;

  ret = pbl_ppg_set_led_current(PPG, s_capture.slot[ch], s_capture.led[ch], TEST_LED_UA);
  if (ret != 0) {
    return ret;
  }

  for (uint8_t rx = 0U; rx < TEST_NUM_RX; rx++) {
    ret = pbl_ppg_set_gain(PPG, s_capture.slot[ch], rx, gain_ohm);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

static int prv_start_capture(uint32_t gain_ohm) {
  const struct pbl_ppg_info *info = pbl_ppg_get_info(PPG);
  struct pbl_ppg_config config = {.sample_rate_hz = TEST_RATE_HZ, .batch = TEST_RATE_HZ};
  int ret;

  if (info == nullptr) {
    return -ENODEV;
  }

  ret = pbl_ppg_stop(PPG);
  if (ret != 0) {
    return ret;
  }

  pbl_ppg_lock(PPG);
  memset(&s_capture, 0, sizeof(s_capture));
  pbl_ppg_unlock(PPG);

  for (uint8_t ch = 0U; ch < CH_NUM_LEDS; ch++) {
    ret = prv_find_slot(info, ch);
    if (ret != 0) {
      return ret;
    }

    ret = prv_setup_channel(ch, gain_ohm);
    if (ret != 0) {
      return ret;
    }

    config.rx_mask[s_capture.slot[ch]] = TEST_RX_MASK;
  }

  ret = pbl_ppg_configure(PPG, &config);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_ppg_set_listener(PPG, &s_listener);
  if (ret != 0) {
    return ret;
  }

  return pbl_ppg_start(PPG);
}

static void prv_stop_capture(void) {
  (void)pbl_ppg_stop(PPG);
  (void)pbl_ppg_set_listener(PPG, nullptr);
}

static bool prv_take_results(uint32_t gain_ohm, int32_t *results) {
  const struct pbl_ppg_info *info = pbl_ppg_get_info(PPG);
  const struct pbl_ppg_channel channel = {.calibrated_gain_ohm = gain_ohm};
  bool done;

  pbl_ppg_lock(PPG);
  done = s_capture.done;
  if (done) {
    for (uint8_t i = 0U; i < NUM_CHANNELS; i++) {
      int32_t mean = (int32_t)(s_capture.sum[i] / (int64_t)TEST_PERIODS);
      int64_t pa = pbl_ppg_value_to_pa(info, &channel, mean);

      results[i] = (int32_t)((pa * RESULT_SCALE * UA_PER_MA) / (PA_PER_NA * TEST_LED_UA));
    }
  }
  pbl_ppg_unlock(PPG);

  return done;
}

static void prv_format(char *buf, size_t len, const char *name, bool passed,
                       const int32_t *results) {
  int32_t whole[NUM_CHANNELS];
  int32_t frac[NUM_CHANNELS];

  for (uint8_t i = 0U; i < NUM_CHANNELS; i++) {
    whole[i] = results[i] / RESULT_SCALE;
    frac[i] = ABS(results[i] % RESULT_SCALE);
  }

  snprintf(buf, len,
           "%s:(%s)\n%4" PRId32 ".%02" PRId32 " %4" PRId32 ".%02" PRId32 " %4" PRId32 ".%02" PRId32
           "\n%4" PRId32 ".%02" PRId32 " %4" PRId32 ".%02" PRId32 " %4" PRId32 ".%02" PRId32,
           name, passed ? "PASS" : "FAILED", whole[0], frac[0], whole[2], frac[2], whole[4],
           frac[4], whole[1], frac[1], whole[3], frac[3], whole[5], frac[5]);
}

static bool prv_ctr_passed(const int32_t *results) {
  for (uint8_t i = 0U; i < NUM_CHANNELS; i++) {
    if (results[i] < s_ctr_min[i]) {
      return false;
    }
  }

  return true;
}

static bool prv_leakage_passed(const int32_t *results) {
  for (uint8_t i = 0U; i < NUM_CHANNELS; i++) {
    if (results[i] > s_leakage_max[i]) {
      return false;
    }
  }

  return true;
}

static void prv_check_capture(AppData *app_data) {
  int32_t results[NUM_CHANNELS];

  if (!prv_take_results(app_data->gain_ohm, results)) {
    return;
  }

  prv_stop_capture();

  if (app_data->test_mode == TestMode_CTR) {
    app_data->ctr_received = true;
    app_data->ctr_passed = prv_ctr_passed(results);
    prv_format(app_data->ctr_string, CTR_STRING_LEN, "CTR", app_data->ctr_passed, results);
    PBL_LOG_DBG("%s", app_data->ctr_string);
  } else {
    app_data->leak_received = true;
    app_data->leak_passed = prv_leakage_passed(results);
    prv_format(app_data->leak_string, LEAKAGE_STRING_LEN, "Leak", app_data->leak_passed, results);
    PBL_LOG_DBG("%s", app_data->leak_string);
  }

  app_data->test_mode = TestMode_None;

  if (app_data->ctr_received && app_data->leak_received && (app_data->result_start_time == 0)) {
    bool passed = app_data->ctr_passed && app_data->leak_passed;

    mfg_test_result_report(MfgTestId_HrmCtrLeakage, passed, 0);
    snprintf(app_data->status_string, STATUS_STRING_LEN, passed ? "PASS" : "FAIL");
    app_data->result_start_time = rtc_get_ticks();
  }
}

static void prv_begin_capture(AppData *app_data) {
  int ret;

  app_data->capture_pending = false;

  ret = prv_start_capture(app_data->gain_ohm);
  if (ret != 0) {
    prv_stop_capture();
    app_data->test_mode = TestMode_None;
    snprintf(app_data->status_string, STATUS_STRING_LEN, "Sensor error %d", ret);
    return;
  }

  snprintf(app_data->status_string, STATUS_STRING_LEN, "%s", app_data->capture_status);
}

static void prv_poll_timer_callback(void *cb_data) {
  AppData *data = app_state_get_user_data();

  if (data->capture_pending && !hrm_ppg_is_running()) {
    prv_begin_capture(data);
  } else if (!data->capture_pending &&
             ((data->test_mode == TestMode_CTR) || (data->test_mode == TestMode_Leakage))) {
    prv_check_capture(data);
  }

  if (data->result_start_time != 0) {
    uint32_t elapsed = (uint32_t)(rtc_get_ticks() - data->result_start_time);
    if (elapsed >= RESULT_DISPLAY_MS) {
      app_window_stack_pop(false);
      return;
    }
  }

  layer_mark_dirty(&data->window.layer);
  app_timer_register(POLL_MS, prv_poll_timer_callback, nullptr);
}

static void prv_handle_hrm_data(PebbleEvent *e, void *context) {
  AppData *app_data = app_state_get_user_data();

  if (e->type != PEBBLE_HRM_EVENT) {
    return;
  }

  if (e->hrm.event_type == HRMEvent_BPM) {
    snprintf(app_data->ctr_string, CTR_STRING_LEN, "HR:%d Q:%d", e->hrm.bpm.bpm,
             e->hrm.bpm.quality);
    PBL_LOG_DBG("%s", app_data->ctr_string);
  } else if (e->hrm.event_type == HRMEvent_SpO2) {
    snprintf(app_data->leak_string, LEAKAGE_STRING_LEN, "SPO2:%d Q:%d", e->hrm.spo2.percent,
             e->hrm.spo2.quality);
    PBL_LOG_DBG("%s", app_data->leak_string);
  }

  layer_mark_dirty(&app_data->window.layer);
}

static void prv_stop_algo(AppData *app_data) {
  if (app_data->hrm_session == HRM_INVALID_SESSION_REF) {
    return;
  }

  event_service_client_unsubscribe(&app_data->hrm_event_info);
  sys_hrm_manager_unsubscribe(app_data->hrm_session);
  app_data->hrm_session = HRM_INVALID_SESSION_REF;
}

static void prv_start_test(AppData *app_data, HRMTestMode mode, uint32_t gain_ohm,
                           const char *status) {
  prv_stop_algo(app_data);

  app_data->test_mode = mode;
  app_data->gain_ohm = gain_ohm;
  app_data->capture_status = status;
  app_data->capture_pending = true;
  snprintf(app_data->status_string, STATUS_STRING_LEN, "Starting...");
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();

  if (!app_data->ctr_received) {
    prv_start_test(app_data, TestMode_CTR, CTR_GAIN_OHM, "CTR Sampling...");
  } else {
    prv_start_test(app_data, TestMode_Leakage, LEAKAGE_GAIN_OHM, "Leak Sampling...");
  }

  layer_mark_dirty(&app_data->window.layer);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();
  HRMFeature features;

  app_data->capture_pending = false;
  prv_stop_capture();
  prv_stop_algo(app_data);

  if (app_data->test_mode != TestMode_Algo_HR) {
    snprintf(app_data->status_string, STATUS_STRING_LEN, "HR Sampling...");
    app_data->test_mode = TestMode_Algo_HR;
    features = HRMFeature_BPM;
  } else {
    snprintf(app_data->status_string, STATUS_STRING_LEN, "SPO2 Sampling...");
    app_data->test_mode = TestMode_Algo_SpO2;
    features = HRMFeature_SpO2;
  }

  snprintf(app_data->ctr_string, CTR_STRING_LEN, "HR: Q:");
  snprintf(app_data->leak_string, LEAKAGE_STRING_LEN, "SPO2: Q:");

  event_service_client_subscribe(&app_data->hrm_event_info);
  app_data->hrm_session = sys_hrm_manager_app_subscribe(1, 1, PBL_SEC_PER_HOUR, features);

  layer_mark_dirty(&app_data->window.layer);
}

static void prv_config_provider(void *data) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

static void prv_handle_init(void) {
  AppData *data = task_zalloc(sizeof(*data));
  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);
  window_set_click_config_provider(window, prv_config_provider);

  TextLayer *title = &data->title_text_layer;
  text_layer_init(title, &window->layer.bounds);
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "HRM TEST");
  layer_add_child(&window->layer, &title->layer);

  sniprintf(data->status_string, STATUS_STRING_LEN, "Press Sel to Start");

  snprintf(data->ctr_string, CTR_STRING_LEN, "CTR:--");
  snprintf(data->leak_string, LEAKAGE_STRING_LEN, "Leak:--");

  TextLayer *status = &data->status_text_layer;
  text_layer_init(status,
                  &GRect(5, 30, window->layer.bounds.size.w - 5, window->layer.bounds.size.h - 30));
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(status, GTextAlignmentCenter);
  text_layer_set_text(status, data->status_string);
  layer_add_child(&window->layer, &status->layer);

  TextLayer *leak = &data->leak_text_layer;
  text_layer_init(
      leak, &GRect(5, 60, window->layer.bounds.size.w - 5, window->layer.bounds.size.h - 140));
  text_layer_set_font(leak, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(leak, GTextAlignmentCenter);
  text_layer_set_text(leak, data->leak_string);
  layer_add_child(&window->layer, &leak->layer);

  TextLayer *ctr = &data->ctr_text_layer;
  text_layer_init(
      ctr, &GRect(5, 140, window->layer.bounds.size.w - 5, window->layer.bounds.size.h - 60));
  text_layer_set_font(ctr, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(ctr, GTextAlignmentCenter);
  text_layer_set_text(ctr, data->ctr_string);
  layer_add_child(&window->layer, &ctr->layer);

  data->hrm_event_info = (EventServiceInfo){
    .type = PEBBLE_HRM_EVENT,
    .handler = prv_handle_hrm_data,
  };
  data->hrm_session = HRM_INVALID_SESSION_REF;

  app_timer_register(POLL_MS, prv_poll_timer_callback, nullptr);
  app_window_stack_push(window, true);
}

static void prv_handle_deinit(void) {
  AppData *data = app_state_get_user_data();

  prv_stop_capture();
  prv_stop_algo(data);

  text_layer_deinit(&data->title_text_layer);
  text_layer_deinit(&data->status_text_layer);
  window_deinit(&data->window);
  app_free(data);
}

static void prv_main(void) {
  prv_handle_init();
  app_event_loop();
  prv_handle_deinit();
}

const PebbleProcessMd *mfg_hrm_ctr_leakage_obelix_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &prv_main,
    .name = "MfgHRMCTRLeakageObelix",
  };
  return (const PebbleProcessMd *)&s_app_info;
}
