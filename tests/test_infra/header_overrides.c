/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "_test/default_only.h"
#include "_test/default_and_custom.h"
#include "_test/custom_only.h"

// Loading of override headers must be done in a separate source file because
// the overrides are not applied to the test harness itself.

int default_only_define(void) {
  return DEFAULT_ONLY_DEFINE;
}

int overridden_define(void) {
  return OVERRIDDEN_DEFINE;
}

int custom_only_define(void) {
  return CUSTOM_ONLY_DEFINE;
}
