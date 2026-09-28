/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "action_bar_layer.h"
#include "value_picker.h"
#include "window.h"

struct ValuePickerWindow;

typedef void (*ValuePickerWindowCallback)(struct ValuePickerWindow *picker_window, void *context);

typedef struct ValuePickerWindowCallbacks {
  //! Called after every change of the value. Optional.
  ValuePickerWindowCallback changed;
  //! Called on SELECT. The callback decides whether to remove the window.
  ValuePickerWindowCallback selected;
  //! Called from the window's unload handler, after the picker is torn down. Optional.
  ValuePickerWindowCallback unload;
} ValuePickerWindowCallbacks;

typedef struct ValuePickerWindow {
  Window window;
  ActionBarLayer action_bar;
  ValuePickerContent content;
  const ValuePickerStyle *style;
  ValuePickerWindowCallbacks callbacks;
  void *callback_context;
  ValuePickerTouch touch;
} ValuePickerWindow;

//! The initial value is clamped to the content's range. A NULL style uses the default style.
void value_picker_window_init(ValuePickerWindow *picker_window, const ValuePickerContent *content,
                              const ValuePickerStyle *style, ValuePickerWindowCallbacks callbacks,
                              void *callback_context);

int32_t value_picker_window_get_value(const ValuePickerWindow *picker_window);
