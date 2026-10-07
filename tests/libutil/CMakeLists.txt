# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_circular_buffer)

pbl_clar_test(test_list)

pbl_clar_test(test_slist)

pbl_clar_test(test_math)

pbl_clar_test(test_string)

pbl_clar_test(test_math_fixed)

pbl_clar_test(test_uuid)

pbl_clar_test(test_circular_cache)

pbl_clar_test(test_keyed_circular_cache)

pbl_clar_test(test_struct)

pbl_clar_test(test_sort)

pbl_clar_test(test_byteorder)

pbl_clar_test(test_bitops)

pbl_clar_test(test_bits)

pbl_clar_test(test_misc)

pbl_clar_test(test_ratio)

pbl_clar_test(test_base64)

pbl_clar_test(test_buffer)

pbl_clar_test(test_string_list)

pbl_clar_test(test_pstring)

pbl_clar_test(test_generic_attr)

pbl_clar_test(test_stats)

pbl_clar_test(test_shared_cbuf)

pbl_clar_test(test_time)

# HPKE (RFC 9180) recipient-side Export, checked against the RFC 9180 A.3 vectors.
# Links the mbed TLS primitives hpke.c uses, built for the host with the firmware's
# mbed TLS config.
pbl_clar_test(test_hpke
  SOURCES
    lib/util/hpke.c
    third_party/mbedtls/mbedtls/library/aes.c
    third_party/mbedtls/mbedtls/library/bignum.c
    third_party/mbedtls/mbedtls/library/bignum_core.c
    third_party/mbedtls/mbedtls/library/constant_time.c
    third_party/mbedtls/mbedtls/library/ecp.c
    third_party/mbedtls/mbedtls/library/ecp_curves.c
    third_party/mbedtls/mbedtls/library/gcm.c
    third_party/mbedtls/mbedtls/library/cipher.c
    third_party/mbedtls/mbedtls/library/cipher_wrap.c
    third_party/mbedtls/mbedtls/library/hkdf.c
    third_party/mbedtls/mbedtls/library/md.c
    third_party/mbedtls/mbedtls/library/platform.c
    third_party/mbedtls/mbedtls/library/platform_util.c
    third_party/mbedtls/mbedtls/library/sha256.c
  DEFINES
    CONFIG_MBEDTLS
    MBEDTLS_CONFIG_FILE="pebble_mbedtls_config.h"
  INCLUDES
    third_party/mbedtls/mbedtls/include
    third_party/mbedtls/include
)
