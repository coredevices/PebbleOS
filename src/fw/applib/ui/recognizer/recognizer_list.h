/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/list.h"

#include <stdbool.h>
#include <stdint.h>

struct Recognizer;

typedef struct RecognizerList {
  ListNode *node;
} RecognizerList;

//! Recognizer iterator callback function
//! @return true if iteration should continue after returning
typedef bool (*RecognizerListIteratorCb)(struct Recognizer *recognizer, void *context);

//! Initialize recognizer list
void recognizer_list_init(RecognizerList *list);

//! Iterate over a recognizer list. It is safe to remove a recognizer from the list (and destroy it)
//! from within the iterator callback
//! @param list \ref RecognizerList to iterate over
//! @param iter_cb iterator callback
//! @param context optional iterator context
//! @return true if iteration through all recognizers in the list completed, false otherwise
bool recognizer_list_iterate(RecognizerList *list, RecognizerListIteratorCb iter_cb, void *context);
