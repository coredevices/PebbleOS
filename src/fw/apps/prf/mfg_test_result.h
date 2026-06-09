/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Test mode bitmask
#define MFG_TEST_MODE_SEMI_FINISHED (1 << 0)
#define MFG_TEST_MODE_FINISHED      (1 << 1)
#define MFG_TEST_MODE_ALL           (MFG_TEST_MODE_SEMI_FINISHED | MFG_TEST_MODE_FINISHED)

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
#if defined(CONFIG_BOARD_FAMILY_ASTERIX) || defined(CONFIG_BOARD_FAMILY_OBELIX)
  MfgTestId_Speaker,
#endif
#if defined(CONFIG_BOARD_FAMILY_ASTERIX) || defined(CONFIG_BOARD_FAMILY_OBELIX) || defined(CONFIG_BOARD_FAMILY_GETAFIX)
  MfgTestId_Mic,
#endif
  MfgTestId_ALS,
  MfgTestId_Vibration,
#if defined(CONFIG_BOARD_FAMILY_OBELIX) && defined(CONFIG_MFG)
  MfgTestId_HrmCtrLeakage,
#endif
  MfgTestId_ProgramColor,
  MfgTestId_Charge,
  MfgTestId_Aging,

  MfgTestIdCount
} MfgTestId;

typedef struct {
  bool ran;
  bool passed;
  uint32_t value;
} MfgTestResult;

//! Set the active test mode (MFG_TEST_MODE_SEMI_FINISHED or
//! MFG_TEST_MODE_FINISHED). Results are stored separately per mode.
void mfg_test_result_set_mode(uint8_t mode);

//! Report the result of a test. Can be called from any test app.
void mfg_test_result_report(MfgTestId test, bool passed, uint32_t value);

//! Get the result of a test. Returns NULL if the test has not been run.
const MfgTestResult *mfg_test_result_get(MfgTestId test);

//! Check if a result was reported since the last call to this function.
//! Returns true once, then resets. Used by the test menu to detect test
//! completion vs user backing out.
bool mfg_test_result_was_reported(void);

//! Clear all stored results. In manufacturing firmware this also erases the
//! MFG_RESULTS flash region.
void mfg_test_result_reset(void);
