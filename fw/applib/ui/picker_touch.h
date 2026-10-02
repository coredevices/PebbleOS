/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "layer.h"

#include "applib/graphics/gtypes.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef CONFIG_TOUCH
#include "applib/ui/recognizer/touch_nav.h"
#endif

typedef void (*PickerTouchStepHandler)(int direction, void *context);
typedef void (*PickerTouchTapHandler)(GPoint point_on_screen, void *context);
typedef bool (*PickerTouchCanStartHandler)(void *context);

typedef struct PickerTouchCallbacks {
  PickerTouchStepHandler step;
  PickerTouchTapHandler tap;
  PickerTouchCanStartHandler can_start;
} PickerTouchCallbacks;

typedef struct PickerTouch {
#ifdef CONFIG_TOUCH
  TouchNavWidgetNode touch_nav_node;
  PickerTouchCallbacks callbacks;
  void *context;
  int16_t applied_steps;
#else
  uint8_t unused;
#endif
} PickerTouch;

int16_t picker_touch_steps_from_drag(int16_t delta_y);

void picker_touch_init(PickerTouch *touch, Layer *layer, PickerTouchCallbacks callbacks,
                       void *context);

void picker_touch_deinit(PickerTouch *touch);
