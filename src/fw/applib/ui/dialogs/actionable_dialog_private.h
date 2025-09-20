/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//! An ActionableDialog is a dialog that has an action bar on the right hand side
//! of the window.  The user can specify there own custom \ref ActionBarLayer to
//! override the default behaviour or specify a \ref ClickConfigProvider to tie
//! into the default \ref ActionBarLayer provided by the dialog.
#pragma once

#include "applib/graphics/gtypes.h"
#include "applib/graphics/perimeter.h"
#include "applib/ui/action_bar_layer.h"
#include "applib/ui/click.h"
#include "applib/ui/dialogs/dialog.h"

//! Different types of action bar. Two commonly used types are built in:
//! Confirm and Decline.  Alternatively, the user can supply their own
//! custom action bar.
typedef enum DialogActionBarType {
  //! SELECT: Confirm icon
  DialogActionBarConfirm,
  //! SELECT: Decline icon
  DialogActionBarDecline,
  //! UP: Confirm icon, DOWN: Decline icon
  DialogActionBarConfirmDecline,
  //! Provide your own action bar
  DialogActionBarCustom
} DialogActionBarType;

typedef struct ActionableDialog {
  Dialog dialog;
  DialogActionBarType action_bar_type;
  union {
    struct {
      GBitmap *select_icon;
    };
    struct {
      GBitmap *up_icon;
      GBitmap *down_icon;
    };
  };
  ActionBarLayer *action_bar;
  ClickConfigProvider config_provider;
  void *user_data;
} ActionableDialog;
