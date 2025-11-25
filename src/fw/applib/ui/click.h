/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! @file click.h
//!

#pragma once

#include "drivers/button_id.h"

#include <stdint.h>
#include <stdbool.h>

//! @addtogroup UI
//! @{
//!   @addtogroup Clicks
//! \brief Handling button click interactions
//!
//! Each Pebble window handles Pebble's buttons while it is displayed. Raw button down and button
//! up events are transformed into click events that can be transferred to your app:
//!
//! * Single-click. Detects a single click, that is, a button down event followed by a button up event.
//! It also offers hold-to-repeat functionality (repeated click).
//! * Multi-click. Detects double-clicking, triple-clicking and other arbitrary click counts.
//! It can fire its event handler on all of the matched clicks, or just the last.
//! * Long-click. Detects long clicks, that is, press-and-hold.
//! * Raw. Simply forwards the raw button events. It is provided as a way to use both the higher level
//! "clicks" processing and the raw button events at the same time.
//!
//! To receive click events when a window is displayed, you must register a \ref ClickConfigProvider for
//! this window with \ref window_set_click_config_provider(). Your \ref ClickConfigProvider will be called every time
//! the window becomes visible with one context argument. By default this context is a pointer to the window but you can
//! change this with \ref window_set_click_config_provider_with_context().
//!
//! In your \ref ClickConfigProvider you call the \ref window_single_click_subscribe(), \ref window_single_repeating_click_subscribe(),
//! \ref window_multi_click_subscribe(), \ref window_long_click_subscribe() and \ref window_raw_click_subscribe() functions to register
//! a handler for each event you wish to receive.
//!
//! For convenience, click handlers are provided with a \ref ClickRecognizerRef and a user-specified context.
//!
//! The \ref ClickRecognizerRef can be used in combination with \ref click_number_of_clicks_counted(), \ref
//! click_recognizer_get_button_id() and \ref click_recognizer_is_repeating() to get more information about the click. This is
//! useful if you want different buttons or event types to share the same handler.
//!
//! The user-specified context is the context of your \ref ClickConfigProvider (see above). By default it points to the window.
//! You can override it for all handlers with \ref window_set_click_config_provider_with_context() or for a specific button with \ref
//! window_set_click_context().
//!
//! <h3>User interaction in watchfaces</h3>
//! Watchfaces cannot use the buttons to interact with the user. Instead, you can use the \ref AccelerometerService.
//!
//! <h3>About the Back button</h3>
//! By default, the Back button will always pop to the previous window on the \ref WindowStack (and leave the app if the current
//! window is the only window). You can override the default back button behavior with \ref window_single_click_subscribe() and
//! \ref window_multi_click_subscribe() but you cannot set a repeating, long or raw click handler on the back button because a long press
//! will always terminate the app and return to the main menu.
//!
//! <h3>Usage example</h3>
//! First associate a click config provider callback with your window:
//! \code{.c}
//! void app_init(void) {
//!   ...
//!   window_set_click_config_provider(&window, (ClickConfigProvider) config_provider);
//!   ...
//! }
//! \endcode
//! Then in the callback, you set your desired configuration for each button:
//! \code{.c}
//! void config_provider(Window *window) {
//!  // single click / repeat-on-hold config:
//!   window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
//!   window_single_repeating_click_subscribe(BUTTON_ID_SELECT, 1000, select_single_click_handler);
//!
//!   // multi click config:
//!   window_multi_click_subscribe(BUTTON_ID_SELECT, 2, 10, 0, true, select_multi_click_handler);
//!
//!   // long click config:
//!   window_long_click_subscribe(BUTTON_ID_SELECT, 700, select_long_click_handler, select_long_click_release_handler);
//! }
//! \endcode
//! Now you implement the handlers for each click you've subscribed to and set up:
//! \code{.c}
//! void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
//!   ... called on single click ...
//!   Window *window = (Window *)context;
//! }
//! void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
//!   ... called on single click, and every 1000ms of being held ...
//!   Window *window = (Window *)context;
//! }
//!
//! void select_multi_click_handler(ClickRecognizerRef recognizer, void *context) {
//!   ... called for multi-clicks ...
//!   Window *window = (Window *)context;
//!   const uint16_t count = click_number_of_clicks_counted(recognizer);
//! }
//!
//! void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
//!   ... called on long click start ...
//!   Window *window = (Window *)context;
//! }
//!
//! void select_long_click_release_handler(ClickRecognizerRef recognizer, void *context) {
//!   ... called when long click is released ...
//!   Window *window = (Window *)context;
//! }
//! \endcode
//!
//! <h3>See also</h3>
//! Refer to the \htmlinclude UiFramework.html (chapter "Clicks") for a conceptual
//! overview of clicks and relevant code examples.
//!
//!   @{

//! Reference to opaque click recognizer
//! When a \ref ClickHandler callback is called, the recognizer that fired the handler is passed in.
//! @see \ref ClickHandler
//! @see \ref click_number_of_clicks_counted()
//! @see \ref click_recognizer_get_button_id()
//! @see \ref click_recognizer_is_repeating()
typedef void *ClickRecognizerRef;

//! Function signature of the callback that handles a recognized click pattern
//! @param recognizer The click recognizer that detected a "click" pattern
//! @param context Pointer to application specified data (see \ref window_set_click_config_provider_with_context() and
//! \ref window_set_click_context()). This defaults to the window.
//! @see \ref ClickConfigProvider
typedef void (*ClickHandler)(ClickRecognizerRef recognizer, void *context);

//! @internal
//! Data structure that defines the configuration for one click recognizer.
//! An array of these configuration structures is passed into the \ref ClickConfigProvider
//! callback, for the application to configure.
//! @see ClickConfigProvider
typedef struct ClickConfig {

  //! Pointer to developer-supplied data that is also passed to ClickHandler callbacks
  void *context;

  /** Single-click */
  struct click {
    //! Fired when a single click is detected and every time "repeat_interval_ms" has been reached.
    //! @note When there is a multi_click and/or long_click setup, there will be a delay depending before the single click handler
    //! will get fired. On the other hand, when there is no multi_click nor long_click setup, the single click handler will
    //! fire directly on button down.
    ClickHandler handler;
    //! When holding button down, milliseconds after which "handler" is fired again. The default 0ms means 'no repeat timer'.
    //! 30 ms is the minimum allowable value. Values below will be disregarded.
    //! In case long_click.handler is configured, `repeat_interval_ms` will not be used.
    uint16_t repeat_interval_ms;
  } click; //!< Single-click configuration

  /** Multi-click */
  struct multi_click {
    uint8_t min; //!< Minimum number of clicks before handler is fired. Defaults to 2.
    uint8_t max; //!< Maximum number of clicks after which the click counter is reset. The default 0 means use "min" also as "max".
    bool last_click_only; //!< Defaults to false. When true, only the for the last multi-click the handler is called.
    ClickHandler handler; //!< Fired for multi-clicks, as "filtered" by the `reset_delay`, `last_click_only`, `min` and `max` parameters.
    uint16_t timeout; //!< The delay after which a sequence of clicks is considered finished, and the click counter is reset. The default 0 means 300ms.
  } multi_click; //!< Multi-click configuration

  /** Long-click */
  struct long_click {
    uint16_t delay_ms; //!< Milliseconds after which "handler" is fired. Defaults to 500ms.
    ClickHandler handler; //!< Fired once, directly as soon as "delay" has been reached.
    ClickHandler release_handler; //!< In case a long click has been detected, fired when the button is released.
  } long_click; //!< Long-click configuration

  /** Raw button up & down */
  struct raw {
    ClickHandler up_handler; //!< Fired on button up events
    ClickHandler down_handler; //!< Fired on button down events
    void *context; //!< If this context is not NULL, it will override the general context.
  } raw;  //!< Raw button event pass-through configuration

} ClickConfig;

//! This callback is called every time the window becomes visible (and when you call \ref window_set_click_config_provider() if
//! the window is already visible).
//!
//! Subscribe to click events using
//!   \ref window_single_click_subscribe()
//!   \ref window_single_repeating_click_subscribe()
//!   \ref window_multi_click_subscribe()
//!   \ref window_long_click_subscribe()
//!   \ref window_raw_click_subscribe()
//! These subscriptions will get used by the click recognizers of each of the 4 buttons.
//! @param context Pointer to application specific data (see \ref window_set_click_config_provider_with_context()).
typedef void (*ClickConfigProvider)(void *context);

//! Gets the click count.
//! You can use this inside a click handler implementation to get the click count for multi_click
//! and (repeated) click events.
//! @param recognizer The click recognizer for which to get the click count
//! @return The number of consecutive clicks, and for auto-repeating the number of repetitions.
uint8_t click_number_of_clicks_counted(ClickRecognizerRef recognizer);

//! @internal
//! Returns a pointer to the click recognizer's ClickConfig
ClickConfig *click_recognizer_get_config(ClickRecognizerRef recognizer);

//! Gets the button identifier.
//! You can use this inside a click handler implementation to get the button id for the click event.
//! @param recognizer The click recognizer for which to get the button id that caused the click event
//! @return the ButtonId of the click recognizer
ButtonId click_recognizer_get_button_id(ClickRecognizerRef recognizer);

//! Is this a repeating click.
//! You can use this inside a click handler implementation to find out whether this is a repeating click or not.
//! @param recognizer The click recognizer for which to find out whether this is a repeating click.
//! @return true if this is a repeating click.
bool click_recognizer_is_repeating(ClickRecognizerRef recognizer);

//! @internal
//! Is this button being held down
//! You can use this inside a click handler implementation to check if it's being held down or not
//! @@param recognizer The click recognizer for which to find out whether this is being held down.
//! @return true if this is being held down.
bool click_recognizer_is_held_down(ClickRecognizerRef recognizer);

//!   @} // end addtogroup Clicks
//! @} // end addtogroup UI
