/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "recognizer.h"

#include "applib/ui/layer.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum RecognizerManagerState {
  RecognizerManagerState_WaitForTouchdown,
  RecognizerManagerState_RecognizersActive,
  RecognizerManagerState_RecognizersTriggered
} RecognizerManagerState;

typedef struct RecognizerManager {
  struct Window *window;
  Layer *active_layer;
  RecognizerManagerState state;
  Recognizer *triggered;
} RecognizerManager;

void recognizer_manager_init(RecognizerManager *manager);

//! Note: Conforms to touch service touch handler prototype
void recognizer_manager_handle_touch_event(const TouchEvent *touch_event, void *context);

//! Set the window that the recognizer manager manages
void recognizer_manager_set_window(RecognizerManager *manager, struct Window *window);

//! Cancel all ongoing touches. Called when window transitions or other events occur that would
//! invalidate previous touch events (e.g. Palm detection)
void recognizer_manager_cancel_touches(RecognizerManager *manager);

//! Reset the state of the recognizer manager.
void recognizer_manager_reset(RecognizerManager *manager);

//! Register a recognizer with the recognizer manager. This will force the recognizer into the
//! correct state, depending on the state of other recognizers being managed by the recognizer
//! manager.
//! Note: This must be called by all objects when attaching recognizers to ensure that the
//! recognizers are in the correct state
void recognizer_manager_register_recognizer(RecognizerManager *manager, Recognizer *recognizer);

//! Deregister a recognizer with the recognizer manager. This will allow the recognizer manager to
//! adjust the state of all other recognizers, if necessary, when a recognizer is detached from it's
//! owner
//! Note: This must be called by all objects when detaching recognizers to ensure that the
//! recognizer manager remains in the correct state
void recognizer_manager_deregister_recognizer(RecognizerManager *manager, Recognizer *recognizer);

//! Handle a state change after a recognizer changes state outside a touch event handler
//! Used to handle state changes caused by timer events and other events that could influence touch
//! gestures
void recognizer_manager_handle_state_change(RecognizerManager *manager, Recognizer *changed);
