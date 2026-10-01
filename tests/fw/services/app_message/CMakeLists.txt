# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_app_message_receiver
  SOURCES
    fw/applib/app_inbox.c
    fw/applib/app_message/app_message_receiver.c
    fw/services/app_message/app_message_receiver.c
    fw/services/app_inbox_service/service.c
  OVERRIDES applib_malloc
)

pbl_clar_test(test_app_message_sender
  SOURCES
    fw/services/app_message/app_message_sender.c
    fw/services/comm_session/session_send_queue.c
)

pbl_clar_test(test_app_session_capabilities
  SOURCES
    fw/services/comm_session/app_session_capabilities.c
)
