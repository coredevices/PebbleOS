# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_base64
  SOURCES
    fw/util/base64.c
)

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

pbl_clar_test(test_dict
  SOURCES
    fw/util/dict.c
)

pbl_clar_test(test_buffer
  SOURCES
    fw/util/buffer.c
)

pbl_clar_test(test_lru_cache
  SOURCES
    fw/util/lru_cache.c
)

pbl_clar_test(test_mbuf
  SOURCES
    fw/util/mbuf.c
    fw/util/mbuf_iterator.c
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

pbl_clar_test(test_generic_attribute
  SOURCES
    fw/util/rand/rand.c
    fw/util/generic_attribute.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
)

pbl_clar_test(test_hdlc
  SOURCES
    fw/util/hdlc.c
)

pbl_clar_test(test_stringlist
  SOURCES
    fw/util/stringlist.c
)

pbl_clar_test(test_pstring
  SOURCES
    fw/util/pstring.c
)

pbl_clar_test(test_ihex
  SOURCES
    fw/util/ihex.c
)

pbl_clar_test(test_sle
  SOURCES
    fw/util/sle.c
)
