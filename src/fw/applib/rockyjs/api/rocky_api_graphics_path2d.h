/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "rocky_api.h"
#include "jerry-api.h"

#include "applib/graphics/gtypes.h"

void rocky_api_graphics_path2d_add_canvas_methods(jerry_value_t obj);
//! Resets the internal state and frees any memory associated with it.
void rocky_api_graphics_path2d_reset_state(void);

// these types exist so that we can unit test the internal state
typedef enum {
  RockyAPIPathStepType_MoveTo,
  RockyAPIPathStepType_LineTo,
  RockyAPIPathStepType_Arc,
} RockyAPIPathStepType;

typedef struct RockyAPIPathStepPoint {
  GPointPrecise xy;
  // to be applied to xy when calling .fill() (not .stroke()) as a workaround for some of our
  // rendering quirks, needs to be solved for real
  GVectorPrecise fill_delta;
} RockyAPIPathStepPoint;

typedef struct RockyAPIPathStepArc {
  GPointPrecise center;
  Fixed_S16_3 radius;
  int32_t angle_start;
  int32_t angle_end;
  bool anti_clockwise;
} RockyAPIPathStepArc;

typedef struct RockyAPIPathStep {
  RockyAPIPathStepType type;
  union {
    RockyAPIPathStepPoint pt;
    RockyAPIPathStepArc arc;
  };
} RockyAPIPathStep;
