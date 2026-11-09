# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_alerts_preferences
  MODULES settings
  SOURCES
    pebble/services/notifications/alerts_preferences.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_notification_storage
  MODULES pfs timeline_item time rand
  SOURCES
    pebble/services/notifications/notification_storage.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_ancs_notifications
  MODULES timeline_item time
  SOURCES
    pebble/services/notifications/ancs/ancs_notifications.c
    pebble/services/notifications/ancs/ancs_notifications_util.c
    pebble/services/notifications/ancs/ancs_item.c
    pebble/services/notifications/ancs/ancs_phone_call.c
    pebble/services/phone_call/util.c
    pebble/applib/fonts/codepoint.c
    pebble/applib/graphics/utf8.c
    tests/fakes/fake_events.c
)

pbl_clar_test(test_ancs_notifications_util
  MODULES time
  SOURCES
    pebble/services/notifications/ancs/ancs_notifications_util.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_ancs_filtering
  MODULES time
  SOURCES
    pebble/services/notifications/ancs/ancs_filtering.c
    pebble/services/timeline/attribute.c
    pebble/services/timeline/attributes_actions.c
    pebble/services/timeline/attribute_group.c
)

pbl_clar_test(test_nexmo
  MODULES time
  SOURCES
    pebble/services/notifications/ancs/nexmo.c
    pebble/services/notifications/ancs/ancs_notifications_util.c
    pebble/services/timeline/attribute.c
    pebble/services/timeline/attributes_actions.c
    pebble/services/timeline/attribute_group.c
    tests/fakes/fake_rtc.c
)
