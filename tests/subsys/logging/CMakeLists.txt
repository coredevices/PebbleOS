# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_log_flash
  SOURCES
    subsys/logging/backends/flash_storage.c
    tests/fakes/fake_spi_flash.c
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
)

pbl_clar_test(test_log_modules
  SOURCES
    subsys/logging/logging.c
    subsys/logging/frontend.c
    tests/subsys/logging/runtime_module_peer.c
    tests/subsys/logging/static_module_peer.c
  DEFINES CONFIG_DEFAULT_LOG_LEVEL=100 CONFIG_LOG_BOOT_BUFFER_SIZE=256
          CONFIG_TEST_RUNTIME_LOG_LEVEL=100 CONFIG_TEST_RUNTIME_LOG_LEVEL_RUNTIME=1
          vsniprintf=vsnprintf PBL_NO_LINKER_SCRIPT
)
