/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "zbt_ctkd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// pebble_mbedtls_config.h allocates from the kernel heap.
void *kernel_calloc(size_t count, size_t size) {
  return calloc(count, size);
}

void kernel_free(void *ptr) {
  free(ptr);
}

// Core Vol 3 Part H Appendix D, h6 and h7 sample data, little-endian.
static const uint8_t s_w[16] = {0x9b, 0x7d, 0x39, 0x0a, 0xa6, 0x10, 0x10, 0x34,
                                0x05, 0xad, 0xc8, 0x57, 0xa3, 0x34, 0x02, 0xec};

void test_zbt_ctkd__h6_matches_spec_sample(void) {
  const uint8_t key_id[4] = {0x72, 0x62, 0x65, 0x6c};
  const uint8_t expect[16] = {0x99, 0x63, 0xb1, 0x80, 0xe2, 0xa9, 0xd3, 0xe8,
                              0x1c, 0xc9, 0x6d, 0xe7, 0x02, 0xe1, 0x9a, 0x2d};
  uint8_t out[16];
  cl_assert_equal_i(zbt_ctkd_h6(s_w, key_id, out), 0);
  cl_assert(memcmp(out, expect, sizeof(expect)) == 0);
}

void test_zbt_ctkd__h7_matches_spec_sample(void) {
  const uint8_t salt[16] = {0x31, 0x70, 0x6d, 0x74};
  const uint8_t expect[16] = {0x11, 0x70, 0xa5, 0x75, 0x2a, 0x8c, 0x99, 0xd2,
                              0xec, 0xc0, 0xa3, 0xc6, 0x97, 0x35, 0x17, 0xfb};
  uint8_t out[16];
  cl_assert_equal_i(zbt_ctkd_h7(salt, s_w, out), 0);
  cl_assert(memcmp(out, expect, sizeof(expect)) == 0);
}

// The whole derivation, with the sample W as the LTK; values from an independent
// AES-CMAC implementation.
void test_zbt_ctkd__link_key_without_ct2(void) {
  const uint8_t expect[16] = {0xd7, 0xf6, 0x2c, 0xc1, 0x8d, 0x9a, 0x95, 0x36,
                              0xdf, 0x54, 0x55, 0x8c, 0x8d, 0xd9, 0xaa, 0xac};
  uint8_t lk[16];
  cl_assert_equal_i(zbt_ctkd_link_key(s_w, false, lk), 0);
  cl_assert(memcmp(lk, expect, sizeof(expect)) == 0);
}

void test_zbt_ctkd__link_key_with_ct2(void) {
  const uint8_t expect[16] = {0xf5, 0xaf, 0x70, 0x38, 0x26, 0x9c, 0xf2, 0x53,
                              0x85, 0x91, 0x68, 0x08, 0xa2, 0xd4, 0x6e, 0x28};
  uint8_t lk[16];
  cl_assert_equal_i(zbt_ctkd_link_key(s_w, true, lk), 0);
  cl_assert(memcmp(lk, expect, sizeof(expect)) == 0);
}

// LTKs and link keys, all in HCI order, from AOSP's crypto_toolbox (checked against D.9 and
// D.10): {LTK, link key without CT2 (h6), link key with CT2 (h7)}.
static const char *const s_aosp_vectors[][3] = {
  {"64bf4f33336c06bd584b26e3bcf98d36", "b08f38eeaf30820dbdc13f63efa41cbc",
   "35b84730f4f1390a5302a4dc79d37a28"},
  {"000102030405060708090a0b0c0d0e0f", "bdd4495e6a0a43c316a5b46437dbf24e",
   "20086c675e7d66acd1c705bbf9e2cfab"},
  {"ffeeddccbbaa99887766554433221100", "4147cde571a0b41b9cc9f465cc90e150",
   "ab2d1cd301444632b44a700ca2801fd7"},
  {"a55a1032547698badcfe0123456789ab", "cd384c06db37e74c5f5d71ff027df648",
   "46b5065ca35b467099bee31a06f8e881"},
};

// The string's bytes, in the order written.
static void prv_hex(const char *hex, uint8_t out[16]) {
  for (int i = 0; i < 16; i++) {
    unsigned int byte;
    cl_assert(sscanf(&hex[2 * i], "%2x", &byte) == 1);
    out[i] = (uint8_t)byte;
  }
}

void test_zbt_ctkd__link_key_matches_aosp(void) {
  for (size_t i = 0; i < sizeof(s_aosp_vectors) / sizeof(s_aosp_vectors[0]); i++) {
    uint8_t ltk[16], expect_h6[16], expect_h7[16], lk[16];
    prv_hex(s_aosp_vectors[i][0], ltk);
    prv_hex(s_aosp_vectors[i][1], expect_h6);
    prv_hex(s_aosp_vectors[i][2], expect_h7);
    cl_assert_equal_i(zbt_ctkd_link_key(ltk, false, lk), 0);
    cl_assert(memcmp(lk, expect_h6, sizeof(lk)) == 0);
    cl_assert_equal_i(zbt_ctkd_link_key(ltk, true, lk), 0);
    cl_assert(memcmp(lk, expect_h7, sizeof(lk)) == 0);
  }
}
