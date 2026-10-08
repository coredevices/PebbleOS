# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_protobuf_log
  SOURCES
    fw/services/protobuf_log/protobuf_log.c
    fw/services/protobuf_log/protobuf_log_hr.c
    fw/services/protobuf_log/protobuf_log_activity_sessions.c
    fw/services/protobuf_log/protobuf_log_test.c
    fw/services/protobuf_log/protobuf_log_util.c
    fw/services/clock/time.c
    tests/fakes/fake_rtc.c
    tests/fw/services/protobuf_log/protobuf_log_test_helpers.c
  LIBS nanopb protobuf_log_schemas
)
