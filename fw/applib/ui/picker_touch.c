/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#include "picker_touch.h"

#define TOUCH_PIXELS_PER_STEP 18

int16_t picker_touch_steps_from_drag(int16_t delta_y) {
  const int16_t half_step = TOUCH_PIXELS_PER_STEP / 2;
  return (delta_y >= 0 ? delta_y + half_step : delta_y - half_step) / TOUCH_PIXELS_PER_STEP;
}

#ifdef CONFIG_TOUCH

#include "applib/ui/recognizer/recognizer_manager.h"
#include "kernel/pebble_tasks.h"

struct TouchNavState *app_state_get_touch_nav_state(void);
struct TouchNavState *modal_manager_get_touch_nav_state(void);

static TouchNavState *prv_task_touch_nav_state(void) {
  return (pebble_task_get_current() == PebbleTask_App) ? app_state_get_touch_nav_state()
                                                       : modal_manager_get_touch_nav_state();
}

static bool prv_ops_can_start(void *context) {
  PickerTouch *touch = context;
  return !touch->callbacks.can_start || touch->callbacks.can_start(touch->context);
}

static void prv_apply_drag(PickerTouch *touch, int16_t delta_y) {
  const int16_t steps = picker_touch_steps_from_drag(delta_y);
  while (touch->applied_steps != steps) {
    const int direction = (steps > touch->applied_steps) ? 1 : -1;
    if (touch->callbacks.step) {
      touch->callbacks.step(direction, touch->context);
    }
    touch->applied_steps += direction;
  }
}

static void prv_ops_pan_started(void *context) {
  ((PickerTouch *)context)->applied_steps = 0;
}

static GPointReturn prv_ops_get_base_offset(void *context) {
  return GPointZero;
}

static void prv_ops_pan_update(void *context, GPoint base, GPoint delta) {
  prv_apply_drag(context, delta.y);
}

static void prv_ops_pan_snap(void *context, GPoint base, GPoint final_delta, GPoint velocity) {
  prv_apply_drag(context, final_delta.y);
}

static void prv_ops_pan_cancel(void *context) {
}

static void prv_ops_tap(void *context, GPoint point_on_screen) {
  PickerTouch *touch = context;
  if (touch->callbacks.tap) {
    touch->callbacks.tap(point_on_screen, touch->context);
  }
}

static void prv_emit_button(ButtonId button) {
  const TouchNavState *state = prv_task_touch_nav_state();
  if (!state || !state->ops || button >= NUM_BUTTONS) {
    return;
  }
  const TouchNavOps *ops = state->ops;
  if (ops->is_animating && ops->is_animating(ops->ctx)) {
    return;
  }
  if (button == BUTTON_ID_BACK && !(ops->top_overrides_back && ops->top_overrides_back(ops->ctx))) {
    if (ops->pop_top) {
      ops->pop_top(ops->ctx);
    }
  } else if (ops->emit_button) {
    ops->emit_button(ops->ctx, button);
  }
}

static void prv_ops_swipe(void *context, SwipeDirection direction) {
  prv_emit_button(direction == SwipeDirection_Left ? BUTTON_ID_SELECT : BUTTON_ID_BACK);
}

static const TouchNavWidgetOps s_picker_touch_nav_ops = {
  .can_start = prv_ops_can_start,
  .pan_started = prv_ops_pan_started,
  .get_base_offset = prv_ops_get_base_offset,
  .pan_update = prv_ops_pan_update,
  .pan_snap = prv_ops_pan_snap,
  .pan_cancel = prv_ops_pan_cancel,
  .tap = prv_ops_tap,
  .swipe = prv_ops_swipe,
};

#endif

void picker_touch_init(PickerTouch *touch, Layer *layer, PickerTouchCallbacks callbacks,
                       void *context) {
#ifdef CONFIG_TOUCH
  *touch = (PickerTouch){
    .callbacks = callbacks,
    .context = context,
  };
  TouchNavState *state = prv_task_touch_nav_state();
  if (state && state->manager) {
    touch_nav_registry_add(state, TouchNavWidgetType_Scroll, &touch->touch_nav_node, layer,
                           &s_picker_touch_nav_ops, touch);
  }
#else
  *touch = (PickerTouch){};
  (void)layer;
  (void)callbacks;
  (void)context;
#endif
}

void picker_touch_deinit(PickerTouch *touch) {
#ifdef CONFIG_TOUCH
  TouchNavState *state = prv_task_touch_nav_state();
  if (state) {
    const bool was_target = state->latched_target && state->latched_target->widget == touch;
    touch_nav_registry_remove(state, TouchNavWidgetType_Scroll, &touch->touch_nav_node);
    if (was_target && state->manager) {
      recognizer_manager_cancel_and_reset(state->manager);
    }
  }
  *touch = (PickerTouch){};
#else
  (void)touch;
#endif
}
