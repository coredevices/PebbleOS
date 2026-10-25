/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "zbt_ctkd.h"

#include <stddef.h>

#include <mbedtls/cipher.h>
#include <mbedtls/cmac.h>

// keyIDs and SALT, little-endian: "tmp1" is 0x746D7031, "lebr" 0x6C656272.
static const uint8_t s_tmp1[4] = {0x31, 0x70, 0x6d, 0x74};
static const uint8_t s_lebr[4] = {0x72, 0x62, 0x65, 0x6c};
static const uint8_t s_salt[16] = {0x31, 0x70, 0x6d, 0x74};

static void prv_swap(uint8_t *dst, const uint8_t *src, size_t len) {
  for (size_t i = 0; i < len; i++) {
    dst[i] = src[len - 1 - i];
  }
}

// The spec's functions take and give big-endian values.
static int prv_cmac_le(const uint8_t key[16], const uint8_t *msg, size_t len, uint8_t out[16]) {
  uint8_t key_be[16];
  uint8_t msg_be[16];
  uint8_t out_be[16];
  prv_swap(key_be, key, sizeof(key_be));
  prv_swap(msg_be, msg, len);
  const int rc = mbedtls_cipher_cmac(mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB),
                                     key_be, 128, msg_be, len, out_be);
  if (rc == 0) {
    prv_swap(out, out_be, sizeof(out_be));
  }
  return rc;
}

int zbt_ctkd_h6(const uint8_t w[16], const uint8_t key_id[4], uint8_t out[16]) {
  return prv_cmac_le(w, key_id, 4, out);
}

int zbt_ctkd_h7(const uint8_t salt[16], const uint8_t w[16], uint8_t out[16]) {
  return prv_cmac_le(salt, w, 16, out);
}

int zbt_ctkd_link_key(const uint8_t ltk[16], bool ct2, uint8_t link_key[16]) {
  uint8_t ilk[16];
  const int rc = ct2 ? zbt_ctkd_h7(s_salt, ltk, ilk) : zbt_ctkd_h6(ltk, s_tmp1, ilk);
  if (rc != 0) {
    return rc;
  }
  return zbt_ctkd_h6(ilk, s_lebr, link_key);
}
