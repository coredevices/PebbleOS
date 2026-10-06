/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

#include "pbl/util/hpke.h"

// HPKE requires mbed TLS. Every firmware that links this file also enables it
// (subsys/bluetooth Kconfig: CONFIG_BT is default-y and selects MBEDTLS), so a
// build without it is a configuration error — fail loudly rather than ship stubs.
#ifndef CONFIG_MBEDTLS
#error "hpke.c requires CONFIG_MBEDTLS"
#endif

#include <pbl/drivers/rng.h>

#include "mbedtls/ecp.h"
#include "mbedtls/gcm.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/md.h"

#include <string.h>

//! HKDF-SHA256 is KDF id 0x0001; both the shared secret and the SHA-256 hash
//! are 32 bytes (Nsecret, Nh). Every supported suite uses this KDF.
#define HPKE_KDF_ID  0x0001
#define HPKE_NSECRET 32
#define HPKE_NH      32
#define HPKE_NN      12 //!< AEAD nonce length, every supported AEAD.

//! Length of the DHKEM kem_context (enc || pkR): two 65-byte P-256 points.
#define HPKE_KEM_CONTEXT_MAX (2 * HPKE_P256_PUBLIC_KEY_BYTES)

// -----------------------------------------------------------------------------
// Two RNG uses, told apart by what a failure costs. prv_blinding_rng feeds only
// mbedtls_ecp_mul's coordinate blinding (a side-channel countermeasure; the DH
// result is the same whatever it returns), so with no TRNG it falls back to a
// deterministic xorshift — and the shared, non-atomic `counter` is thus a benign
// race. Key/nonce generation (prv_fill_secret / prv_hard_rng) fails hard instead,
// since a predictable secret is a real break.

static int prv_blinding_rng(void *ctx, unsigned char *out, size_t len) {
  (void)ctx;
  static uint32_t counter = 0x9e3779b9u;
  while (len > 0) {
    uint32_t word;
    if (!rng_rand(&word)) {
      counter ^= counter << 13;
      counter ^= counter >> 17;
      counter ^= counter << 5;
      word = counter;
    }
    size_t take = len < sizeof(word) ? len : sizeof(word);
    memcpy(out, &word, take);
    out += take;
    len -= take;
  }
  return 0;
}

static bool prv_fill_secret(uint8_t *out, size_t len) {
  while (len > 0) {
    uint32_t word;
    if (!rng_rand(&word)) {
      return false;
    }
    size_t take = len < sizeof(word) ? len : sizeof(word);
    memcpy(out, &word, take);
    out += take;
    len -= take;
  }
  return true;
}

//! Hard RNG for real key material: fails (nonzero) when there is no TRNG.
static int prv_hard_rng(void *ctx, unsigned char *out, size_t len) {
  (void)ctx;
  return prv_fill_secret(out, len) ? 0 : -1;
}

// -----------------------------------------------------------------------------
// P-256 (secp256r1), SEC1. Scalars are 32-byte big-endian; points are 65-byte
// uncompressed (0x04 || X || Y). DH is ECDH: the shared secret is the 32-byte X
// coordinate of scalar * peer.

//! The secp256r1 generator, uncompressed (0x04 || Gx || Gy). Hardcoded so
//! public-key derivation does not reach into the group struct's private generator
//! field.
static const uint8_t k_p256_generator[65] = {
  0x04, 0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6, 0xe5,
  0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0, 0xf4,
  0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96, 0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a,
  0x7f, 0x9b, 0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16, 0x2b, 0xce, 0x33,
  0x57, 0x6b, 0x31, 0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5
};

//! result_point = scalar * base_point, both 65-byte uncompressed points.
static bool prv_p256_mul(uint8_t out_point[65], const uint8_t scalar_be[32],
                         const uint8_t base_point[65]) {
  mbedtls_ecp_group grp;
  mbedtls_ecp_point base;
  mbedtls_ecp_point result;
  mbedtls_mpi scalar;
  bool ok = false;

  mbedtls_ecp_group_init(&grp);
  mbedtls_ecp_point_init(&base);
  mbedtls_ecp_point_init(&result);
  mbedtls_mpi_init(&scalar);

  // mbedtls_ecp_mul validates `base` is on-curve (mbedtls_ecp_check_pubkey); keep an
  // explicit check if this ever moves off mbedtls_ecp_mul (ecdh/PSA).
  if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) != 0 ||
      mbedtls_mpi_read_binary(&scalar, scalar_be, 32) != 0 ||
      mbedtls_ecp_point_read_binary(&grp, &base, base_point, 65) != 0 ||
      mbedtls_ecp_mul(&grp, &result, &scalar, &base, prv_blinding_rng, NULL) != 0) {
    goto cleanup;
  }
  size_t olen = 0;
  if (mbedtls_ecp_point_write_binary(&grp, &result, MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, out_point,
                                     65) != 0 ||
      olen != 65) {
    goto cleanup;
  }
  ok = true;

cleanup:
  mbedtls_mpi_free(&scalar);
  mbedtls_ecp_point_free(&result);
  mbedtls_ecp_point_free(&base);
  mbedtls_ecp_group_free(&grp);
  return ok;
}

static bool prv_p256_dh(uint8_t out_dh[32], const uint8_t *sk, const uint8_t *pk) {
  uint8_t point[65];
  if (!prv_p256_mul(point, sk, pk)) {
    return false;
  }
  memcpy(out_dh, point + 1, 32); // shared secret = X coordinate.
  mbedtls_platform_zeroize(point, sizeof(point));
  return true;
}

static bool prv_p256_public(uint8_t *out_pk, const uint8_t *sk) {
  return prv_p256_mul(out_pk, sk, k_p256_generator);
}

static bool prv_p256_gen_sk(uint8_t *out_sk) {
  mbedtls_ecp_group grp;
  mbedtls_mpi d;
  bool ok = false;
  mbedtls_ecp_group_init(&grp);
  mbedtls_mpi_init(&d);
  if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) == 0 &&
      mbedtls_ecp_gen_privkey(&grp, &d, prv_hard_rng, NULL) == 0 &&
      mbedtls_mpi_write_binary(&d, out_sk, 32) == 0) {
    ok = true;
  }
  mbedtls_mpi_free(&d);
  mbedtls_ecp_group_free(&grp);
  return ok;
}

// -----------------------------------------------------------------------------
// KEM abstraction: pick the DH primitive and its sizes by KEM id.

typedef struct {
  size_t nenc; //!< == Npk: length of `enc` and of a public key.
  size_t nsk;  //!< private-key length.
  bool (*derive_public)(uint8_t *out_pk, const uint8_t *sk);
  bool (*dh)(uint8_t out_dh[32], const uint8_t *sk, const uint8_t *pk);
  bool (*gen_sk)(uint8_t *out_sk);
} HPKEKem;

static const HPKEKem *prv_kem(HPKEKemId id) {
  static const HPKEKem p256 = {65, 32, prv_p256_public, prv_p256_dh, prv_p256_gen_sk};
  switch (id) {
    case HPKEKemP256HkdfSha256:
      return &p256;
    default:
      return NULL;
  }
}

// -----------------------------------------------------------------------------
// Labeled HKDF (RFC 9180 §4). The suite id ties every derivation to this exact
// ciphersuite, so a key schedule from one suite can never be mistaken for
// another's.

typedef struct {
  uint8_t bytes[10]; //!< "HPKE" + kem(2) + kdf(2) + aead(2)  — 10 bytes.
} HPKESuiteId;

static HPKESuiteId prv_suite_id(uint16_t kem_id, HPKEAeadId aead) {
  HPKESuiteId id = {
    {'H', 'P', 'K', 'E', (uint8_t)(kem_id >> 8), (uint8_t)kem_id, (uint8_t)(HPKE_KDF_ID >> 8),
     (uint8_t)HPKE_KDF_ID, (uint8_t)((uint16_t)aead >> 8), (uint8_t)aead}
  };
  return id;
}

//! The KEM suite id is "KEM" + kem(2) — 5 bytes — for the DHKEM derivations.
static void prv_kem_suite_id(uint16_t kem_id, uint8_t out[5]) {
  out[0] = 'K';
  out[1] = 'E';
  out[2] = 'M';
  out[3] = (uint8_t)(kem_id >> 8);
  out[4] = (uint8_t)kem_id;
}

static const char k_hpke_version[] = "HPKE-v1"; // 7 bytes, no terminator wanted.

//! Longest `info`/exporter-context an HPKE call here carries. The largest is the
//! AccessoryTransportSecurity exporter context "P256-Version1-<uuid>...-<dir>-<feature_id>"
//! with uuid and feature_id each up to 64 bytes (~159 bytes); 192 leaves margin. It also
//! covers the caller `info` ("P256-Version1-<uuid>", ~78 bytes).
#define HPKE_MAX_INFO_LEN 192

//! Scratch for the longest labeled_extract/expand input: I2OSP(2, expand only) +
//! version(7) + suite_id(<=10) + label(<=16) + the largest trailing bytes, which is
//! the greater of the P-256 kem_context (ExtractAndExpand) and HPKE_MAX_INFO_LEN.
#define HPKE_LABEL_MAX_TRAILER \
  (HPKE_KEM_CONTEXT_MAX > HPKE_MAX_INFO_LEN ? HPKE_KEM_CONTEXT_MAX : HPKE_MAX_INFO_LEN)
#define HPKE_LABEL_SCRATCH (2 + 7 + 10 + 16 + HPKE_LABEL_MAX_TRAILER)

static int prv_labeled_extract(const mbedtls_md_info_t *md, const uint8_t *salt, size_t salt_len,
                               const uint8_t *suite_id, size_t suite_id_len, const char *label,
                               const uint8_t *ikm, size_t ikm_len, uint8_t out_prk[HPKE_NH]) {
  // labeled_ikm = "HPKE-v1" || suite_id || label || ikm, then HKDF-Extract.
  uint8_t buf[HPKE_LABEL_SCRATCH];
  size_t label_len = strlen(label);
  size_t n = 0;
  if (7 + suite_id_len + label_len + ikm_len > sizeof(buf)) {
    return MBEDTLS_ERR_HKDF_BAD_INPUT_DATA;
  }
  memcpy(buf + n, k_hpke_version, 7);
  n += 7;
  memcpy(buf + n, suite_id, suite_id_len);
  n += suite_id_len;
  memcpy(buf + n, label, label_len);
  n += label_len;
  if (ikm_len) {
    memcpy(buf + n, ikm, ikm_len);
    n += ikm_len;
  }
  return mbedtls_hkdf_extract(md, salt, salt_len, buf, n, out_prk);
}

static int prv_labeled_expand(const mbedtls_md_info_t *md, const uint8_t prk[HPKE_NH],
                              const uint8_t *suite_id, size_t suite_id_len, const char *label,
                              const uint8_t *info, size_t info_len, uint8_t *out, size_t out_len) {
  // labeled_info = I2OSP(L,2) || "HPKE-v1" || suite_id || label || info.
  uint8_t buf[HPKE_LABEL_SCRATCH];
  size_t label_len = strlen(label);
  size_t n = 0;
  if (2 + 7 + suite_id_len + label_len + info_len > sizeof(buf)) {
    return MBEDTLS_ERR_HKDF_BAD_INPUT_DATA;
  }
  buf[n++] = (uint8_t)(out_len >> 8);
  buf[n++] = (uint8_t)out_len;
  memcpy(buf + n, k_hpke_version, 7);
  n += 7;
  memcpy(buf + n, suite_id, suite_id_len);
  n += suite_id_len;
  memcpy(buf + n, label, label_len);
  n += label_len;
  if (info_len) {
    memcpy(buf + n, info, info_len);
    n += info_len;
  }
  return mbedtls_hkdf_expand(md, prk, HPKE_NH, buf, n, out, out_len);
}

// -----------------------------------------------------------------------------
// DHKEM ExtractAndExpand: turn a raw DH result into the KEM shared secret.

static bool prv_extract_and_expand(const mbedtls_md_info_t *md, uint16_t kem_id,
                                   const uint8_t dh[32], const uint8_t *kem_context,
                                   size_t kem_context_len, uint8_t out_shared[HPKE_NSECRET]) {
  uint8_t kem_suite_id[5];
  prv_kem_suite_id(kem_id, kem_suite_id);
  uint8_t eae_prk[HPKE_NH];
  bool ok = prv_labeled_extract(md, NULL, 0, kem_suite_id, sizeof(kem_suite_id), "eae_prk", dh, 32,
                                eae_prk) == 0 &&
            prv_labeled_expand(md, eae_prk, kem_suite_id, sizeof(kem_suite_id), "shared_secret",
                               kem_context, kem_context_len, out_shared, HPKE_NSECRET) == 0;
  mbedtls_platform_zeroize(eae_prk, sizeof(eae_prk));
  return ok;
}

// -----------------------------------------------------------------------------
// Key schedule (base mode) -> exporter secret.

typedef struct {
  //! RFC 9180 §5.1 exporter_secret. We only use HPKE Export, so the AEAD key and
  //! base nonce the key schedule can also derive are not kept.
  uint8_t exporter_secret[HPKE_NH];
  HPKEKemId kem;
  HPKEAeadId aead;
} HPKEContext;

static bool prv_key_schedule(const mbedtls_md_info_t *md, uint16_t kem_id, HPKEAeadId aead,
                             const uint8_t shared_secret[HPKE_NSECRET], const uint8_t *info,
                             size_t info_len, HPKEContext *out) {
  HPKESuiteId suite = prv_suite_id(kem_id, aead);
  const uint8_t *suite_id = suite.bytes;
  const size_t suite_id_len = sizeof(suite.bytes);

  uint8_t psk_id_hash[HPKE_NH];
  uint8_t info_hash[HPKE_NH];
  uint8_t secret[HPKE_NH];
  bool ok = false;

  // Base mode: empty psk and psk_id. mode identifier is 0x00.
  if (prv_labeled_extract(md, NULL, 0, suite_id, suite_id_len, "psk_id_hash", NULL, 0,
                          psk_id_hash) != 0 ||
      prv_labeled_extract(md, NULL, 0, suite_id, suite_id_len, "info_hash", info, info_len,
                          info_hash) != 0) {
    goto cleanup;
  }

  uint8_t ks_context[1 + HPKE_NH + HPKE_NH];
  ks_context[0] = 0x00;
  memcpy(ks_context + 1, psk_id_hash, HPKE_NH);
  memcpy(ks_context + 1 + HPKE_NH, info_hash, HPKE_NH);

  // secret = LabeledExtract(shared_secret, "secret", psk=""), psk empty in base mode.
  if (prv_labeled_extract(md, shared_secret, HPKE_NSECRET, suite_id, suite_id_len, "secret", NULL,
                          0, secret) != 0) {
    goto cleanup;
  }

  if (prv_labeled_expand(md, secret, suite_id, suite_id_len, "exp", ks_context, sizeof(ks_context),
                         out->exporter_secret, HPKE_NH) != 0) {
    goto cleanup;
  }
  out->kem = (HPKEKemId)kem_id;
  out->aead = aead;
  ok = true;

cleanup:
  mbedtls_platform_zeroize(secret, sizeof(secret));
  mbedtls_platform_zeroize(psk_id_hash, sizeof(psk_id_hash));
  mbedtls_platform_zeroize(info_hash, sizeof(info_hash));
  return ok;
}

// -----------------------------------------------------------------------------
// Secret export (RFC 9180 §5.3): Export(exporter_context, L) =
// LabeledExpand(exporter_secret, "sec", exporter_context, L). This is what
// AccessoryTransportSecurity's key derivation is built on (the watch calls
// `HPKE.Recipient.exportSecret(...)` per Apple's docs).

static bool prv_context_export(const mbedtls_md_info_t *md, const HPKEContext *ctx,
                               const uint8_t *exporter_context, size_t exporter_context_len,
                               uint8_t *out, size_t out_len) {
  HPKESuiteId suite = prv_suite_id((uint16_t)ctx->kem, ctx->aead);
  return prv_labeled_expand(md, ctx->exporter_secret, suite.bytes, sizeof(suite.bytes), "sec",
                            exporter_context, exporter_context_len, out, out_len) == 0;
}

// Decap + base-mode key schedule (recipient).
static bool prv_recipient_context(const HPKEKem *k, uint16_t kem_id, HPKEAeadId aead,
                                  const mbedtls_md_info_t *md, const uint8_t *recipient_sk,
                                  const uint8_t *enc, const uint8_t *info, size_t info_len,
                                  HPKEContext *out) {
  uint8_t pk_r[HPKE_P256_PUBLIC_KEY_BYTES];
  uint8_t dh[32];
  uint8_t kem_context[HPKE_KEM_CONTEXT_MAX];
  bool ok = false;
  if (!k->derive_public(pk_r, recipient_sk) || !k->dh(dh, recipient_sk, enc)) {
    goto cleanup;
  }
  memcpy(kem_context, enc, k->nenc);
  memcpy(kem_context + k->nenc, pk_r, k->nenc);
  uint8_t shared_secret[HPKE_NSECRET];
  if (prv_extract_and_expand(md, kem_id, dh, kem_context, k->nenc * 2, shared_secret) &&
      prv_key_schedule(md, kem_id, aead, shared_secret, info, info_len, out)) {
    ok = true;
  }
  mbedtls_platform_zeroize(shared_secret, sizeof(shared_secret));
cleanup:
  mbedtls_platform_zeroize(dh, sizeof(dh));
  return ok;
}

// -----------------------------------------------------------------------------
// Public entry points.

HPKEResult hpke_open_export(HPKEKemId kem_id, HPKEAeadId aead, const uint8_t *recipient_sk,
                            size_t recipient_sk_len, const uint8_t *enc, size_t enc_len,
                            const uint8_t *info, size_t info_len, const uint8_t *exporter_context,
                            size_t exporter_context_len, uint8_t *out_secret,
                            size_t out_secret_len) {
  const HPKEKem *k = prv_kem(kem_id);
  if (!k || !recipient_sk || !enc || !out_secret) {
    return HPKEErrorBadArgument;
  }
  if (recipient_sk_len != k->nsk || enc_len != k->nenc) {
    return HPKEErrorBadArgument;
  }
  // `aead` is intentionally not range-checked: it only feeds the suite id for the
  // key schedule and Export (no AEAD cipher runs on this path), which lets the
  // RFC 9180 A.3 test drive us with (HPKEAeadId)0x0001. The ship path only ever
  // passes HPKEAeadAes256Gcm.
  const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!md) {
    return HPKEErrorCrypto;
  }
  HPKEContext ctx;
  HPKEResult result = HPKEErrorCrypto;
  if (prv_recipient_context(k, kem_id, aead, md, recipient_sk, enc, info, info_len, &ctx) &&
      prv_context_export(md, &ctx, exporter_context, exporter_context_len, out_secret,
                         out_secret_len)) {
    result = HPKEOk;
  }
  mbedtls_platform_zeroize(&ctx, sizeof(ctx));
  return result;
}

// -----------------------------------------------------------------------------
// AccessoryTransportSecurity glue for the watch peer.

//! Generate the accessory's static P-256 key pair. `out_priv` is the 32-byte
//! scalar (kept secret); `out_pub_raw` is the 64-byte raw X||Y public key (the
//! form sent in a SecurityMessage's `key`, i.e. CryptoKit's rawRepresentation).
HPKEResult hpke_p256_keygen(uint8_t out_priv[HPKE_P256_PRIVATE_KEY_BYTES],
                            uint8_t out_pub_raw[HPKE_P256_PUBLIC_KEY_RAW_BYTES]) {
  const HPKEKem *k = prv_kem(HPKEKemP256HkdfSha256);
  if (!k || !out_priv || !out_pub_raw) {
    return HPKEErrorBadArgument;
  }
  if (!k->gen_sk(out_priv)) {
    return HPKEErrorCrypto;
  }
  return hpke_p256_public_from_private(out_priv, out_pub_raw);
}

HPKEResult hpke_p256_public_from_private(const uint8_t priv[HPKE_P256_PRIVATE_KEY_BYTES],
                                         uint8_t out_pub_raw[HPKE_P256_PUBLIC_KEY_RAW_BYTES]) {
  const HPKEKem *k = prv_kem(HPKEKemP256HkdfSha256);
  if (!k || !priv || !out_pub_raw) {
    return HPKEErrorBadArgument;
  }
  uint8_t pub65[65];
  if (!k->derive_public(pub65, priv)) {
    return HPKEErrorCrypto;
  }
  memcpy(out_pub_raw, pub65 + 1, 64); // drop the 0x04 uncompressed-point prefix
  return HPKEOk;
}

//! AES-256-GCM open: `wire` = nonce(12) || ciphertext || tag(16) under
//! `message_key`, empty AAD.
HPKEResult hpke_aes256gcm_open(const uint8_t message_key[32], const uint8_t *wire, size_t wire_len,
                               uint8_t *out, size_t out_capacity, size_t *out_len) {
  if (!message_key || !wire || !out || !out_len) {
    return HPKEErrorBadArgument;
  }
  if (wire_len < 12 + 16) {
    return HPKEErrorBadArgument;
  }
  size_t pt_len = wire_len - 12 - 16;
  if (out_capacity < pt_len) {
    return HPKEErrorBufferTooSmall;
  }
  const uint8_t *nonce = wire;
  const uint8_t *ct = wire + 12;
  const uint8_t *tag = wire + wire_len - 16;
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  bool ok = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, message_key, 256) == 0 &&
            mbedtls_gcm_auth_decrypt(&gcm, pt_len, nonce, 12, NULL, 0, tag, 16, ct, out) == 0;
  mbedtls_gcm_free(&gcm);
  if (!ok) {
    return HPKEErrorCrypto;
  }
  *out_len = pt_len;
  return HPKEOk;
}

HPKEResult hpke_aes256gcm_seal(const uint8_t message_key[32], const uint8_t *plaintext,
                               size_t pt_len, uint8_t *out_wire, size_t out_capacity,
                               size_t *out_len) {
  if (!message_key || (!plaintext && pt_len) || !out_wire || !out_len) {
    return HPKEErrorBadArgument;
  }
  const size_t wire_len = 12 + pt_len + 16;
  if (out_capacity < wire_len) {
    return HPKEErrorBufferTooSmall;
  }
  uint8_t *nonce = out_wire;             // 12-byte IV, written in place
  uint8_t *ct = out_wire + 12;           // ciphertext
  uint8_t *tag = out_wire + 12 + pt_len; // 16-byte tag
  // Fresh random IV (hard RNG — a repeated GCM nonce under one key is fatal).
  if (!prv_fill_secret(nonce, 12)) {
    return HPKEErrorCrypto;
  }
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  bool ok = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, message_key, 256) == 0 &&
            mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, pt_len, nonce, 12, NULL, 0,
                                      plaintext, ct, 16, tag) == 0;
  mbedtls_gcm_free(&gcm);
  if (!ok) {
    return HPKEErrorCrypto;
  }
  *out_len = wire_len;
  return HPKEOk;
}
