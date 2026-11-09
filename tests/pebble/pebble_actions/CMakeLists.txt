# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_ancs_pebble_actions
  MODULES settings timeline_item time rand
  SOURCES
    pebble/applib/ui/preferred_durations.c
    pebble/comm/ble/kernel_le_client/ancs/ancs_util.c
    pebble/services/blob_db/ios_notif_pref_db.c
    pebble/services/notifications/ancs/ancs_filtering.c
    pebble/services/notifications/ancs/ancs_item.c
    pebble/services/notifications/ancs/ancs_notifications.c
    pebble/services/notifications/ancs/ancs_notifications_util.c
    pebble/services/notifications/ancs/ancs_phone_call.c
    pebble/services/timeline/actions_endpoint.c
    pebble/services/timeline/timeline.c
    pebble/services/timeline/timeline_actions.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_timeline_actions
  MODULES timeline_item time
  SOURCES
    pebble/applib/ui/preferred_durations.c
    pebble/services/timeline/actions_endpoint.c
    pebble/services/timeline/timeline.c
    pebble/services/timeline/timeline_actions.c
  OVERRIDES dummy_board
)
