# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_alerts_preferences
  MODULES settings
  SOURCES
    fw/services/notifications/alerts_preferences.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_notification_storage
  MODULES pfs timeline_item time rand
  SOURCES
    fw/services/notifications/notification_storage.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_ancs_notifications
  MODULES timeline_item time
  SOURCES
    fw/services/notifications/ancs/ancs_notifications.c
    fw/services/notifications/ancs/ancs_notifications_util.c
    fw/services/notifications/ancs/ancs_item.c
    fw/services/notifications/ancs/ancs_phone_call.c
    fw/services/phone_call/util.c
    fw/applib/fonts/codepoint.c
    fw/applib/graphics/utf8.c
    tests/fakes/fake_events.c
)

pbl_clar_test(test_ancs_notifications_util
  MODULES time
  SOURCES
    fw/services/notifications/ancs/ancs_notifications_util.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_ancs_filtering
  MODULES time
  SOURCES
    fw/services/notifications/ancs/ancs_filtering.c
    fw/services/timeline/attribute.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
)

pbl_clar_test(test_nexmo
  MODULES time
  SOURCES
    fw/services/notifications/ancs/nexmo.c
    fw/services/notifications/ancs/ancs_notifications_util.c
    fw/services/timeline/attribute.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
    tests/fakes/fake_rtc.c
)
