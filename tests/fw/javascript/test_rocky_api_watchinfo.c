/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "test_jerry_port_common.h"
#include "test_rocky_common.h"

#include "applib/rockyjs/api/rocky_api_global.h"
#include "applib/rockyjs/api/rocky_api_watchinfo.h"
#include "applib/rockyjs/pbl_jerry_port.h"

#include "applib/app_watch_info.h"
#include "system/version.h"

#include <string.h>

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

#define TEST_LOCALE "test_locale"
#define VERSION_PREFIX "v4.0"
#define VERSION_SUFFIX "beta5"
#define VERSION_TAG VERSION_PREFIX"-"VERSION_SUFFIX
#define VERSION_MAJOR 4
#define VERSION_MINOR 0
#define VERSION_PATCH 122

char *app_get_system_locale(void) {
  return TEST_LOCALE;
}

bool version_copy_running_fw_metadata(FirmwareMetadata *out_metadata) {
  strncpy(out_metadata->version_tag, VERSION_TAG, FW_METADATA_VERSION_TAG_BYTES);
  return true;
}

WatchInfoVersion watch_info_get_firmware_version(void) {
  return (WatchInfoVersion) {
    .major = VERSION_MAJOR,
    .minor = VERSION_MINOR,
    .patch = VERSION_PATCH
  };
}

static WatchInfoColor s_watch_info_color;
WatchInfoColor sys_watch_info_get_color(void) {
  return s_watch_info_color;
}

static WatchInfoModel s_watch_info_model;
WatchInfoModel watch_info_get_model(void) {
  return s_watch_info_model;
}

static PlatformType s_current_app_sdk_platform;
PlatformType sys_get_current_app_sdk_platform(void) {
  return s_current_app_sdk_platform;
}


static const RockyGlobalAPI *s_watchinfo_api[] = {
  &WATCHINFO_APIS,
  NULL,
};

void test_rocky_api_watchinfo__initialize(void) {
  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_EMPTY);
  s_watch_info_model = WATCH_INFO_MODEL_PEBBLE_TIME_STEEL;
  s_watch_info_color = WATCH_INFO_COLOR_TIME_STEEL_GOLD;
  s_current_app_sdk_platform = PlatformTypeBasalt;
}

void test_rocky_api_watchinfo__cleanup(void) {
  if (app_state_get_rocky_runtime_context() != NULL) {
    jerry_cleanup();
    rocky_runtime_context_deinit();
  }
}

void test_rocky_api_watchinfo__model(void) {
  rocky_global_init(s_watchinfo_api);

  EXECUTE_SCRIPT("var model = _rocky.watchInfo.model");
  ASSERT_JS_GLOBAL_EQUALS_S("model", "pebble_time_steel_gold");
}

void test_rocky_api_watchinfo__qemu_model(void) {
  s_watch_info_color = -1;
  rocky_global_init(s_watchinfo_api);

  EXECUTE_SCRIPT("var model = _rocky.watchInfo.model");
  ASSERT_JS_GLOBAL_EQUALS_S("model", "qemu_platform_basalt");
}

void test_rocky_api_watchinfo__language(void) {
  rocky_global_init(s_watchinfo_api);

  EXECUTE_SCRIPT("var language = _rocky.watchInfo.language");
  ASSERT_JS_GLOBAL_EQUALS_S("language", TEST_LOCALE);
}

void test_rocky_api_watchinfo__platform(void) {
  rocky_global_init(s_watchinfo_api);

  EXECUTE_SCRIPT("var platform = _rocky.watchInfo.platform");
  ASSERT_JS_GLOBAL_EQUALS_S("platform", "basalt");
}

void test_rocky_api_watchinfo__platform_unknown(void) {
  s_current_app_sdk_platform = -1; // Some unknown / invalid
  rocky_global_init(s_watchinfo_api);

  EXECUTE_SCRIPT("var platform = _rocky.watchInfo.platform");
  ASSERT_JS_GLOBAL_EQUALS_S("platform", "unknown");
}

void test_rocky_api_watchinfo__fw_version(void) {
  rocky_global_init(s_watchinfo_api);

  EXECUTE_SCRIPT("var major = _rocky.watchInfo.firmware.major");
  ASSERT_JS_GLOBAL_EQUALS_I("major", VERSION_MAJOR);

  EXECUTE_SCRIPT("var minor = _rocky.watchInfo.firmware.minor");
  ASSERT_JS_GLOBAL_EQUALS_I("minor", VERSION_MINOR);

  EXECUTE_SCRIPT("var patch = _rocky.watchInfo.firmware.patch");
  ASSERT_JS_GLOBAL_EQUALS_I("patch", VERSION_PATCH);

  EXECUTE_SCRIPT("var suffix = _rocky.watchInfo.firmware.suffix");
  ASSERT_JS_GLOBAL_EQUALS_S("suffix", VERSION_SUFFIX);
}
