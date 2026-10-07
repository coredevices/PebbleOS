/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

//! @file hpke.h
//! HPKE (RFC 9180) base mode, recipient side, DHKEM(P-256, HKDF-SHA256) +
//! AES-256-GCM — the suite AccessoryTransportSecurity's `p256` uses. Mbed TLS has
//! the primitives but no HPKE, so the key schedule and secret export live here,
//! alongside the P-256 keygen and the AES-256-GCM message seal/open the transport
//! relies on.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//! The KEM (key encapsulation mechanism). The value is the RFC 9180 KEM id.
typedef enum {
  //! DHKEM(P-256, HKDF-SHA256) (0x0010). Recipient public key and `enc` are a
  //! 65-byte uncompressed SEC1 point; the private key is a 32-byte scalar.
  HPKEKemP256HkdfSha256 = 0x0010,
} HPKEKemId;

//! The AEAD. The value is the RFC 9180 AEAD id.
typedef enum {
  //! AES-256-GCM (0x0002), the AEAD Apple's suites use. 32-byte key.
  HPKEAeadAes256Gcm = 0x0002,
} HPKEAeadId;

//! Key/`enc` sizes. P-256 public keys and `enc` are a 65-byte uncompressed SEC1
//! point (0x04 || X || Y); private keys a 32-byte scalar. The "raw" form is the
//! 64-byte X || Y (CryptoKit's rawRepresentation), used for the accessory pubkey.
#define HPKE_P256_PUBLIC_KEY_BYTES     65
#define HPKE_P256_PUBLIC_KEY_RAW_BYTES 64
#define HPKE_P256_PRIVATE_KEY_BYTES    32
//! The AES-256-GCM authentication tag appended to every ciphertext.
#define HPKE_TAG_BYTES 16
//! AES-256-GCM key length — the message key HPKE Export produces for this suite.
#define HPKE_AES256GCM_KEY_BYTES 32

typedef enum {
  HPKEOk = 0,
  HPKEErrorBadArgument,
  HPKEErrorCrypto, //!< A primitive (DH, HKDF, AEAD) failed or rejected input.
  HPKEErrorBufferTooSmall,
} HPKEResult;

//! HPKE secret export (RFC 9180 §5.3), recipient side: decapsulate `enc` with the
//! recipient private key under `info`, then Export(`exporter_context`, `out_secret_len`)
//! into `out_secret`. This is the primitive AccessoryNotifications' transport security
//! builds its per-direction keys on (Apple's `HPKE.Recipient.exportSecret(...)`).
HPKEResult hpke_open_export(HPKEKemId kem, HPKEAeadId aead, const uint8_t *recipient_sk,
                            size_t recipient_sk_len, const uint8_t *enc, size_t enc_len,
                            const uint8_t *info, size_t info_len, const uint8_t *exporter_context,
                            size_t exporter_context_len, uint8_t *out_secret,
                            size_t out_secret_len);

//! Generate the accessory's static P-256 key pair for AccessoryTransportSecurity.
//! `out_priv` = 32-byte scalar, `out_pub_raw` = 64-byte raw X||Y public key (the
//! form carried in a SecurityMessage's `key`).
HPKEResult hpke_p256_keygen(uint8_t out_priv[HPKE_P256_PRIVATE_KEY_BYTES],
                            uint8_t out_pub_raw[HPKE_P256_PUBLIC_KEY_RAW_BYTES]);

//! Recompute the 64-byte raw X||Y public key from a stored 32-byte private scalar
//! (e.g. after loading a persisted keypair). Same output form as hpke_p256_keygen.
HPKEResult hpke_p256_public_from_private(const uint8_t priv[HPKE_P256_PRIVATE_KEY_BYTES],
                                         uint8_t out_pub_raw[HPKE_P256_PUBLIC_KEY_RAW_BYTES]);

//! AES-256-GCM open: `wire` = nonce(12) || ciphertext || tag(16) under
//! `message_key`, empty AAD. (Used for the transport's data-message framing.)
HPKEResult hpke_aes256gcm_open(const uint8_t message_key[32], const uint8_t *wire, size_t wire_len,
                               uint8_t *out, size_t out_capacity, size_t *out_len);

//! AES-256-GCM seal with a fresh random 12-byte IV, empty AAD; `out_wire` =
//! nonce(12) || ciphertext || tag(16), so `*out_len` = pt_len + 28. Inverse of
//! `hpke_aes256gcm_open`.
HPKEResult hpke_aes256gcm_seal(const uint8_t message_key[32], const uint8_t *plaintext,
                               size_t pt_len, uint8_t *out_wire, size_t out_capacity,
                               size_t *out_len);
