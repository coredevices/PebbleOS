/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/prf/mfg_test_result.h"

static MfgTestResult s_results[MfgTestIdCount];
static bool s_result_reported;

void mfg_test_result_report(MfgTestId test, bool passed, uint32_t value) {
  if (test >= MfgTestIdCount) {
    return;
  }

  s_results[test] = (MfgTestResult) {
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

  return &s_results[test];
}

bool mfg_test_result_was_reported(void) {
  bool reported = s_result_reported;
  s_result_reported = false;
  return reported;
}
