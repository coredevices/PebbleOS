/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "recognizer.h"

#include "applib/graphics/gtypes.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct TapRecognizerData TapRecognizerData;

//! Create a tap recognizer. The default recognizer recognizes a single tap from a single finger
//! @param event_cb event callback
//! @param user_data user data associated with recognizer
//! @return recognizer reference
Recognizer *tap_recognizer_create(RecognizerEventCb event_cb, void *user_data);

//! Get the tap recognizer data from a recognizer. Should be used in the event callback to get the
//! the data for a tap recognizer event
//! @param recognizer recognizer from which to get data
//! @return \ref TapRecognizerData reference
const TapRecognizerData *tap_recognizer_get_data(const Recognizer *recognizer);

// TODO: Add configuration methods & getters for state
// https://pebbletechnology.atlassian.net/browse/PBL-28983
