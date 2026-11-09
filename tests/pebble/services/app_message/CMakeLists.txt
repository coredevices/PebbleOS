# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_app_message_receiver
  SOURCES
    pebble/applib/app_inbox.c
    pebble/applib/app_message/app_message_receiver.c
    pebble/services/app_message/app_message_receiver.c
    pebble/services/app_inbox_service/service.c
  OVERRIDES applib_malloc
)

pbl_clar_test(test_app_message_sender
  SOURCES
    pebble/services/app_message/app_message_sender.c
    subsys/pp/send_queue.c
)

pbl_clar_test(test_app_session_capabilities
  SOURCES
    pebble/services/app_session/capabilities.c
)
