/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "recognizer.h"
#include "recognizer_impl.h"
#include "recognizer_manager.h"

#include "services/common/touch/touch_event.h"
#include "util/list.h"

#include <stdbool.h>
#include <stdint.h>

typedef void (*RecognizerOnRemoveCb)(Recognizer *recognizer, void *context);

struct Recognizer {
  ListNode node;
  RecognizerState state;
  const RecognizerImpl *impl;
  RecognizerManager *manager;

  struct {
    RecognizerEventCb event;
    RecognizerTouchFilterCb filter;
    RecognizerOnDestroyCb on_destroy;
    void *data;
  } subscriber;

  union {
    struct {
      bool handling_touch_event:1;
      bool is_owned:1;
    };
    uint32_t flags;
  };

  struct Recognizer *fail_after;
  RecognizerSimultaneousWithCb simultaneous_with_cb;

  uint8_t impl_data[];
};

//! Used to dispatch a touch event to a touch gesture recognizer
//! Called by the recognizer manager to dispatch events to each recognizer
void recognizer_handle_touch_event(Recognizer *recognizer, const TouchEvent *touch_event);

//! Reset the recognizer. Will cancel the recognizer before resetting it
void recognizer_reset(Recognizer *recognizer);

//! Cancel the gesture being recognized
void recognizer_cancel(Recognizer *recognizer);

//! Set the state of the recognizer to the failed state
void recognizer_set_failed(Recognizer *recognizer);

//! Set the manager that will manage this recognizer
void recognizer_set_manager(Recognizer *recognizer, RecognizerManager *manager);

//! Get the manager managing this recognizer
RecognizerManager *recognizer_get_manager(Recognizer *recognizer);
