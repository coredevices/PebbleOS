# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_protobuf_log
  MODULES time
  SOURCES
    pebble/services/protobuf_log/protobuf_log.c
    pebble/services/protobuf_log/protobuf_log_hr.c
    pebble/services/protobuf_log/protobuf_log_activity_sessions.c
    pebble/services/protobuf_log/protobuf_log_test.c
    pebble/services/protobuf_log/protobuf_log_util.c
    tests/fakes/fake_rtc.c
    tests/pebble/services/protobuf_log/protobuf_log_test_helpers.c
  LIBS nanopb protobuf_log_schemas
)
