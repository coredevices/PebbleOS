# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_console_frame
  SOURCES
    fw/console/console_frame.c
    lib/util/base64.c
    subsys/crc/crc32.c
)
