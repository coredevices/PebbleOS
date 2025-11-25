/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "test_jerry_port_common.h"
#include "test_rocky_common.h"

#include "applib/rockyjs/api/rocky_api_global.h"
#include "applib/rockyjs/api/rocky_api_preferences.h"
#include "applib/rockyjs/pbl_jerry_port.h"

#include <string.h>
#include <applib/preferred_content_size.h>

// Fakes
#include "fake_app_timer.h"
#include "fake_time.h"

// Stubs
#include "stubs_app_state.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_serial.h"
#include "stubs_sys_exit.h"

////////////////////////////////////////////////////////////////////////////////
// Fakes / Stubs
////////////////////////////////////////////////////////////////////////////////

static PreferredContentSize s_preferred_content_size;
PreferredContentSize preferred_content_size(void) {
  return s_preferred_content_size;
}

static const RockyGlobalAPI *s_preferences_api[] = {
  &PREFERENCES_APIS,
  NULL,
};

void test_rocky_api_preferences__initialize(void) {
  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_EMPTY);
  s_preferred_content_size = PreferredContentSizeMedium;
}

void test_rocky_api_preferences__cleanup(void) {
  if (app_state_get_rocky_runtime_context() != NULL) {
    jerry_cleanup();
    rocky_runtime_context_deinit();
  }
}

void test_rocky_api_preferences__unknown(void) {
  s_preferred_content_size = (PreferredContentSize) -1;
  rocky_global_init(s_preferences_api);

  EXECUTE_SCRIPT("var size = _rocky.userPreferences.contentSize");
  ASSERT_JS_GLOBAL_EQUALS_S("size", "medium");
}

void test_rocky_api_preferences__always_valid(void) {
  s_preferred_content_size = NumPreferredContentSizes;
  rocky_global_init(s_preferences_api);

  EXECUTE_SCRIPT("var size = _rocky.userPreferences.contentSize");
  ASSERT_JS_GLOBAL_EQUALS_S("size", "medium");
}

void test_rocky_api_preferences__small(void) {
  s_preferred_content_size = PreferredContentSizeSmall;
  rocky_global_init(s_preferences_api);

  EXECUTE_SCRIPT("var size = _rocky.userPreferences.contentSize");
  ASSERT_JS_GLOBAL_EQUALS_S("size", "small");
}

void test_rocky_api_preferences__medium(void) {
  s_preferred_content_size = PreferredContentSizeMedium;
  rocky_global_init(s_preferences_api);

  EXECUTE_SCRIPT("var size = _rocky.userPreferences.contentSize");
  ASSERT_JS_GLOBAL_EQUALS_S("size", "medium");
}

void test_rocky_api_preferences__large(void) {
  s_preferred_content_size = PreferredContentSizeLarge;
  rocky_global_init(s_preferences_api);

  EXECUTE_SCRIPT("var size = _rocky.userPreferences.contentSize");
  ASSERT_JS_GLOBAL_EQUALS_S("size", "large");
}

void test_rocky_api_preferences__extra_large(void) {
  s_preferred_content_size = PreferredContentSizeExtraLarge;
  rocky_global_init(s_preferences_api);

  EXECUTE_SCRIPT("var size = _rocky.userPreferences.contentSize");
  ASSERT_JS_GLOBAL_EQUALS_S("size", "x-large");
}
