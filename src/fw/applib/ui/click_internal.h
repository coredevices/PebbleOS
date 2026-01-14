/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! @file click_internal.h
//!

#pragma once

#include "click.h"
#include "applib/app_timer.h"

/**
A bag of parameters that holds all of the state required to identify
different types of clicks performed on a single button. You can think
of this as the per-button "context" used by the click detection
system. A single set of ClickRecognizers are shared between all
windows within an app, though only the top-most window may use the
recognizers (see the notes in app_window_click_glue.c).

<p/>

Each ClickRecognizer contains a ClickConfig struct that holds the
callbacks (ClickHandlers) to be fired after a click has been
detected/dispatched to the system event loop. ClickConfigs are
typically instantiated by calling a configuration callback (the
window's ClickConfigProvider) that is responsible for copying over a
template to the app's ClickRecognizers.

<p/>

Whenever a the head of the window stack changes, the OS is responsible
for ensuring that all of its registered click recognizers are reset
and reconfigured using the new visible window's
ClickConfigProvider. This happens in the window_stack_private_push &
window_stack_private_pop functions used to place a new window at the
top of the stack.

@see ClickConfig
@see ClickHandler
@see ClickConfigProvider
@see app_window_click_glue.c
@see window_set_click_config_provider_with_context
*/
typedef struct ClickRecognizer {
  ButtonId button;
  ClickConfig config;
  bool is_button_down;
  bool is_repeating;

  uint8_t number_of_clicks_counted;

  AppTimer *hold_timer;
  AppTimer *multi_click_timer;
} ClickRecognizer;

typedef struct ClickManager {
  ClickRecognizer recognizers[NUM_BUTTONS];
} ClickManager;

//! Tell the particular recognizer that the associated button has been released.
void click_recognizer_handle_button_up(ClickRecognizer *recognizer);

//! Tell the particular recognizer that the associated button has been pressed.
void click_recognizer_handle_button_down(ClickRecognizer *recognizer);

//! Reset the state of a single recognizer, including timers.
void click_recognizer_reset(ClickRecognizer *recognizer);

//! Initialize a click manager for use. This only needs to be called once to initialize the structure, and then the
//! same struct can be reconfigured multiple times by using click_manager_clear.
void click_manager_init(ClickManager* click_manager);

//! Clear out any state from the click manager, including configuration. This ClickManager can be reconfigured at any
//! time.
void click_manager_clear(ClickManager* click_manager);

//! Reset the state from the click manager, including timers.
void click_manager_reset(ClickManager* click_manager);

