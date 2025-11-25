/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/rockyjs/rocky_res.h"
#include "process_management/process_manager.h"
#include "process_management/app_install_manager.h"
#include "process_management/pebble_process_info.h"
#include "util/size.h"

// Stubs
#include "stubs_accel_service.h"
#include "stubs_analytics.h"
#include "stubs_analytics_external.h"
#include "stubs_animation_service.h"
#include "stubs_app_cache.h"
#include "stubs_app_manager.h"
#include "stubs_app_state.h"
#include "stubs_dls.h"
#include "stubs_evented_timer.h"
#include "stubs_expandable_dialog.h"
#include "stubs_freertos.h"
#include "stubs_heap.h"
#include "stubs_i18n.h"
#include "stubs_logging.h"
#include "stubs_modal_manager.h"
#include "stubs_new_timer.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_process_md.h"
#include "stubs_pebble_tasks.h"
#include "stubs_persist.h"
#include "stubs_queue.h"
#include "stubs_resources.h"
#include "stubs_syscalls.h"
#include "stubs_task.h"
#include "stubs_tick.h"
#include "stubs_watchface.h"
#include "stubs_worker_manager.h"
#include "stubs_worker_state.h"

char __APP_RAM__[1024*128];
char *__APP_RAM_end__ = &__APP_RAM__[1024*128];
char __WORKER_RAM__[1024*12];
char *__WORKER_RAM_end__ = &__APP_RAM__[1024*12];

typedef struct {
  AppInstallEntry entry;
  bool should_pass;
} AppInstallEntryTestCase;

static AppInstallEntryTestCase s_test_cases[] = {
  {
    .entry = (AppInstallEntry) {
      .install_id = 1,
      .sdk_version = (Version) {
        .major = PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR,
        .minor = PROCESS_INFO_CURRENT_SDK_VERSION_MINOR
      }
    },
    .should_pass = true
  }, {
    .entry = (AppInstallEntry) {
      .install_id = 2,
      .sdk_version = (Version) {
        .major = PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR - 1,
        .minor = PROCESS_INFO_CURRENT_SDK_VERSION_MINOR
      }
    },
    .should_pass = false
  }, {
    .entry = (AppInstallEntry) {
      .install_id = 3,
      .sdk_version = (Version) {
        .major = PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR + 1,
        .minor = PROCESS_INFO_CURRENT_SDK_VERSION_MINOR
      }
    },
    .should_pass = false
  }, {
    .entry = (AppInstallEntry) {
      .install_id = 4,
      .sdk_version = (Version) {
        .major = PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR,
        .minor = PROCESS_INFO_CURRENT_SDK_VERSION_MINOR - 10
      }
    },
    .should_pass = true
  }, {
    .entry = (AppInstallEntry) {
      .install_id = 5,
      .sdk_version = (Version) {
        .major = PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR,
        .minor = PROCESS_INFO_CURRENT_SDK_VERSION_MINOR + 10
      }
    },
    .should_pass = false
  }, {
    .entry = (AppInstallEntry) {
      .install_id = 6,
      .sdk_version = (Version) {
        .major = PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR + 1,
        .minor = PROCESS_INFO_CURRENT_SDK_VERSION_MINOR + 10
      }
    },
    .should_pass = false
  }, {
    .entry = (AppInstallEntry) {
      .install_id = 7,
      .sdk_version = (Version) {
        .major = PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR - 1,
        .minor = PROCESS_INFO_CURRENT_SDK_VERSION_MINOR - 10
      }
    },
    .should_pass = false
  }
};

PlatformType process_metadata_get_app_sdk_platform(const PebbleProcessMd *md) {
  cl_fail("should not be called");
  return (PlatformType)-1;
}

UBaseType_t uxQueueMessagesWaiting(const QueueHandle_t xQueue) {
  return 0;
}

BaseType_t event_queue_cleanup_and_reset(QueueHandle_t queue) {
  return pdPASS;
}

void event_service_clear_process_subscriptions(void) {
}

bool app_install_entry_is_watchface(const AppInstallEntry *entry) {
  return false;
}

AppInstallId app_install_get_id_for_uuid(const Uuid *uuid) {
  return 1;
}

bool app_install_get_entry_for_install_id(AppInstallId install_id, AppInstallEntry *entry) {
  *entry = s_test_cases[install_id - 1].entry;
  return true;
}

bool app_install_id_from_app_db(AppInstallId id) {
  return (id > INSTALL_ID_INVALID);
}

bool app_install_entry_is_SDK_compatible(const AppInstallEntry *entry) {
  return (entry->sdk_version.major == PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR &&
          entry->sdk_version.minor <= PROCESS_INFO_CURRENT_SDK_VERSION_MINOR);
}

static PebbleProcessMd *s_app_install_get_md__result;
const PebbleProcessMd *app_install_get_md(AppInstallId id, bool worker) {
  return s_app_install_get_md__result;
}

void app_install_release_md(const PebbleProcessMd *md) {
}

static int s_process_metadata_get_res_bank_num__result;
int process_metadata_get_res_bank_num(const PebbleProcessMd *md) {
  return s_process_metadata_get_res_bank_num__result;
}

static RockyResourceValidation s_rocky_app_validate_resources__result;
RockyResourceValidation rocky_app_validate_resources(const PebbleProcessMd *md) {
  return s_rocky_app_validate_resources__result;
}

static PebbleEvent* s_event_put__event;
void event_put(PebbleEvent* event) {
  s_event_put__event = event;
  cl_assert(event != NULL);
}

void event_put_from_app(PebbleEvent* event) { cl_fail("unexpected"); }
void event_put_from_process(PebbleTask task, PebbleEvent* event) { cl_fail("unexpected"); }
void event_reset_from_process_queue(PebbleTask task) { cl_fail("unexpected"); }


void test_process_manager__initialize(void) {
  s_app_install_get_md__result = NULL;
  s_process_metadata_get_res_bank_num__result = 123;
  s_rocky_app_validate_resources__result = RockyResourceValidation_NotRocky;
  s_app_manager_launch_new_app__callcount = 0;
  s_app_manager_launch_new_app__config = (__typeof__(s_app_manager_launch_new_app__config)){};
  s_event_put__event = NULL;
}

void test_process_manager__check_SDK_compatible(void) {
  for (uint32_t i = 0; i < ARRAY_LENGTH(s_test_cases); i++) {
    // skipping 0, since it's INSTALL_ID_INVALID
    cl_assert_equal_b(process_manager_check_SDK_compatible(i + 1), s_test_cases[i].should_pass);
  }
}

void test_process_manager__launch_valid_rocky_app(void) {
  s_app_install_get_md__result = &(PebbleProcessMd){.is_rocky_app = true};
  s_rocky_app_validate_resources__result = RockyResourceValidation_Valid;
  process_manager_launch_process(&(ProcessLaunchConfig){.id=1});

  // app was launched, no events (especially no fetch event) on the queue
  cl_assert_equal_b(1, s_app_manager_launch_new_app__callcount);
  cl_assert_equal_p(s_app_install_get_md__result,
                    s_app_manager_launch_new_app__config.md);
  cl_assert(s_event_put__event == NULL);
}

void test_process_manager__launch_invalid_rocky_app(void) {
  s_app_install_get_md__result = &(PebbleProcessMd){.is_rocky_app = true};
  s_rocky_app_validate_resources__result = RockyResourceValidation_Invalid;
  process_manager_launch_process(&(ProcessLaunchConfig){.id=1});

  // app wasn't launched, instead we see a fetch request
  cl_assert_equal_b(0, s_app_manager_launch_new_app__callcount);
  cl_assert(s_event_put__event != NULL);
  cl_assert_equal_i(PEBBLE_APP_FETCH_REQUEST_EVENT, s_event_put__event->type);
}
