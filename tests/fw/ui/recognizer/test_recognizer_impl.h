/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/recognizer/recognizer.h"

#include <stdbool.h>
#include <stdint.h>

#define NEW_RECOGNIZER(r) \
  Recognizer *r __attribute__ ((__cleanup__(test_recognizer_destroy)))

typedef struct TestImplData {
  int test;
  bool *destroyed;
  bool *cancelled;
  bool *reset;
  bool *handled;
  bool *updated;
  bool *failed;
  TouchEvent *last_touch_event;
  RecognizerState *new_state;
} TestImplData;

Recognizer *test_recognizer_create(TestImplData *test_impl_data, void *user_data);

void test_recognizer_destroy(Recognizer **recognizer);

void test_recognizer_enable_on_destroy(void);
