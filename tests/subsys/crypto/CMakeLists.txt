# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(mbedtls_sources
  third_party/mbedtls/mbedtls/library/aes.c
  third_party/mbedtls/mbedtls/library/platform_util.c
  third_party/mbedtls/mbedtls/library/sha256.c
)
set(mbedtls_includes
  third_party/mbedtls/mbedtls/include
  third_party/mbedtls/include
)
set(mbedtls_defines "MBEDTLS_CONFIG_FILE=\"pebble_mbedtls_config.h\"")

pbl_clar_test(test_sha256
  SOURCES
    subsys/crypto/sha256.c
    ${mbedtls_sources}
  INCLUDES
    ${mbedtls_includes}
  DEFINES
    ${mbedtls_defines}
)

pbl_clar_test(test_sha256_hw
  SOURCES
    subsys/crypto/sha256.c
    ${mbedtls_sources}
  INCLUDES
    ${mbedtls_includes}
  DEFINES
    ${mbedtls_defines}
    CONFIG_CRYPTO_SHA256_HW_ACCEL=1
    CONFIG_CRYPTO_SHA256_HW_MIN_LEN=128
    CONFIG_CRYPTO_LOG_LEVEL=0
)

pbl_clar_test(test_aes
  SOURCES
    subsys/crypto/aes.c
    ${mbedtls_sources}
  INCLUDES
    ${mbedtls_includes}
  DEFINES
    ${mbedtls_defines}
)

pbl_clar_test(test_aes_hw
  SOURCES
    subsys/crypto/aes.c
    ${mbedtls_sources}
  INCLUDES
    ${mbedtls_includes}
  DEFINES
    ${mbedtls_defines}
    CONFIG_CRYPTO_AES_HW_ACCEL=1
    CONFIG_CRYPTO_AES_HW_MIN_LEN=32
    CONFIG_CRYPTO_LOG_LEVEL=0
)

# The self-test against a fake accelerator that is correct, wrong for SHA-256, or not ready
foreach(mode ok wrong absent)
  pbl_clar_test(test_crypto_selftest_${mode}
    SOURCE test_crypto_selftest.c
    SOURCES
      subsys/crypto/aes.c
      subsys/crypto/selftest.c
      subsys/crypto/sha256.c
      ${mbedtls_sources}
    INCLUDES
      ${mbedtls_includes}
    DEFINES
      ${mbedtls_defines}
      CONFIG_CRYPTO_SHA256_HW_ACCEL=1
      CONFIG_CRYPTO_SHA256_HW_MIN_LEN=64
      CONFIG_CRYPTO_AES_HW_ACCEL=1
      CONFIG_CRYPTO_AES_HW_MIN_LEN=32
      CONFIG_CRYPTO_SELFTEST=1
      CONFIG_CRYPTO_LOG_LEVEL=0
      FAKE_HW_${mode}=1
  )
endforeach()
