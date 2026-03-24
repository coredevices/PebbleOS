/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  MfgTestId_Buttons,
  MfgTestId_Display,
#ifdef CONFIG_TOUCH
  MfgTestId_Touch,
#endif
  MfgTestId_Backlight,
  MfgTestId_Accel,
#ifdef CONFIG_MAG
  MfgTestId_Mag,
#endif
#if PLATFORM_ASTERIX || PLATFORM_OBELIX
  MfgTestId_Speaker,
#endif
#if PLATFORM_ASTERIX || PLATFORM_OBELIX || PLATFORM_GETAFIX
  MfgTestId_Mic,
#endif
  MfgTestId_ALS,
  MfgTestId_Vibration,
#if PLATFORM_OBELIX && defined(MANUFACTURING_FW)
  MfgTestId_HrmCtrLeakage,
#endif
  MfgTestId_ProgramColor,
  MfgTestId_Adv,
  MfgTestId_Aging,
  MfgTestId_Charge,
  MfgTestId_Discharge,

  MfgTestIdCount
} MfgTestId;

typedef struct {
  bool ran;
  bool passed;
  uint32_t value;
} MfgTestResult;

//! Report the result of a test. Can be called from any test app.
void mfg_test_result_report(MfgTestId test, bool passed, uint32_t value);

//! Get the result of a test. Returns NULL if the test has not been run.
const MfgTestResult *mfg_test_result_get(MfgTestId test);
