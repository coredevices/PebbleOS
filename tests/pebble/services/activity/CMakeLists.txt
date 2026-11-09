# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_activity
  MODULES settings time
  SOURCES
    pebble/applib/health_service.c
    pebble/services/activity/activity.c
    pebble/services/activity/activity_metrics.c
    pebble/services/activity/activity_sessions.c
    pebble/services/activity/activity_calculators.c
    pebble/services/activity/hr_util.c
    pebble/shell/normal/prefs.c
    tests/stubs/stubs_system_resource.c
    tests/fakes/fake_accel_service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  DEFINES
    DUMA_DISABLED
    CAPABILITY_HAS_HEALTH_TRACKING=1
    CONFIG_HRM
    CONFIG_SERVICE_ACTIVITY_BATCH_SAMPLES=25
    CONFIG_SERVICE_LIGHT_DARK_THRESHOLD=0
    CONFIG_SERVICE_ACCEL_MANAGER_DEFAULT_SENSITIVITY=55
  OVERRIDES dummy_board
)

pbl_clar_test(test_activity_insights
  MODULES time rand
  SOURCES
    pebble/services/activity/activity_insights.c
    pebble/services/activity/insights_settings.c
    pebble/services/timeline/attribute.c
    pebble/services/timeline/metricgroup.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_settings.c
    tests/fakes/fake_kernel_services_notifications.c
    tests/fakes/fake_rtc.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_kraepelin_algorithm
  SOURCES
    tests/fakes/fake_rtc.c
    tests/pebble/services/activity/step_samples.c
    tests/pebble/services/activity/sleep_samples_v1.c
    tests/pebble/services/activity/kraepelin_reference/fourier.c
    tests/pebble/services/activity/kraepelin_reference/helper_worker.c
    tests/pebble/services/activity/kraepelin_reference/ProjectK_worker.c
    tests/pebble/services/activity/kraepelin_reference/raw_stats.c
  LIBS libkraepelin
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
)

pbl_clar_test(test_activity_algorithm_kraepelin
  MODULES settings time
  SOURCES
    pebble/services/activity/kraepelin/activity_algorithm_kraepelin.c
    tests/fakes/fake_accel_service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
  DEFINES
    DUMA_DISABLED
    CONFIG_HRM
    CONFIG_SERVICE_ACTIVITY_SLEEP_MAX_MINUTE_SCORE=330
    CONFIG_SERVICE_ACTIVITY_SLEEP_MIN_VALID_VMC=20
    CONFIG_SERVICE_ACTIVITY_SLEEP_MAX_AVG_VMC=180
    CONFIG_SERVICE_ACTIVITY_NOT_WORN_MIN_WORN_VMC=4
    CONFIG_SERVICE_ACTIVITY_NOT_WORN_MAX_LOW_VMC_RUN_M=180
  OVERRIDES dummy_board
)

pbl_clar_test(test_health_util
  SOURCES
    pebble/services/activity/health_util.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_hr_util
  SOURCES
    pebble/services/activity/hr_util.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_workout_service
  SOURCES
    pebble/services/activity/activity_calculators.c
    pebble/services/activity/hr_util.c
    pebble/services/activity/workout_service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_HRM
  OVERRIDES dummy_board
)

pbl_clar_test(test_activity_calculators
  SOURCES
    pebble/services/activity/activity_calculators.c
  OVERRIDES dummy_board
)
