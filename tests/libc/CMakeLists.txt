# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_mktime
  SOURCES
    lib/c/time/gmtime.c
    lib/c/time/mktime.c
)

pbl_clar_test(test_gmtime
  SOURCES
    lib/c/time/gmtime.c
    lib/c/time/mktime.c
)
