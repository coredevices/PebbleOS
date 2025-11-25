/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "recognizer_list.h"

#include "services/common/touch/touch_event.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct Recognizer Recognizer;

typedef enum RecognizerState {
  RecognizerState_Failed,
  RecognizerState_Possible,
  RecognizerState_Started,
  RecognizerState_Updated,
  RecognizerState_Completed,
  RecognizerState_Cancelled,

  RecognizerStateCount
} RecognizerState;

typedef enum RecognizerEventType {
  RecognizerEvent_Started,
  RecognizerEvent_Updated,
  RecognizerEvent_Completed,
  RecognizerEvent_Cancelled,
} RecognizerEvent;

//! User event callback. When a recognizer changes state to any state other than the failed state
//! the user callback of this type will be called.
//! @param recognizer recognizer affected by the event
//! @param event_type event that occurred
typedef void (*RecognizerEventCb)(const Recognizer *recognizer, RecognizerEvent event_type);

//! Callback called when recognizer is destroyed. Allows additional user de-initialization &
//! destruction of data
//! @param recognizer that will be destroyed
typedef void (*RecognizerOnDestroyCb)(const Recognizer *recognizer);

//! Touch filter that determines whether the recognizer will handle a sequence of touch events. It
//! is called when the recognizer first receives touches after reset.
//! @param recognizer recognizer to filter touches for
//! @param touch_event touch event that will be received by the recognizer if allowed to handle
//! touches
//! @return true if the recognizer should handle the touch events
typedef bool (*RecognizerTouchFilterCb)(const Recognizer *recognizer,
                                        const TouchEvent *touch_event);

//! This function is called to determine whether \a recognizer should be evaluated simultaneously
//! with \a simultaneous_with
//! @param recognizer recognizer to be tested
//! @param simultaneous_with recognizer with which \a recognizer would be evaluated simultaneously
//! should this test pass
//! @return true if recognizers should be evaluated simultaneously
typedef bool (*RecognizerSimultaneousWithCb)(const Recognizer *recognizer,
                                             const Recognizer *simultaneous_with);

//! Get the current recognizer state. No restrictions on where it is called from
//! @param recognizer recognizer to get the state from
//! @return current recognizer state
RecognizerState recognizer_get_state(const Recognizer *recognizer);

//! Tell a recognizer to only evaluate after the another recognizer fails
//! @param recognizer recognizer to modify
//! @param fail_after recognizer after which the modified gesture will be evaluated
void recognizer_set_fail_after(Recognizer *recognizer, Recognizer *fail_after);

//! Get the recognizer that must fail before this recognizer is evaluated
//! @param recognizer recognizer to check
//! @return \ref Recognizer that recognizer is configured to fail after
Recognizer *recognizer_get_fail_after(const Recognizer *recognizer);

//! Check whether a recognizer should be evaluated simultaneously with another recognizer

//! Specify a callback to determine whether a recognizer should be evaluated simultaneously with
//! another recognizer
//! @param recognizer recognizer to modify
//! @param simultaneous_with_cb callback that determines whether this recognizer will be evaluated
//! simultaneous with another recognizer
void recognizer_set_simultaneous_with(Recognizer *recognizer,
                                      RecognizerSimultaneousWithCb simultaneous_with_cb);

//! Check whether a recognizer should evaluate simultaneously with \a test
//! @param recognizer recognizer to be tested against
//! @param test check whether this recognizer is evaluated simultaneously with \ref recognizer
//! @return true if the two recognizers will be evaluated simultaneously
bool recognizer_should_evaluate_simultaneously(const Recognizer *recognizer,
                                               const Recognizer *test);

//! Check whether a recognizer is still actively looking for a gesture
//! @param recognizer recognizer to query
//! @return true if recognizer is not in an end state
bool recognizer_is_active(const Recognizer *recognizer);

//! Check whether the recognizer has started (and may have finished recognizing a gesture)
//! @param recognizer recognizer to query
//! @return true if recognizer has started to recognize a gesture; false otherwise
bool recognizer_has_triggered(const Recognizer *recognizer);

//! Set the user data attached to the recognizer
//! @param recognizer recognizer to modify
void recognizer_set_user_data(Recognizer *recognizer, void *data);

//! Get the user data attached to the recognizer
//! @param recognizer recognizer to get the data from
//! @return pointer to user data
void *recognizer_get_user_data(const Recognizer *recognizer);

//! Set the touch filter used to determine whether a recognizer should start analysing a series of
//! touch events
//! @param recognizer recognizer to modify
//! @param filter_cb filter callback
void recognizer_set_touch_filter(Recognizer *recognizer, RecognizerTouchFilterCb filter_cb);

//! Set the callback that will be called when the recognizer is destroyed
//! @param recognizer recognizer to modify
//! @param on_destroy_cb destruction handler
void recognizer_set_on_destroy(Recognizer *recognizer, RecognizerOnDestroyCb on_destroy_cb);

//! @internal
//! Add a recognizer to a list. Used to attach it to a layer, window or app. Recognizer will not be
//! added to the list if it is already in another list
//! @param recognizer recognizer to add to list
//! @param list \ref RecognizerList to which to add recognizer
void recognizer_add_to_list(Recognizer *recognizer, RecognizerList *list);

//! @internal
//! Remove a recognizer from a list. Used to detach it from a layer, window, or app.
//! @param recognizer recognizer to remove from list
//! @param list \ref RecognizerList from which to remove recognizer
void recognizer_remove_from_list(Recognizer *recognizer, RecognizerList *list);

//! Destroy an un-owned recognizer. If a recognizer is not owned, this will destroy the recognizer
//! freeing it's data and calling the destructor (see \ref recognizer_set_on_destroy), if set. If it
//! is owned, this will do nothing.
//! @param recognizer \ref Recognizer to destroy
void recognizer_destroy(Recognizer *recognizer);

//! Return whether or not a recognizer is owned (by a layer, window or app)
//! @param recognizer \ref Recognizer to check
//! @return true if recognizer is owned
bool recognizer_is_owned(Recognizer *recognizer);
