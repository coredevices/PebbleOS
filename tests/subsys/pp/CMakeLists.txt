# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_session
  SOURCES
    subsys/pp/session.c
    subsys/pp/send_queue.c
  INCLUDES subsys/pp
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_PP_LOG_LEVEL=0
)

pbl_clar_test(test_receive
  SOURCES
    subsys/pp/session.c
    subsys/pp/send_queue.c
    subsys/pp/receive.c
  INCLUDES subsys/pp
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_PP_LOG_LEVEL=0
)

pbl_clar_test(test_send_queue
  SOURCES
    subsys/pp/send_queue.c
  INCLUDES subsys/pp
  DEFINES
    CONFIG_PP_LOG_LEVEL=0
)

pbl_clar_test(test_send_buffer
  SOURCES
    subsys/pp/send_buffer.c
    subsys/pp/send_queue.c
    tests/fakes/fake_rtc.c
  INCLUDES subsys/pp
  DEFINES
    CONFIG_PP_LOG_LEVEL=0
)

pbl_clar_test(test_version
  SOURCES
    subsys/pp/version.c
    tests/fakes/fake_events.c
  INCLUDES subsys/pp
  DEFINES
    CONFIG_PP_LOG_LEVEL=0
)

pbl_clar_test(test_kernel_receiver
  SOURCES
    subsys/pp/kernel_receiver.c
  INCLUDES subsys/pp
  DEFINES
    CONFIG_PP_LOG_LEVEL=0
)

pbl_clar_test(test_meta
  SOURCES
    subsys/pp/meta.c
    tests/fakes/fake_session.c
  INCLUDES subsys/pp
  DEFINES
    CONFIG_PP_LOG_LEVEL=0
)
