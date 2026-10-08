/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "music_volume.h"

#include "applib/ui/recognizer/recognizer_impl.h"
#include "pbl/util/math.h"
#include "pbl/util/trig.h"

#include <string.h>

#define START_ANGLE             DEG_TO_TRIGANGLE(45)
#define STEP_ANGLE              DEG_TO_TRIGANGLE(30)
#define RADIUS_TOLERANCE        16
#define HEADING_SAMPLE_DISTANCE 12
#define MIN_HEADING_CHANGE      DEG_TO_TRIGANGLE(20)

typedef struct {
  GPoint center;
  int16_t min_radius;
  int16_t max_radius;
  struct {
    int16_t radius;
    int32_t angle;
    int32_t remainder;
    int32_t steps;
    GPoint heading_point;
    int32_t first_heading;
    int32_t heading;
    bool has_heading;
  } state;
} MusicVolumeRecognizerData;

static const RecognizerImpl s_impl;

static MusicVolumeRecognizerData *prv_data(const Recognizer *recognizer) {
  return recognizer_get_impl_data((Recognizer *)recognizer, &s_impl);
}

static void prv_end(Recognizer *recognizer) {
  recognizer_transition_state(recognizer, recognizer_has_triggered(recognizer)
                                              ? RecognizerState_Cancelled
                                              : RecognizerState_Failed);
}

static int32_t prv_angle_delta(int32_t angle, int32_t previous) {
  int32_t delta = angle - previous;
  if (delta > TRIG_PI) {
    delta -= TRIG_MAX_ANGLE;
  } else if (delta < -TRIG_PI) {
    delta += TRIG_MAX_ANGLE;
  }
  return delta;
}

static void prv_handle_touch(Recognizer *recognizer, const TouchEvent *event) {
  MusicVolumeRecognizerData *data = prv_data(recognizer);
  if (event->type == TouchEvent_Liftoff) {
    // The driver reports liftoff at (0, 0), not at the last finger position.
    recognizer_transition_state(recognizer, recognizer_has_triggered(recognizer)
                                                ? RecognizerState_Completed
                                                : RecognizerState_Failed);
    return;
  }
  const int16_t x = event->x - data->center.x;
  const int16_t y = event->y - data->center.y;
  const int16_t radius = integer_sqrt((int32_t)x * x + (int32_t)y * y);
  if (event->non_navigational || radius < data->min_radius || radius > data->max_radius) {
    prv_end(recognizer);
    return;
  }
  const int32_t angle = atan2_lookup(y, x);
  if (event->type == TouchEvent_Touchdown) {
    data->state.radius = radius;
    data->state.angle = angle;
    data->state.heading_point = GPoint(event->x, event->y);
    return;
  }
  if (ABS(radius - data->state.radius) > RADIUS_TOLERANCE) {
    prv_end(recognizer);
    return;
  }
  const int32_t delta = prv_angle_delta(angle, data->state.angle);
  // Discontinuous samples must not turn into a burst of volume commands.
  if (ABS(delta) > TRIG_MAX_ANGLE / 4) {
    prv_end(recognizer);
    return;
  }
  data->state.angle = angle;
  data->state.remainder += delta;
  const bool started = recognizer_has_triggered(recognizer);
  if (!started) {
    const int16_t dx = event->x - data->state.heading_point.x;
    const int16_t dy = event->y - data->state.heading_point.y;
    if ((int32_t)dx * dx + (int32_t)dy * dy >= HEADING_SAMPLE_DISTANCE * HEADING_SAMPLE_DISTANCE) {
      data->state.heading = atan2_lookup(dy, dx);
      data->state.heading_point = GPoint(event->x, event->y);
      if (!data->state.has_heading) {
        data->state.first_heading = data->state.heading;
        data->state.has_heading = true;
      }
    }
    // A straight chord can stay inside the radial tolerance; require curvature too.
    const int32_t turn = prv_angle_delta(data->state.heading, data->state.first_heading);
    if (ABS(data->state.remainder) < START_ANGLE || ABS(turn) < MIN_HEADING_CHANGE ||
        ((turn > 0) != (data->state.remainder > 0))) {
      return;
    }
  }
  const int32_t steps = data->state.remainder / STEP_ANGLE;
  data->state.steps += steps;
  data->state.remainder -= steps * STEP_ANGLE;
  recognizer_transition_state(recognizer,
                              started ? RecognizerState_Updated : RecognizerState_Started);
}

static void prv_reset(Recognizer *recognizer) {
  MusicVolumeRecognizerData *data = prv_data(recognizer);
  memset(&data->state, 0, sizeof(data->state));
}

static bool prv_cancel(Recognizer *recognizer) {
  return recognizer_has_triggered(recognizer);
}

static const RecognizerImpl s_impl = {
  .handle_touch_event = prv_handle_touch,
  .reset = prv_reset,
  .cancel = prv_cancel,
};

Recognizer *music_volume_recognizer_create(RecognizerEventCb callback, void *context, GPoint center,
                                           int16_t min_radius, int16_t max_radius) {
  const MusicVolumeRecognizerData data = {
    .center = center,
    .min_radius = min_radius,
    .max_radius = max_radius,
  };
  return recognizer_create_with_data(&s_impl, &data, sizeof(data), callback, context);
}

int32_t music_volume_recognizer_get_steps(const Recognizer *recognizer) {
  return prv_data(recognizer)->state.steps;
}
