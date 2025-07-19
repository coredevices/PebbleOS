/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bluetooth/bt_test.h"
#include "console/prompt.h"
#include "drivers/button.h"
#include "drivers/flash.h"
#include "drivers/i2c.h"
#include "drivers/imu.h"
#include "drivers/imu/bmi160/bmi160.h"
#include "util/bitset.h"
#include "util/size.h"

#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

struct SelfTestCase {
  char name[16];
  bool (*func)(void);
};

// TODO: PBL-34018 This file is a mess. We should clean it up to better chose the right functions
// and list of test cases depending on the capabilities of the platform.

// Here's a clever trick: selftest functions which may or may not be
// linked into the firmware depending on the ./waf configure settings
// (read: IMU) are redeclared as weak so that it is not a linker error
// to have missing definitions for these functions. They simply link as
// zero (null pointer). This works out perfectly as the selftest code
// considers a null function pointer to mean not-implemented, which is
// exactly the outcome we want!
bool bmi160_query_whoami(void) WEAK;
bool bma255_query_whoami(void) WEAK;
bool flash_check_whoami(void) WEAK;
bool accel_manager_run_selftest(void) WEAK;
bool gyro_manager_run_selftest(void) WEAK;
bool mag3110_check_whoami(void) WEAK;
bool snowy_mag3110_query_whoami(void) WEAK;
bool mic_selftest(void) WEAK;

// NULL function pointer means test is not implemented
static const struct SelfTestCase s_test_cases[] = {
#if PLATFORM_SILK
  { "Accel Comm", bma255_query_whoami },
#else
  { "IMU Comm", bmi160_query_whoami },
#endif
  { "Accel ST", accel_manager_run_selftest },
#if !PLATFORM_SILK
  { "Gyro ST", gyro_manager_run_selftest },
  { "MAG3110 Comm", mag3110_check_whoami },
#endif
#if CAPABILITY_HAS_APPLE_MFI
  { "Apple ACP I2C", bt_driver_test_mfi_chip_selftest },
#endif
  { "BT Module", bt_driver_test_selftest },
  { "Flash Comm", flash_check_whoami },
  { "Mic", mic_selftest },
  { "Buttons", button_selftest },
};

static char* bool_to_pass_fail(bool b) {
  if (b) {
    return "PASS";
  } else {
    return "FAIL";
  }
}

//! Runs all the test cases
//! @return a bitset of tests that passed or failed
uint32_t mfg_selftest(void) {
  uint32_t result = 0;
  for (uint32_t i = 0; i < ARRAY_LENGTH(s_test_cases); i++) {
    const struct SelfTestCase* test = s_test_cases + i;
    bool test_passed = false;
    if (test->func != 0) {
      test_passed = test->func();
    }
    bitset32_update(&result, i, test_passed);
  }
  return result;
}

void command_selftest(void) {
  char buffer[32];
  uint32_t result = mfg_selftest();
  for (uint32_t i = 0; i < ARRAY_LENGTH(s_test_cases); i++) {
    const struct SelfTestCase* test = s_test_cases + i;
    char *pass_fail;
    if (test->func != 0) {  // Test is implemented
      pass_fail = bool_to_pass_fail(bitset32_get(&result, i));
    } else {
      pass_fail = "NYI";
    }
    prompt_send_response_fmt(buffer, 32, "%15s: %s", test->name, pass_fail);
  }
}
