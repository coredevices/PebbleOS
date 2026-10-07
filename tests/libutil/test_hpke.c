/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/util/hpke.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "clar.h"

#include "stubs_passert.h"

// Stubs
///////////////////////////////////////////////////////////
int g_pbl_log_level = 0;
void pbl_log(const char *src_filename, int src_line_number, const char *fmt, ...) {
}

// pebble_mbedtls_config.h maps mbed TLS's allocator to these; back them with libc.
void *kernel_calloc(size_t count, size_t size) {
  return calloc(count, size);
}
void kernel_free(void *ptr) {
  free(ptr);
}

// hpke.c pulls the ECC coordinate-blinding RNG from here. Export never generates a
// secret, so failing is fine — the blinding falls back to a deterministic stream and
// the DH result is unchanged.
bool rng_rand(uint32_t *rand_out) {
  return false;
}

// Helpers
///////////////////////////////////////////////////////////
static uint8_t prv_nibble(char c) {
  if (c >= '0' && c <= '9')
    return (uint8_t)(c - '0');
  if (c >= 'a' && c <= 'f')
    return (uint8_t)(c - 'a' + 10);
  if (c >= 'A' && c <= 'F')
    return (uint8_t)(c - 'A' + 10);
  cl_fail("bad hex");
  return 0;
}

static size_t prv_unhex(uint8_t *out, size_t out_cap, const char *hex) {
  size_t n = strlen(hex) / 2;
  cl_assert(n <= out_cap);
  for (size_t i = 0; i < n; i++) {
    out[i] = (uint8_t)((prv_nibble(hex[2 * i]) << 4) | prv_nibble(hex[2 * i + 1]));
  }
  return n;
}

// RFC 9180 Appendix A.3 — DHKEM(P-256, HKDF-SHA256), HKDF-SHA256, AES-128-GCM, base mode.
// This exercises the interop-critical path: DHKEM(P-256) decap + base-mode key schedule
// + HPKE Export. Our production suite is AES-256-GCM and hpke.h no longer defines the
// AES-128-GCM id, but Export is HKDF-only (it never runs the AEAD cipher) and the
// exporter_secret depends on the AEAD only through the suite id, so passing
// (HPKEAeadId)0x0001 reproduces A.3's suite id and its published exported_values.
#define A3_AEAD_ID ((HPKEAeadId)0x0001)

static const char *k_skRm = "f3ce7fdae57e1a310d87f1ebbde6f328be0a99cdbcadf4d6589cf29de4b8ffd2";
static const char *k_enc =
    "04a92719c6195d5085104f469a8b9814d5838ff72b60501e2c4466e5e67b325a"
    "c98536d7b61a1af4b78e5b7f951c0900be863c403ce65c9bfcb9382657222d18c4";
static const char *k_info = "4f6465206f6e2061204772656369616e2055726e";

// Tests
///////////////////////////////////////////////////////////
static void prv_check_export(const char *ctx_hex, const char *expected_hex) {
  uint8_t skRm[32], enc[65], info[32], ctx[64], expected[64], out[64];
  size_t skRm_len = prv_unhex(skRm, sizeof(skRm), k_skRm);
  size_t enc_len = prv_unhex(enc, sizeof(enc), k_enc);
  size_t info_len = prv_unhex(info, sizeof(info), k_info);
  size_t ctx_len = prv_unhex(ctx, sizeof(ctx), ctx_hex);
  size_t exp_len = prv_unhex(expected, sizeof(expected), expected_hex);

  cl_assert_equal_i(skRm_len, 32);
  cl_assert_equal_i(enc_len, 65);

  HPKEResult r = hpke_open_export(HPKEKemP256HkdfSha256, A3_AEAD_ID, skRm, skRm_len, enc, enc_len,
                                  info, info_len, ctx, ctx_len, out, exp_len);
  cl_assert_equal_i(r, HPKEOk);
  cl_assert_equal_m(out, expected, exp_len);
}

// exporter_context = "" (empty)
void test_hpke__rfc9180_a3_export_empty_context(void) {
  prv_check_export("", "5e9bc3d236e1911d95e65b576a8a86d478fb827e8bdfe77b741b289890490d4d");
}

// exporter_context = 0x00
void test_hpke__rfc9180_a3_export_single_byte_context(void) {
  prv_check_export("00", "6cff87658931bda83dc857e6353efe4987a201b849658d9b047aab4cf216e796");
}

// exporter_context = "TestContext"
void test_hpke__rfc9180_a3_export_string_context(void) {
  prv_check_export("54657374436f6e74657874",
                   "d8f1ea7942adbba7412c6d431c62d01371ea476b823eb697e1f6e6cae1dab85a");
}

// A wrong recipient key must not reproduce the exported value.
void test_hpke__wrong_key_differs(void) {
  uint8_t skRm[32], enc[65], info[32], expected[32], out[32];
  prv_unhex(skRm, sizeof(skRm), k_skRm);
  prv_unhex(enc, sizeof(enc), k_enc);
  size_t info_len = prv_unhex(info, sizeof(info), k_info);
  prv_unhex(expected, sizeof(expected),
            "5e9bc3d236e1911d95e65b576a8a86d478fb827e8bdfe77b741b289890490d4d");
  skRm[0] ^= 0x01; // flip one bit of the private scalar
  HPKEResult r = hpke_open_export(HPKEKemP256HkdfSha256, A3_AEAD_ID, skRm, sizeof(skRm), enc,
                                  sizeof(enc), info, info_len, NULL, 0, out, sizeof(out));
  // Either it fails, or it succeeds with a different secret — never the A.3 value.
  if (r == HPKEOk) {
    cl_assert(memcmp(out, expected, sizeof(out)) != 0);
  }
}
