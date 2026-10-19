# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_ancs_pebble_actions
  MODULES settings timeline_item time rand
  SOURCES
    fw/applib/ui/preferred_durations.c
    fw/comm/ble/kernel_le_client/ancs/ancs_util.c
    fw/services/blob_db/ios_notif_pref_db.c
    fw/services/notifications/ancs/ancs_filtering.c
    fw/services/notifications/ancs/ancs_item.c
    fw/services/notifications/ancs/ancs_notifications.c
    fw/services/notifications/ancs/ancs_notifications_util.c
    fw/services/notifications/ancs/ancs_phone_call.c
    fw/services/timeline/actions_endpoint.c
    fw/services/timeline/timeline.c
    fw/services/timeline/timeline_actions.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_timeline_actions
  MODULES timeline_item time
  SOURCES
    fw/applib/ui/preferred_durations.c
    fw/services/timeline/actions_endpoint.c
    fw/services/timeline/timeline.c
    fw/services/timeline/timeline_actions.c
  OVERRIDES dummy_board
)
