# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_activity
  MODULES settings time
  SOURCES
    fw/applib/health_service.c
    fw/services/regular_timer/service.c
    fw/services/activity/activity.c
    fw/services/activity/activity_metrics.c
    fw/services/activity/activity_sessions.c
    fw/services/activity/activity_calculators.c
    fw/services/activity/hr_util.c
    fw/shell/normal/prefs.c
    tests/stubs/stubs_system_resource.c
    tests/fakes/fake_accel_service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  DEFINES DUMA_DISABLED CAPABILITY_HAS_HEALTH_TRACKING=1 CONFIG_SERVICE_HRM CONFIG_SERVICE_ACTIVITY_BATCH_SAMPLES=25
  OVERRIDES dummy_board
)

pbl_clar_test(test_activity_insights
  MODULES time rand
  SOURCES
    fw/services/activity/activity_insights.c
    fw/services/activity/insights_settings.c
    fw/services/timeline/attribute.c
    fw/services/timeline/metricgroup.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_settings_file.c
    tests/fakes/fake_kernel_services_notifications.c
    tests/fakes/fake_rtc.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_kraepelin_algorithm
  SOURCES
    fw/services/activity/kraepelin/kraepelin_algorithm.c
    tests/fakes/fake_rtc.c
    tests/fw/services/activity/step_samples.c
    tests/fw/services/activity/sleep_samples_v1.c
    tests/fw/services/activity/kraepelin_reference/fourier.c
    tests/fw/services/activity/kraepelin_reference/helper_worker.c
    tests/fw/services/activity/kraepelin_reference/ProjectK_worker.c
    tests/fw/services/activity/kraepelin_reference/raw_stats.c
  DEFINES DUMA_DISABLED CONFIG_SERVICE_HRM
  OVERRIDES dummy_board
)

pbl_clar_test(test_activity_algorithm_kraepelin
  MODULES settings time
  SOURCES
    fw/services/regular_timer/service.c
    fw/services/activity/kraepelin/activity_algorithm_kraepelin.c
    tests/fakes/fake_accel_service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
)

pbl_clar_test(test_health_util
  SOURCES
    fw/services/activity/health_util.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_hr_util
  SOURCES
    fw/services/activity/hr_util.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_workout_service
  SOURCES
    fw/services/activity/activity_calculators.c
    fw/services/activity/hr_util.c
    fw/services/activity/workout_service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_SERVICE_HRM
  OVERRIDES dummy_board
)

pbl_clar_test(test_activity_calculators
  SOURCES
    fw/services/activity/activity_calculators.c
  OVERRIDES dummy_board
)
