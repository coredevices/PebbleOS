# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_shared_circular_buffer
  SOURCES
    fw/util/shared_circular_buffer.c
)

pbl_clar_test(test_time
  SOURCES
    fw/util/time/time.c
    fw/util/time/mktime.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_rand
  SOURCES
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
)

pbl_clar_test(test_stats
  SOURCES
    fw/util/stats.c
)

pbl_clar_test(test_legacy_checksum
  SOURCES
    fw/util/legacy_checksum.c
)

