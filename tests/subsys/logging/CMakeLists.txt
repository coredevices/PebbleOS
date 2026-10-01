# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_pulse_logging
  SOURCES
    subsys/logging/pulse_logging.c
)

pbl_clar_test(test_log_modules
  SOURCES
    subsys/logging/logging.c
    fw/applib/logging.c
    tests/subsys/logging/runtime_module_peer.c
    tests/subsys/logging/static_module_peer.c
  DEFINES CONFIG_PULSE_EVERYWHERE=1 CONFIG_DEFAULT_LOG_LEVEL=100
          CONFIG_TEST_RUNTIME_LOG_LEVEL=100 CONFIG_TEST_RUNTIME_LOG_LEVEL_RUNTIME=1
          vsniprintf=vsnprintf
)
