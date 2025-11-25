/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/dialogs/dialog.h"
#include "applib/ui/window_stack.h"

//! Initializes the dialog.
//! @param dialog Pointer to a \ref Dialog to initialize
//! @param dialog_name The debug name to give the dialog
void dialog_init(Dialog *dialog, const char *dialog_name);

//! Pushes the dialog onto the window stack.
//! @param dialog Pointer to a \ref Dialog to push
//! @param window_stack Pointer to a \ref WindowStack to push the \ref Dialog to
void dialog_push(Dialog *dialog, WindowStack *window_stack);

//! Wrapper to call \ref dialog_push() for an application
//! @note: Put a better comment here when we export
void app_dialog_push(Dialog *dialog);

//! Pops the dialog off the window stack.
//! @param dialog Pointer to a \ref Dialog to push
void dialog_pop(Dialog *dialog);

//! This function is called by each type of dialog's load functions to execute common dialog code.
//! @param dialog Pointer to a \ref Dialog to load
void dialog_load(Dialog *dialog);

//! Displays the icon by playing the kino layer
//! @param dialog Pointer to the \ref Dialog to appear
void dialog_appear(Dialog *dialog);

//! This function is called by each type of dialog's unload function. The dialog_context is the
//! the dialog object being unloaded.
//! @param dialog Pointer to a \ref Dialog to unload
void dialog_unload(Dialog *dialog);

//! Draw the status layer on the dialog.
//! @param dialog Pointer to a \ref Dialog to draw the status layer on
//! @param status_layer_frame The frame of the status layer
void dialog_add_status_bar_layer(Dialog *dialog, const GRect *status_layer_frame);

//! Create the icon for the dialog.
//! @param dialog Pointer to a \ref Dialog from which to grab the icon id
//! @return the \ref KinoReel for the dialog's icon
KinoReel *dialog_create_icon(Dialog *dialog);

//! Initialize the dialog's icon layer with the provided image and frame origin.
//! @param dialog Pointer to \ref Dialog from which to initialize it's \ref KinoLayer
//! @param image Pointer to a \ref KinoReel to put on the dialog's \ref KinoLayer
//! @param icon_origin The starting point for the icon, if it is being animated,
//!    this is the point it will animate to.
//! @param animated `True` if animated, otherwise `False`
//! @return `True` if successfully initialized the dialog's \ref KinoLayer, otherwise
//!     `False`
bool dialog_init_icon_layer(Dialog *dialog, KinoReel *image,
                            GPoint icon_origin, bool animated);
