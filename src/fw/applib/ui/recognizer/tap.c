/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "tap.h"

#include "recognizer.h"
#include "recognizer_impl.h"

// TODO: Implement this correctly:
// https://pebbletechnology.atlassian.net/browse/PBL-28983

struct TapRecognizerData {
  // Recognizer config
  struct {
    uint16_t taps_required;
    uint16_t fingers_required;
    GPoint movement_threshold;
  } config;

  // Gesture state
  struct {
    uint16_t taps_detected;
    uint16_t fingers_down;
  } state;
};

static void prv_handle_touch_event(Recognizer *recognizer, const TouchEvent *touch_event);
static void prv_reset(Recognizer *recognizer);
static bool prv_cancel(Recognizer *recognizer);

static const RecognizerImpl s_tap_recognizer_impl = {
  .handle_touch_event = prv_handle_touch_event,
  .reset = prv_reset,
  .cancel = prv_cancel
};

static void prv_handle_touch_event(Recognizer *recognizer, const TouchEvent *touch_event) {
  TapRecognizerData *data = recognizer_get_impl_data((Recognizer *)recognizer,
                                                     &s_tap_recognizer_impl);

  // TODO: This is a stub and fails immediately
  // (https://pebbletechnology.atlassian.net/browse/PBL-28983)
  (void)data;

  recognizer_transition_state(recognizer, RecognizerState_Failed);
}

static void prv_reset(Recognizer *recognizer) {
  TapRecognizerData *data = recognizer_get_impl_data((Recognizer *)recognizer,
                                                     &s_tap_recognizer_impl);
  (void)data;
}

static bool prv_cancel(Recognizer *recognizer) {
  prv_reset(recognizer);
  return false;
}

Recognizer *tap_recognizer_create(RecognizerEventCb event_cb, void *user_data) {
  TapRecognizerData data = {
    .config = {
      .taps_required = 1,
      .fingers_required = 1,
    },
  };

  return recognizer_create_with_data(&s_tap_recognizer_impl, &data, sizeof(data), event_cb,
                                     user_data);
}

const TapRecognizerData *tap_recognizer_get_data(const Recognizer *recognizer) {
  return recognizer_get_impl_data((Recognizer *)recognizer, &s_tap_recognizer_impl);
}

void tap_recognizer_set_num_taps_required(Recognizer *recognizer, int num_taps) {
  TapRecognizerData *data = recognizer_get_impl_data(recognizer, &s_tap_recognizer_impl);
  if (!data) {
    return;
  }
  data->config.taps_required = num_taps;
}
