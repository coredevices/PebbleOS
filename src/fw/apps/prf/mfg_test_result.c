/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/prf/mfg_test_result.h"

#define NUM_MODES 2

static MfgTestResult s_results[NUM_MODES][MfgTestIdCount];
static bool s_result_reported;
static uint8_t s_mode_index;

void mfg_test_result_set_mode(uint8_t mode) {
  // Map mode bitmask to array index: semi-finished=0, finished=1
  s_mode_index = (mode == MFG_TEST_MODE_FINISHED) ? 1 : 0;
}

void mfg_test_result_report(MfgTestId test, bool passed, uint32_t value) {
  if (test >= MfgTestIdCount) {
    return;
  }

  s_results[s_mode_index][test] = (MfgTestResult) {
    .ran = true,
    .passed = passed,
    .value = value,
  };
  s_result_reported = true;
}

const MfgTestResult *mfg_test_result_get(MfgTestId test) {
  if (test >= MfgTestIdCount) {
    return NULL;
  }

  return &s_results[s_mode_index][test];
}

bool mfg_test_result_was_reported(void) {
  bool reported = s_result_reported;
  s_result_reported = false;
  return reported;
}
