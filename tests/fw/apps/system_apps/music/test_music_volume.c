/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "apps/system/music_volume.h"
#include "applib/ui/recognizer/recognizer_private.h"
#include "pbl/util/trig.h"

#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_logging.h"
#include "fw/ui/recognizer/test_recognizer_impl.h"

void recognizer_manager_handle_state_change(RecognizerManager *manager, Recognizer *changed) {
}

static RecognizerEvent s_event;

static void prv_callback(const Recognizer *recognizer, RecognizerEvent event) {
  s_event = event;
}

static void prv_send(Recognizer *r, TouchEventType type, int16_t x, int16_t y) {
  const TouchEvent event = {.type = type, .x = x, .y = y};
  recognizer_handle_touch_event(r, &event);
}

static void prv_angle(Recognizer *r, TouchEventType type, int degrees) {
  const int32_t angle = DEG_TO_TRIGANGLE(degrees);
  prv_send(r, type, 130 + cos_lookup(angle) * 100 / TRIG_MAX_RATIO,
           130 + sin_lookup(angle) * 100 / TRIG_MAX_RATIO);
}

static Recognizer *prv_create(void) {
  return music_volume_recognizer_create(prv_callback, NULL, GPoint(130, 130), 65, 126);
}

void test_music_volume__clockwise_and_liftoff(void) {
  NEW_RECOGNIZER(r) = prv_create();
  prv_angle(r, TouchEvent_Touchdown, -90);
  for (int angle = -80; angle <= 10; angle += 10) {
    prv_angle(r, TouchEvent_PositionUpdate, angle);
  }
  cl_assert_equal_i(music_volume_recognizer_get_steps(r), 3);
  prv_send(r, TouchEvent_Liftoff, 0, 0);
  cl_assert_equal_i(s_event, RecognizerEvent_Completed);
  cl_assert_equal_i(music_volume_recognizer_get_steps(r), 3);
}

void test_music_volume__counterclockwise_across_wrap(void) {
  NEW_RECOGNIZER(r) = prv_create();
  prv_angle(r, TouchEvent_Touchdown, 10);
  for (int angle = 0; angle >= -90; angle -= 10) {
    prv_angle(r, TouchEvent_PositionUpdate, angle);
  }
  cl_assert_equal_i(music_volume_recognizer_get_steps(r), -3);
}

void test_music_volume__clockwise_across_wrap(void) {
  NEW_RECOGNIZER(r) = prv_create();
  prv_angle(r, TouchEvent_Touchdown, 170);
  for (int angle = 180; angle <= 270; angle += 10) {
    prv_angle(r, TouchEvent_PositionUpdate, angle);
  }
  cl_assert_equal_i(music_volume_recognizer_get_steps(r), 3);
}

void test_music_volume__reversal_and_multiple_turns(void) {
  NEW_RECOGNIZER(r) = prv_create();
  prv_angle(r, TouchEvent_Touchdown, 0);
  for (int angle = 10; angle <= 730; angle += 10) {
    prv_angle(r, TouchEvent_PositionUpdate, angle);
  }
  cl_assert_equal_i(music_volume_recognizer_get_steps(r), 24);
  for (int angle = 720; angle >= 630; angle -= 10) {
    prv_angle(r, TouchEvent_PositionUpdate, angle);
  }
  cl_assert_equal_i(music_volume_recognizer_get_steps(r), 22);
}

void test_music_volume__tap_and_jitter_do_not_start(void) {
  NEW_RECOGNIZER(r) = prv_create();
  prv_angle(r, TouchEvent_Touchdown, -90);
  for (int i = 0; i < 50; i++) {
    prv_angle(r, TouchEvent_PositionUpdate, -90 + i % 3);
  }
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Possible);
  prv_send(r, TouchEvent_Liftoff, 0, 0);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Failed);
}

void test_music_volume__center_and_unarmed_contacts_fail(void) {
  NEW_RECOGNIZER(r) = prv_create();
  prv_send(r, TouchEvent_Touchdown, 130, 130);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Failed);
  recognizer_reset(r);
  const TouchEvent event = {
    .type = TouchEvent_Touchdown,
    .x = 130,
    .y = 30,
    .non_navigational = true,
  };
  recognizer_handle_touch_event(r, &event);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Failed);
}

void test_music_volume__straight_tangent_is_not_a_rotation(void) {
  NEW_RECOGNIZER(r) = prv_create();
  prv_send(r, TouchEvent_Touchdown, 130, 30);
  for (int x = 140; x <= 200 && recognizer_is_active(r); x += 10) {
    prv_send(r, TouchEvent_PositionUpdate, x, 30);
  }
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Failed);
  cl_assert_equal_i(music_volume_recognizer_get_steps(r), 0);
}

void test_music_volume__radial_motion_and_position_jumps_fail(void) {
  NEW_RECOGNIZER(r) = prv_create();
  prv_send(r, TouchEvent_Touchdown, 130, 30);
  prv_send(r, TouchEvent_PositionUpdate, 130, 60);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Failed);
  recognizer_reset(r);
  prv_angle(r, TouchEvent_Touchdown, 0);
  prv_angle(r, TouchEvent_PositionUpdate, 160);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Failed);
}

void test_music_volume__straight_chord_inside_ring_does_not_start(void) {
  NEW_RECOGNIZER(r) = prv_create();
  prv_send(r, TouchEvent_Touchdown, 80, 40);
  for (int x = 85; x <= 180; x += 5) {
    prv_send(r, TouchEvent_PositionUpdate, x, 40);
  }
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Possible);
  prv_send(r, TouchEvent_Liftoff, 0, 0);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Failed);
  cl_assert_equal_i(music_volume_recognizer_get_steps(r), 0);
}

void test_music_volume__leaving_ring_cancels_and_next_contact_resets(void) {
  NEW_RECOGNIZER(r) = prv_create();
  prv_angle(r, TouchEvent_Touchdown, 0);
  for (int angle = 10; angle <= 60; angle += 10) {
    prv_angle(r, TouchEvent_PositionUpdate, angle);
  }
  prv_send(r, TouchEvent_PositionUpdate, 130, 130);
  cl_assert_equal_i(s_event, RecognizerEvent_Cancelled);
  recognizer_reset(r);
  cl_assert_equal_i(music_volume_recognizer_get_steps(r), 0);
  prv_angle(r, TouchEvent_Touchdown, 0);
  for (int angle = -10; angle >= -100; angle -= 10) {
    prv_angle(r, TouchEvent_PositionUpdate, angle);
  }
  cl_assert_equal_i(music_volume_recognizer_get_steps(r), -3);
}
