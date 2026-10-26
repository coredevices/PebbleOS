/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/util/size.h>
#include <pbl/util/uuid.h>

#include <applib/app_install.h>
#include <applib/app_launch_reason.h>
#include <clar.h>
#include <process_management/app_install_manager.h>
#include <process_management/app_manager.h>
#include <shell/normal/watchface.h>

enum {
  IdHiddenFace = -2,
  IdAlarms = 1,
  IdQuiet = -101,
  IdTictoc = -69,
  IdKickstart = -98,
  IdCompanion = 2,
};

static const Uuid s_hidden_face_uuid = UuidMake(0xe9, 0x47, 0x52, 0x44, 0x5b, 0xbe, 0x4e, 0x0f,
                                                0xa6, 0x37, 0xa2, 0x18, 0xaf, 0x4c, 0x31, 0x10);
static const Uuid s_alarms_uuid = UuidMake(0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
                                           0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x01);
static const Uuid s_quiet_uuid = UuidMake(0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
                                          0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x02);
static const Uuid s_tictoc_uuid = UuidMake(0x8f, 0x3c, 0x86, 0x86, 0x31, 0xa1, 0x4f, 0x5f, 0x91,
                                           0xf5, 0x01, 0x60, 0x0c, 0x9b, 0xdc, 0x59);
static const Uuid s_kickstart_uuid = UuidMake(0x3a, 0xf8, 0x58, 0xc3, 0x16, 0xcb, 0x45, 0x61, 0x91,
                                              0xe7, 0xf1, 0xad, 0x2d, 0xf8, 0x72, 0x5f);
static const Uuid s_companion_uuid = UuidMake(0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
                                              0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x03);
static const Uuid s_missing_uuid = UuidMake(0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
                                            0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10);

static AppInstallEntry s_entries[6];

enum {
  StepSetDefault = 1,
  StepLaunch,
};

static int s_steps[4];
static int s_num_steps;
static AppInstallId s_default_id;
static AppLaunchEventConfig s_launch;
static const AppInstallId s_unchanged_default = 42;

static void prv_entry(AppInstallEntry *entry, AppInstallId id, ProcessType type,
                      ProcessVisibility visibility, const Uuid *uuid, const char *name) {
  *entry = (AppInstallEntry){
    .install_id = id,
    .process_type = type,
    .visibility = visibility,
    .uuid = *uuid,
  };
  strncpy(entry->name, name, sizeof(entry->name) - 1);
}

void test_app_install__initialize(void) {
  s_num_steps = 0;
  memset(s_steps, 0, sizeof(s_steps));
  s_default_id = s_unchanged_default;
  s_launch = (AppLaunchEventConfig){};

  prv_entry(&s_entries[0], IdHiddenFace, ProcessTypeWatchface, ProcessVisibilityHidden,
            &s_hidden_face_uuid, "Secret");
  prv_entry(&s_entries[1], IdAlarms, ProcessTypeApp, ProcessVisibilityShown, &s_alarms_uuid,
            "Alarms");
  prv_entry(&s_entries[2], IdQuiet, ProcessTypeApp, ProcessVisibilityQuickLaunch, &s_quiet_uuid,
            "Quiet");
  prv_entry(&s_entries[3], IdTictoc, ProcessTypeWatchface, ProcessVisibilityShown, &s_tictoc_uuid,
            "TicToc");
  prv_entry(&s_entries[4], IdKickstart, ProcessTypeWatchface, ProcessVisibilityShown,
            &s_kickstart_uuid, "Kickstart");
  prv_entry(&s_entries[5], IdCompanion, ProcessTypeApp, ProcessVisibilityShownOnCommunication,
            &s_companion_uuid, "Companion");
}

void app_install_enumerate_entries(AppInstallEnumerateCb cb, void *data) {
  for (unsigned int i = 0; i < ARRAY_LENGTH(s_entries); i++) {
    if (!cb(&s_entries[i], data)) {
      return;
    }
  }
}

bool app_install_entry_is_watchface(const AppInstallEntry *entry) {
  return entry->process_type == ProcessTypeWatchface;
}

bool app_install_entry_is_hidden(const AppInstallEntry *entry) {
  switch (entry->visibility) {
    case ProcessVisibilityHidden:
    case ProcessVisibilityQuickLaunch:
    case ProcessVisibilityShownOnCommunication:
      return true;
    case ProcessVisibilityShown:
      return false;
  }
  return false;
}

AppInstallId app_install_get_id_for_uuid(const Uuid *uuid) {
  if (uuid == nullptr) {
    return INSTALL_ID_INVALID;
  }
  for (unsigned int i = 0; i < ARRAY_LENGTH(s_entries); i++) {
    if (uuid_equal(uuid, &s_entries[i].uuid)) {
      return s_entries[i].install_id;
    }
  }
  return INSTALL_ID_INVALID;
}

bool app_install_get_entry_for_install_id(AppInstallId install_id, AppInstallEntry *entry) {
  for (unsigned int i = 0; i < ARRAY_LENGTH(s_entries); i++) {
    if (s_entries[i].install_id == install_id) {
      *entry = s_entries[i];
      return true;
    }
  }
  return false;
}

void watchface_set_default_install_id(AppInstallId app_id) {
  s_default_id = app_id;
  s_steps[s_num_steps++] = StepSetDefault;
}

void app_manager_put_launch_app_event(const AppLaunchEventConfig *config) {
  s_launch = *config;
  s_steps[s_num_steps++] = StepLaunch;
}

void test_app_install__enumerate(void) {
  cl_assert_equal_i(app_install_count(), 3);

  AppInstallInfo info = {};
  cl_assert(app_install_get_at_index(0, &info));
  cl_assert_equal_s(info.name, "Alarms");
  cl_assert(!info.is_watchface);
  cl_assert(uuid_equal(&info.uuid, &s_alarms_uuid));

  cl_assert(app_install_get_at_index(1, &info));
  cl_assert_equal_s(info.name, "TicToc");
  cl_assert(info.is_watchface);
  cl_assert(uuid_equal(&info.uuid, &s_tictoc_uuid));

  cl_assert(app_install_get_at_index(2, &info));
  cl_assert_equal_s(info.name, "Kickstart");
  cl_assert(info.is_watchface);

  cl_assert(!app_install_get_at_index(3, &info));
  cl_assert_equal_s(info.name, "Kickstart");
}

void test_app_install__launch_missing(void) {
  cl_assert(!app_launch(&s_missing_uuid));
  cl_assert(!app_launch(nullptr));
  cl_assert_equal_i(s_num_steps, 0);
  cl_assert_equal_i(s_default_id, s_unchanged_default);
}

void test_app_install__launch_does_not_change_default(void) {
  cl_assert(app_launch(&s_kickstart_uuid));
  cl_assert_equal_i(s_num_steps, 1);
  cl_assert_equal_i(s_steps[0], StepLaunch);
  cl_assert_equal_i(s_launch.id, IdKickstart);
  cl_assert_equal_i(s_launch.common.reason, APP_LAUNCH_SYSTEM);
  cl_assert_equal_i(s_default_id, s_unchanged_default);

  s_num_steps = 0;
  cl_assert(app_launch(&s_hidden_face_uuid));
  cl_assert_equal_i(s_launch.id, IdHiddenFace);
  cl_assert_equal_i(s_default_id, s_unchanged_default);
}

void test_app_install__select_rejects_non_watchface(void) {
  cl_assert(!watchface_select(&s_alarms_uuid));
  cl_assert(!watchface_select(&s_missing_uuid));
  cl_assert(!watchface_select(nullptr));
  cl_assert_equal_i(s_num_steps, 0);
  cl_assert_equal_i(s_default_id, s_unchanged_default);
}

void test_app_install__select_rejects_hidden_watchface(void) {
  cl_assert(!watchface_select(&s_hidden_face_uuid));
  cl_assert_equal_i(s_num_steps, 0);
  cl_assert_equal_i(s_default_id, s_unchanged_default);
}

void test_app_install__select_persists_default(void) {
  cl_assert(watchface_select(&s_kickstart_uuid));
  cl_assert_equal_i(s_num_steps, 2);
  cl_assert_equal_i(s_steps[0], StepSetDefault);
  cl_assert_equal_i(s_steps[1], StepLaunch);
  cl_assert_equal_i(s_default_id, IdKickstart);
  cl_assert_equal_i(s_launch.id, IdKickstart);
  cl_assert_equal_i(s_launch.common.reason, APP_LAUNCH_SYSTEM);
}
