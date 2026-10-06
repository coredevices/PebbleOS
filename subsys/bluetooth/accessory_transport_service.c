/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! @file accessory_transport_service.c
//! Watch-hosted GATT server for Apple AccessoryTransportSecurity (phone = GATT
//! client). Apple pins no service/PSM, so the transport is developer-defined.
//! The watch is the HPKE recipient: it hands up its P-256 public key, the phone
//! returns the encapsulated key + identifiers, and each message key is HPKE Export
//! with info = "P256-Version1-<uuid>", context = info +
//! "-{HostToAccessory,AccessoryToHost}-<feature_id>", AES-256-GCM (empty AAD).
//!
//! Frames (first byte = type). PUBKEY/SESSION fit one ATT op; DATA and RESPONSE can
//! exceed an MTU, so they are fragmented as `type | u8 flags | chunk` (flags bit0 MORE
//! while more follows, bit1 FIRST on the first fragment) and reassembled into the
//! logical frame below:
//!   TX 0x01 PUBKEY   : pub[64] (raw X||Y); notified on subscribe, and the TX value on a read
//!   RX 0x02 SESSION  : enc[65] | u8 uuid_len | uuid
//!   RX 0x03 DATA     : u8 feature_id_len | feature_id | nonce[12] | ct | tag[16]
//!   TX 0x82 RESPONSE : u8 feature_id_len | feature_id | nonce[12] | ct | tag[16] (reply)

#include <pbl/bluetooth/accessory_transport.h>

#include "mbedtls/platform_util.h"

#include <pbl/bluetooth/pebble_bt.h>
#include <host/ble_att.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <host/ble_uuid.h>
#include <kernel/pbl_malloc.h>
#include <nimble/nimble_port.h>
#include <os/os_mbuf.h>
#include <pbl/kernel/compiler.h>
#include <pbl/logging/logging.h>
#include <pbl/services/settings/settings_file.h>
#include <system/status_codes.h>
#include <system/passert.h>
#include <pbl/util/hpke.h>

#include "nimble_type_conversions.h"

#include <string.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

// Frame type bytes.
#define ATS_FRAME_PUBKEY   0x01u // watch -> phone (TX notify or read)
#define ATS_FRAME_SESSION  0x02u // phone -> watch (RX write)
#define ATS_FRAME_DATA     0x03u // phone -> watch (RX write)
#define ATS_FRAME_RESPONSE 0x82u // watch -> phone (TX notify)

// Fragment header flags (byte after the type on a fragmented DATA/RESPONSE frame).
#define ATS_FRAG_MORE  0x01u // more fragments follow
#define ATS_FRAG_FIRST 0x02u // first fragment of a message (receiver resets on this)

// The CoreBluetooth peripheral UUID the phone sends in a SESSION frame (36 chars);
// cap well above that.
#define ATS_MAX_UUID_LEN          64
#define ATS_INFO_PREFIX           "P256-Version1-"
#define ATS_CTX_HOST_TO_ACCESSORY "-HostToAccessory-" // iPhone -> watch (open)
#define ATS_CTX_ACCESSORY_TO_HOST "-AccessoryToHost-" // watch -> iPhone (seal)
#define ATS_MAX_INFO_LEN          (sizeof(ATS_INFO_PREFIX) - 1 + ATS_MAX_UUID_LEN)
#define ATS_MAX_CTX_LEN                                       \
  (ATS_MAX_INFO_LEN + sizeof(ATS_CTX_HOST_TO_ACCESSORY) - 1 + \
   ACCESSORY_TRANSPORT_MAX_FEATURE_ID_LEN)

// The 12-byte AEAD nonce carried in a sealed wire frame (nonce || ciphertext || tag).
#define ATS_NONCE_BYTES 12

// A phone->watch DATA frame (feature_id_len | feature_id | wire) can exceed a single ATT write, so
// it is fragmented across writes just like a reply, and reassembled here. This bounds
// the reassembly buffer at one maximal logical frame.
#define ATS_RX_REASM_MAX                                          \
  (1 + ACCESSORY_TRANSPORT_MAX_FEATURE_ID_LEN + ATS_NONCE_BYTES + \
   ACCESSORY_TRANSPORT_MAX_PAYLOAD + HPKE_TAG_BYTES)

// The BLE minimum ATT MTU; a floor when sizing outgoing fragments.
#define ATS_MIN_ATT_MTU 23

// Sanity ceiling for a single RX write (one SESSION frame or one DATA fragment): an
// ATT attribute value is at most BLE_ATT_ATTR_MAX_LEN (512) bytes.
#define ATS_MAX_RX_WRITE BLE_ATT_ATTR_MAX_LEN

// Derived-key cache depth (per direction + feature id). Two directions and usually
// one feature id, so a handful of slots covers a session.
#define ATS_KEY_CACHE_SLOTS 4

// Key-schedule direction: which context infix the export secret is bound to.
typedef enum {
  ATSDirectionHostToAccessory = 0, // iPhone -> watch (open)
  ATSDirectionAccessoryToHost = 1, // watch -> iPhone reply (seal)
} ATSDirection;

static uint16_t s_tx_notify_handle;
static uint16_t s_rx_write_handle;
// The connection a reply notifies over; updated as data arrives / on subscribe.
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
// The connection that established the current session (SESSION frame or a decrypted DATA
// frame). While it is live, a SESSION write from a different connection is rejected so a
// second paired device can't overwrite the real phone's enc/uuid. Cleared on its
// disconnect / init so the legitimate phone can re-establish on reconnect.
static uint16_t s_session_conn_handle = BLE_HS_CONN_HANDLE_NONE;

// The consumer that parses decrypted notifications, registered at init.
static AccessoryTransportNotificationHandler s_notif_handler;

// Reassembly accumulator for fragmented DATA frames (heap; freed between frames).
// Assumes a single active AN connection: interleaved DATA from another conn would
// mix into the same buffer.
static uint8_t *s_rx_reasm;
static size_t s_rx_reasm_len;

// Accessory keypair. Persisted (plaintext, same trust level as the BLE bonding keys
// already in flash) so the phone's cached public key stays valid across reboots.
static struct {
  bool ready;
  uint8_t priv[HPKE_P256_PRIVATE_KEY_BYTES];
  uint8_t pub_raw[HPKE_P256_PUBLIC_KEY_RAW_BYTES];
} s_keypair;

// Whether the in-RAM keypair is also persisted. If a save fails (store not ready
// yet), this stays false and prv_ensure_keypair retries on later calls (e.g. the
// subscribe path) so the key doesn't stay RAM-only and change on the next boot.
static bool s_keypair_saved;

// Session parameters from the phone's SESSION frame (enc + accessory UUID); kept
// across reconnects (the transport extension's DATA frames carry the feature id the
// per-message key is derived with).
static struct {
  bool enc_ready;
  uint8_t enc[HPKE_P256_PUBLIC_KEY_BYTES];
  uint8_t uuid[ATS_MAX_UUID_LEN];
  uint8_t uuid_len;
  // Derived-key cache: the export secret for (direction, feature id) is fixed for the
  // session, so cache it to avoid two P-256 scalar mults per notification. Cleared
  // when a new SESSION frame changes enc/uuid.
  struct {
    bool valid;
    ATSDirection dir;
    uint8_t feature_id_len;
    uint8_t feature_id[ACCESSORY_TRANSPORT_MAX_FEATURE_ID_LEN];
    uint8_t key[HPKE_AES256GCM_KEY_BYTES];
  } key_cache[ATS_KEY_CACHE_SLOTS];
  uint8_t key_cache_next;
} s_session;

// The private key persists as one value in its own settings file.
#define ATS_KEYPAIR_SETTINGS_FILE "accessoryts_key"
#define ATS_KEYPAIR_KEY           "priv"
#define ATS_KEYPAIR_FILE_SIZE     256 // one 32-byte value + settings-file overhead

//! Outcome of prv_load_keypair — a transient failure must be told apart from a
//! genuinely-absent key so the caller never regenerates over a good stored key.
typedef enum {
  ATSKeypairLoaded,    // a valid keypair was read from the store
  ATSKeypairAbsent,    // no key stored yet (first boot) — safe to generate one
  ATSKeypairCorrupt,   // readable but wrong size / won't derive — delete + regenerate
  ATSKeypairLoadError, // store not ready / read I/O error — retry, don't touch the store
} ATSKeypairLoad;

//! Load the persisted private key and recompute the public key. Distinguishes absent
//! (mint), corrupt (delete + mint), and transient I/O errors (retry, never overwrite a
//! possibly-present key with a fresh one).
static ATSKeypairLoad prv_load_keypair(void) {
  SettingsFile file;
  if (settings_file_open(&file, ATS_KEYPAIR_SETTINGS_FILE, ATS_KEYPAIR_FILE_SIZE) != S_SUCCESS) {
    return ATSKeypairLoadError; // store not ready — retry later, don't regenerate
  }
  uint8_t priv[HPKE_P256_PRIVATE_KEY_BYTES];
  status_t st =
      settings_file_get(&file, ATS_KEYPAIR_KEY, strlen(ATS_KEYPAIR_KEY), priv, sizeof(priv));
  settings_file_close(&file);
  ATSKeypairLoad result;
  if (st == S_SUCCESS) {
    if (hpke_p256_public_from_private(priv, s_keypair.pub_raw) == HPKEOk) {
      memcpy(s_keypair.priv, priv, sizeof(priv));
      s_keypair.ready = true;
      s_keypair_saved = true; // it came from the store
      result = ATSKeypairLoaded;
    } else {
      result = ATSKeypairCorrupt; // stored value read but not a valid P-256 scalar
    }
  } else if (st == E_DOES_NOT_EXIST) {
    result = ATSKeypairAbsent; // nothing stored yet — caller may mint one
  } else if (st == E_RANGE) {
    result = ATSKeypairCorrupt; // stored value is the wrong size
  } else {
    // A read I/O error: transient, so a freshly generated key never clobbers a
    // possibly-present stored one.
    result = ATSKeypairLoadError;
  }
  mbedtls_platform_zeroize(priv, sizeof(priv)); // don't leave the key on the stack
  return result;
}

//! Delete the stored keypair entry (best-effort), so a corrupt value can be replaced.
static void prv_delete_keypair(void) {
  SettingsFile file;
  if (settings_file_open(&file, ATS_KEYPAIR_SETTINGS_FILE, ATS_KEYPAIR_FILE_SIZE) != S_SUCCESS) {
    return;
  }
  settings_file_delete(&file, ATS_KEYPAIR_KEY, strlen(ATS_KEYPAIR_KEY));
  settings_file_close(&file);
}

//! Persist the in-RAM private key. Returns false if the store isn't ready, so the
//! caller can retry later (the key stays RAM-only until then).
static bool prv_save_keypair(void) {
  SettingsFile file;
  if (settings_file_open(&file, ATS_KEYPAIR_SETTINGS_FILE, ATS_KEYPAIR_FILE_SIZE) != S_SUCCESS) {
    return false;
  }
  status_t st = settings_file_set(&file, ATS_KEYPAIR_KEY, strlen(ATS_KEYPAIR_KEY), s_keypair.priv,
                                  sizeof(s_keypair.priv));
  settings_file_close(&file);
  return st == S_SUCCESS;
}

//! Ensure the accessory keypair exists: load the persisted one, else generate a new
//! one and persist it. Once the key is in RAM this only retries a failed save, so
//! the key gets persisted (and stays stable across boots) as soon as the store is up.
static bool prv_ensure_keypair(void) {
  if (s_keypair.ready) {
    if (!s_keypair_saved) {
      s_keypair_saved = prv_save_keypair(); // retry persisting a still-RAM-only key
    }
    return true;
  }
  switch (prv_load_keypair()) {
    case ATSKeypairLoaded:
      PBL_LOG_DBG("AN transport: loaded persisted keypair");
      return true;
    case ATSKeypairLoadError:
      // Store not ready or read I/O error. Do NOT generate — a new key could overwrite the
      // stored one and break decrypt until re-pair. Retry the load on the next call.
      PBL_LOG_DBG("AN transport: keypair load deferred (store not ready)");
      return false;
    case ATSKeypairCorrupt:
      // Readable but invalid (wrong size / won't derive): delete it so we don't retry
      // forever, then mint a fresh one below.
      PBL_LOG_WRN("AN transport: stored keypair corrupt; deleting and regenerating");
      prv_delete_keypair();
      break;
    case ATSKeypairAbsent:
      break; // genuinely no key yet — mint and persist below
  }
  HPKEResult res = hpke_p256_keygen(s_keypair.priv, s_keypair.pub_raw);
  if (res != HPKEOk) {
    PBL_LOG_ERR("AN transport: keygen failed: %d", (int)res);
    return false;
  }
  s_keypair.ready = true;
  s_keypair_saved = prv_save_keypair();
  return true;
}

//! A TX read returns the PUBKEY frame, the same bytes the subscribe notification carries.
//! The phone's key-exchange and transport extensions are separate processes on one link,
//! and a second subscriber writes no CCCD, so it gets no subscribe event and no key.
static int prv_access_tx(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
    return BLE_ATT_ERR_UNLIKELY;
  }
  if (!prv_ensure_keypair()) {
    return BLE_ATT_ERR_UNLIKELY;
  }
  const uint8_t type = ATS_FRAME_PUBKEY;
  if (os_mbuf_append(ctxt->om, &type, 1) != 0 ||
      os_mbuf_append(ctxt->om, s_keypair.pub_raw, sizeof(s_keypair.pub_raw)) != 0) {
    return BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  return 0;
}

//! Notify one TX frame: `type`, then optional `hdr`, then `body` (the mbuf appends do
//! the assembly, so a caller can stream a slice without a contiguous copy). Returns
//! false if the frame could not be sent — the fragment loop uses that to abort.
static bool prv_notify_frame(uint16_t conn_handle, uint8_t type, const uint8_t *hdr, size_t hdr_len,
                             const uint8_t *body, size_t body_len) {
  struct os_mbuf *om = ble_hs_mbuf_from_flat(&type, 1);
  if (!om) {
    PBL_LOG_ERR("AN transport: no mbuf for notify (type 0x%02x)", type);
    return false;
  }
  if ((hdr_len > 0 && os_mbuf_append(om, hdr, hdr_len) != 0) ||
      (body_len > 0 && os_mbuf_append(om, body, body_len) != 0)) {
    os_mbuf_free_chain(om);
    PBL_LOG_ERR("AN transport: mbuf append failed (type 0x%02x)", type);
    return false;
  }
  int rc = ble_gatts_notify_custom(conn_handle, s_tx_notify_handle, om);
  if (rc != 0) {
    PBL_LOG_ERR("AN transport: notify failed (type 0x%02x): 0x%04x", type, (uint16_t)rc);
    return false;
  }
  return true;
}

static void prv_notify_pubkey(uint16_t conn_handle) {
  prv_notify_frame(conn_handle, ATS_FRAME_PUBKEY, NULL, 0, s_keypair.pub_raw,
                   sizeof(s_keypair.pub_raw));
}

//! RX 0x02 SESSION: enc[65] | u8 uuid_len | uuid. Stashes the encapsulated key and
//! the accessory UUID; the message key is derived later, per DATA frame (which
//! carries the feature id). Written by the phone's security extension.
static void prv_handle_session_frame(uint16_t conn_handle, const uint8_t *p, uint16_t len) {
  // Don't let a different connection overwrite an active session. s_session_conn_handle
  // is cleared when its owner disconnects, so this only rejects a second live device —
  // the legitimate phone can still refresh on the same link or re-establish on reconnect.
  if (s_session.enc_ready && s_session_conn_handle != BLE_HS_CONN_HANDLE_NONE &&
      conn_handle != s_session_conn_handle) {
    PBL_LOG_WRN("AN transport: rejecting SESSION overwrite from a different connection");
    return;
  }
  if (len < HPKE_P256_PUBLIC_KEY_BYTES + 1) {
    PBL_LOG_ERR("AN transport: session frame too short (%u)", len);
    return;
  }
  uint16_t off = HPKE_P256_PUBLIC_KEY_BYTES;
  uint8_t uuid_len = p[off++];
  if (uuid_len == 0 || uuid_len > ATS_MAX_UUID_LEN || off + uuid_len > len) {
    PBL_LOG_ERR("AN transport: bad uuid_len %u", uuid_len);
    return;
  }
  s_session_conn_handle = conn_handle; // this connection now owns the session
  memcpy(s_session.enc, p, HPKE_P256_PUBLIC_KEY_BYTES);
  memcpy(s_session.uuid, &p[off], uuid_len);
  s_session.uuid_len = uuid_len;
  s_session.enc_ready = true;
  // enc/uuid just changed, so any cached derived keys are stale — drop them.
  memset(s_session.key_cache, 0, sizeof(s_session.key_cache));
  s_session.key_cache_next = 0;
  PBL_LOG_DBG("AN transport: session params stored (enc + uuid)");
}

//! Derive the per-message AES-256-GCM key for `dir` and this feature id from
//! the stored enc + uuid: info = "P256-Version1-<uuid>", context = info +
//! "-HostToAccessory-<feature_id>" (open) or "-AccessoryToHost-<feature_id>" (seal). The result
//! is cached per (dir, feature_id) for the session, since the two P-256 scalar mults
//! inside hpke_open_export are otherwise repeated on every notification.
static bool prv_derive_key(ATSDirection dir, const uint8_t *feature_id, uint8_t feature_id_len,
                           uint8_t out_key[HPKE_AES256GCM_KEY_BYTES]) {
  for (int i = 0; i < ATS_KEY_CACHE_SLOTS; i++) {
    if (s_session.key_cache[i].valid && s_session.key_cache[i].dir == dir &&
        s_session.key_cache[i].feature_id_len == feature_id_len &&
        memcmp(s_session.key_cache[i].feature_id, feature_id, feature_id_len) == 0) {
      memcpy(out_key, s_session.key_cache[i].key, HPKE_AES256GCM_KEY_BYTES);
      return true;
    }
  }

  static const struct {
    const char *str;
    size_t len;
  } k_dir_ctx[] = {
    [ATSDirectionHostToAccessory] =
        {ATS_CTX_HOST_TO_ACCESSORY, sizeof(ATS_CTX_HOST_TO_ACCESSORY) - 1},
    [ATSDirectionAccessoryToHost] = {
      ATS_CTX_ACCESSORY_TO_HOST, sizeof(ATS_CTX_ACCESSORY_TO_HOST) - 1
    },
  };
  const char *direction = k_dir_ctx[dir].str;
  const size_t direction_len = k_dir_ctx[dir].len;

  uint8_t info[ATS_MAX_INFO_LEN];
  size_t info_len = sizeof(ATS_INFO_PREFIX) - 1;
  memcpy(info, ATS_INFO_PREFIX, info_len);
  memcpy(info + info_len, s_session.uuid, s_session.uuid_len);
  info_len += s_session.uuid_len;

  uint8_t ctx[ATS_MAX_CTX_LEN];
  size_t ctx_len = info_len;
  memcpy(ctx, info, info_len);
  memcpy(ctx + ctx_len, direction, direction_len);
  ctx_len += direction_len;
  memcpy(ctx + ctx_len, feature_id, feature_id_len);
  ctx_len += feature_id_len;

  HPKEResult res =
      hpke_open_export(HPKEKemP256HkdfSha256, HPKEAeadAes256Gcm, s_keypair.priv,
                       sizeof(s_keypair.priv), s_session.enc, HPKE_P256_PUBLIC_KEY_BYTES, info,
                       info_len, ctx, ctx_len, out_key, HPKE_AES256GCM_KEY_BYTES);
  if (res != HPKEOk) {
    PBL_LOG_ERR("AN transport: HPKE export failed: %d", (int)res);
    return false;
  }

  // Round-robin; feature_id_len is bounded by the caller.
  const uint8_t slot = s_session.key_cache_next;
  s_session.key_cache[slot].valid = true;
  s_session.key_cache[slot].dir = dir;
  s_session.key_cache[slot].feature_id_len = feature_id_len;
  memcpy(s_session.key_cache[slot].feature_id, feature_id, feature_id_len);
  memcpy(s_session.key_cache[slot].key, out_key, HPKE_AES256GCM_KEY_BYTES);
  s_session.key_cache_next = (uint8_t)((slot + 1) % ATS_KEY_CACHE_SLOTS);
  return true;
}

//! Seal a reply (accessory->host) for feature id `feature_id` and notify it on TX as a
//! 0x82 RESPONSE frame. Must run on the NimBLE host task (notifies). Returns true only
//! if the reply was fully sealed and notified — the caller reports this as the result.
static bool prv_send_response(uint16_t conn_handle, const uint8_t *feature_id,
                              uint8_t feature_id_len, const uint8_t *plaintext, size_t pt_len) {
  if (!s_keypair.ready || !s_session.enc_ready) {
    return false;
  }
  if (pt_len > ACCESSORY_TRANSPORT_MAX_PAYLOAD) {
    return false;
  }
  uint8_t key[HPKE_AES256GCM_KEY_BYTES];
  if (!prv_derive_key(ATSDirectionAccessoryToHost, feature_id, feature_id_len, key)) {
    return false;
  }
  // Heap, not stack (this runs after HPKE, which already presses on the host-task stack).
  const size_t frame_cap =
      1 + feature_id_len +
      (ATS_NONCE_BYTES + pt_len + HPKE_TAG_BYTES); // feature_id_len | feature_id | wire
  uint8_t *frame = kernel_malloc(frame_cap);
  if (!frame) {
    return false;
  }
  frame[0] = feature_id_len;
  memcpy(&frame[1], feature_id, feature_id_len);
  size_t wire_len = 0;
  if (hpke_aes256gcm_seal(key, plaintext, pt_len, &frame[1 + feature_id_len],
                          ATS_NONCE_BYTES + pt_len + HPKE_TAG_BYTES, &wire_len) != HPKEOk) {
    PBL_LOG_ERR("AN transport: reply seal failed");
    kernel_free(frame);
    return false;
  }
  // The reply can exceed the ATT MTU, so fragment it: each 0x82 frame is `flags | chunk`
  // (flags = MORE|FIRST). The phone resets its reassembly on FIRST and concatenates
  // chunks into feature_id_len | feature_id | wire. Abort if a fragment fails to send —
  // a partial message must not dribble out (the phone would splice it onto the next).
  const size_t total_len = 1 + feature_id_len + wire_len;
  const uint16_t mtu = ble_att_mtu(conn_handle);
  const uint16_t eff_mtu = (mtu > ATS_MIN_ATT_MTU) ? mtu : ATS_MIN_ATT_MTU;
  const size_t usable = eff_mtu - 5; // 3 ATT + 1 type + 1 flags
  size_t sent = 0;
  while (sent < total_len) {
    size_t n = total_len - sent;
    if (n > usable) {
      n = usable;
    }
    uint8_t flags = 0;
    if (sent + n < total_len) {
      flags |= ATS_FRAG_MORE;
    }
    if (sent == 0) {
      flags |= ATS_FRAG_FIRST;
    }
    if (!prv_notify_frame(conn_handle, ATS_FRAME_RESPONSE, &flags, 1, &frame[sent], n)) {
      break; // abort the rest; a partial reply is worse than none.
    }
    sent += n;
  }
  kernel_free(frame);
  return sent == total_len; // false if a fragment failed mid-send
}

//! RX 0x03 DATA: u8 feature_id_len | feature_id | nonce(12) | ciphertext | tag(16). Derives the
//! key for this feature id and opens the wire. Written by the phone's
//! transport extension.
static void prv_handle_data_frame(uint16_t conn_handle, const uint8_t *p, uint16_t len) {
  if (!s_keypair.ready || !s_session.enc_ready) {
    PBL_LOG_ERR("AN transport: data frame before session params");
    return;
  }
  if (len < 1) {
    return;
  }
  uint8_t feature_id_len = p[0];
  if (feature_id_len == 0 || feature_id_len > ACCESSORY_TRANSPORT_MAX_FEATURE_ID_LEN ||
      (uint16_t)(1 + feature_id_len) > len) {
    PBL_LOG_ERR("AN transport: bad feature_id_len %u", feature_id_len);
    return;
  }
  const uint8_t *feature_id = &p[1];
  const uint8_t *wire = &p[1 + feature_id_len];
  uint16_t wire_len = len - 1 - feature_id_len;
  if (wire_len < ATS_NONCE_BYTES + HPKE_TAG_BYTES) {
    PBL_LOG_ERR("AN transport: data frame too short (%u)", wire_len);
    return;
  }
  // Plaintext is exactly wire minus the nonce and tag, so size the buffer to that
  // rather than the fixed maximum.
  const size_t pt_cap = wire_len - ATS_NONCE_BYTES - HPKE_TAG_BYTES;

  uint8_t message_key[HPKE_AES256GCM_KEY_BYTES];
  if (!prv_derive_key(ATSDirectionHostToAccessory, feature_id, feature_id_len, message_key)) {
    return;
  }
  // Heap, not stack (the decrypt runs on the NimBLE host task, whose stack HPKE presses on).
  uint8_t *plaintext = kernel_malloc(pt_cap ? pt_cap : 1);
  if (!plaintext) {
    return;
  }
  size_t pt_len = 0;
  HPKEResult res = hpke_aes256gcm_open(message_key, wire, wire_len, plaintext, pt_cap, &pt_len);
  if (res != HPKEOk) {
    PBL_LOG_ERR("AN transport: data decrypt failed: %d", (int)res);
    kernel_free(plaintext);
    return;
  }
  PBL_LOG_DBG("AN transport: decrypted %u bytes", (unsigned)pt_len);
  // Only a valid, decryptable DATA frame (re)binds the reply target, so a bogus write
  // from another encrypted connection can't hijack where replies go. A successful decrypt
  // also proves this connection holds the session key, so it (re)claims session ownership
  // — this re-establishes it after a reconnect that didn't re-send SESSION.
  s_conn_handle = conn_handle;
  s_session_conn_handle = conn_handle;
  // Hand the decrypted AccessoryNotifications payload to the registered consumer
  // (the parser for Apple's custom serialized notification format).
  if (s_notif_handler) {
    s_notif_handler(plaintext, pt_len, (const char *)feature_id, feature_id_len);
  }
  kernel_free(plaintext);
}

// The reply is requested from the UI task (a notification action), but HPKE +
// notify must run where the BT stack lives (the caller task's stack is too small
// for P-256 and notify wants the host task). Marshal onto the NimBLE host task.
typedef struct {
  struct ble_npl_event ev;
  AccessoryTransportSendComplete complete; // async delivery result (may be NULL)
  void *ctx;
  uint16_t conn_handle;
  uint8_t feature_id[ACCESSORY_TRANSPORT_MAX_FEATURE_ID_LEN];
  uint8_t feature_id_len;
  size_t payload_len;
  uint8_t payload[]; // flexible: allocated sizeof(*r) + payload_len
} ATSReplyReq;

static void prv_reply_event(struct ble_npl_event *ev) {
  ATSReplyReq *r = ble_npl_event_get_arg(ev);
  const bool ok = prv_send_response(r->conn_handle, r->feature_id, r->feature_id_len, r->payload,
                                    r->payload_len);
  if (r->complete) {
    r->complete(r->ctx, ok); // report the real seal+notify outcome (fires exactly once)
  }
  kernel_free(r);
}

bool accessory_transport_service_send_response(const char *feature_id, size_t feature_id_len,
                                               const uint8_t *payload, size_t payload_len,
                                               AccessoryTransportSendComplete complete, void *ctx) {
  // Best-effort pre-check (no lock; these are owned by the host task, where the
  // authoritative seal+notify runs) so the caller isn't told "accepted" with no way to send.
  // On any false return here `complete` is NOT called (the caller keeps `ctx`).
  if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !s_keypair.ready || !s_session.enc_ready ||
      !feature_id || feature_id_len == 0 ||
      feature_id_len > ACCESSORY_TRANSPORT_MAX_FEATURE_ID_LEN ||
      payload_len > ACCESSORY_TRANSPORT_MAX_PAYLOAD) {
    return false;
  }
  ATSReplyReq *r = kernel_malloc(sizeof(*r) + payload_len);
  if (!r) {
    return false;
  }
  r->complete = complete;
  r->ctx = ctx;
  r->conn_handle = s_conn_handle;
  r->feature_id_len = (uint8_t)feature_id_len;
  memcpy(r->feature_id, feature_id, feature_id_len);
  r->payload_len = payload_len;
  if (payload_len) {
    memcpy(r->payload, payload, payload_len);
  }
  ble_npl_event_init(&r->ev, prv_reply_event, r);
  ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &r->ev);
  return true;
}

void accessory_transport_service_set_handler(AccessoryTransportNotificationHandler handler) {
  s_notif_handler = handler;
}

static void prv_rx_reasm_reset(void) {
  if (s_rx_reasm) {
    kernel_free(s_rx_reasm);
    s_rx_reasm = NULL;
  }
  s_rx_reasm_len = 0;
}

//! Append a DATA fragment's chunk to the reassembly buffer (lazily allocated).
//! Returns false if it would overflow one maximal frame; the caller then drops it.
static bool prv_rx_reasm_append(const uint8_t *p, size_t n) {
  if (!s_rx_reasm) {
    s_rx_reasm = kernel_malloc(ATS_RX_REASM_MAX);
    if (!s_rx_reasm) {
      return false;
    }
    s_rx_reasm_len = 0;
  }
  if (s_rx_reasm_len + n > ATS_RX_REASM_MAX) {
    return false;
  }
  memcpy(&s_rx_reasm[s_rx_reasm_len], p, n);
  s_rx_reasm_len += n;
  return true;
}

static int prv_access_rx_write(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
    return BLE_ATT_ERR_UNLIKELY;
  }
  const uint16_t pkt_len = OS_MBUF_PKTLEN(ctxt->om);
  if (pkt_len == 0) {
    return 0;
  }
  // One ATT write is at most a whole SESSION frame or one DATA fragment (<= the ATT
  // 512-byte value limit). Allocate exactly that; flattened onto the heap, not the
  // stack, because this runs on the NimBLE host task whose stack the HPKE open below
  // (P-256 scalar mults, mbedtls scratch) already presses on.
  if (pkt_len > ATS_MAX_RX_WRITE) {
    PBL_LOG_ERR("AN transport: RX frame too large (%u)", pkt_len);
    return BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  uint8_t *buf = kernel_malloc(pkt_len);
  if (!buf) {
    return BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  uint16_t out_len = 0;
  int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, pkt_len, &out_len);
  if (rc != 0) {
    PBL_LOG_ERR("AN transport: RX flatten failed: 0x%04x", (uint16_t)rc);
    kernel_free(buf);
    return BLE_ATT_ERR_UNLIKELY;
  }
  const uint8_t type = buf[0];
  const uint8_t *payload = &buf[1];
  const uint16_t payload_len = out_len - 1;
  switch (type) {
    case ATS_FRAME_SESSION:
      prv_handle_session_frame(conn_handle, payload, payload_len);
      break;
    case ATS_FRAME_DATA: {
      // DATA is fragmented across ATT writes (flags | chunk). Reassemble into
      // feature_id_len | feature_id | wire; process on !MORE. Reset on FIRST so a peer
      // that restarted mid-message can't splice a stale tail onto the new one.
      if (payload_len < 1) {
        break;
      }
      const uint8_t flags = payload[0];
      if (flags & ATS_FRAG_FIRST) {
        prv_rx_reasm_reset();
      }
      if (!prv_rx_reasm_append(&payload[1], payload_len - 1)) {
        PBL_LOG_ERR("AN transport: RX reassembly overflow, dropping frame");
        prv_rx_reasm_reset();
        break;
      }
      if (!(flags & ATS_FRAG_MORE)) {
        prv_handle_data_frame(conn_handle, s_rx_reasm, (uint16_t)s_rx_reasm_len);
        prv_rx_reasm_reset();
      }
      break;
    }
    default:
      PBL_LOG_ERR("AN transport: unknown RX frame type 0x%02x", type);
      break;
  }
  kernel_free(buf);
  return 0;
}

#include "accessory_transport_gatt.h"

typedef struct {
  uint8_t count;
  uint16_t handle; // the last-seen encrypted connection (meaningful only when count == 1)
} ATSEncConnAcc;

static int prv_count_encrypted_conn(uint16_t conn_handle, void *arg) {
  ATSEncConnAcc *acc = arg;
  struct ble_gap_conn_desc desc;
  if (ble_gap_conn_find(conn_handle, &desc) == 0 && desc.sec_state.encrypted) {
    acc->count++;
    acc->handle = conn_handle;
  }
  return 0; // keep iterating all connections
}

//! Count currently-encrypted connections. When exactly one exists, return its handle via
//! *only_handle. Used so a TX re-subscribe can rebind the reply target on a lone
//! encrypted link (reconnect) without letting a second bonded central hijack replies.
static int prv_encrypted_conn_count(uint16_t *only_handle) {
  ATSEncConnAcc acc = {.count = 0, .handle = BLE_HS_CONN_HANDLE_NONE};
  ble_gap_conn_foreach_handle(prv_count_encrypted_conn, &acc);
  if (acc.count == 1 && only_handle) {
    *only_handle = acc.handle;
  }
  return acc.count;
}

static void prv_handle_subscribe_event(struct ble_gap_event *event) {
  if (event->subscribe.attr_handle != s_tx_notify_handle) {
    return;
  }
  if (event->subscribe.cur_notify == event->subscribe.prev_notify) {
    return;
  }
  if (event->subscribe.cur_notify) {
    // A client subscribed: ensure the keypair and hand up the public key. Don't reset
    // the session here — several phone processes subscribe, and the enc/uuid from a
    // prior SESSION frame must survive.
    if (!prv_ensure_keypair()) {
      return;
    }
    PBL_LOG_DBG("AN transport: subscribed, sending public key");
    prv_notify_pubkey(event->subscribe.conn_handle);
    // Rebind the reply target on reconnect, but only when the subscriber is the SOLE
    // encrypted connection: a lone bonded iPhone re-subscribing after a drop must be
    // able to reply to an already-stored notification before the next DATA frame (the
    // session is kept across reconnects; only the conn handle was lost). With 2+
    // encrypted links we leave binding to the decrypt of a DATA frame, so a second
    // bonded central can't hijack replies just by subscribing. Requiring the sole
    // encrypted handle to equal this subscriber also covers the case where this link
    // isn't encrypted yet at subscribe time (then we don't bind, and decrypt binds later).
    uint16_t only_handle = BLE_HS_CONN_HANDLE_NONE;
    if (prv_encrypted_conn_count(&only_handle) == 1 &&
        only_handle == event->subscribe.conn_handle) {
      s_conn_handle = event->subscribe.conn_handle;
      PBL_LOG_DBG("AN transport: sole encrypted link, bound reply target on subscribe");
    }
  } else if (event->subscribe.conn_handle == s_conn_handle) {
    // This client unsubscribed from our TX characteristic; stop treating it as the
    // reply target.
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
  }
}

static int prv_handle_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_SUBSCRIBE:
      prv_handle_subscribe_event(event);
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      // Only forget the reply handle if it is OUR link that dropped (another
      // connection, e.g. an HRM, must not break replies). Keep the session params
      // across reconnects; a new SESSION frame overwrites them, a reboot clears them.
      if (event->disconnect.conn.conn_handle == s_conn_handle) {
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        prv_rx_reasm_reset(); // drop any half-received DATA frame
      }
      // Release session ownership when its owner drops, so the phone can re-establish on
      // reconnect (the enc/uuid itself is kept until a new SESSION replaces it).
      if (event->disconnect.conn.conn_handle == s_session_conn_handle) {
        s_session_conn_handle = BLE_HS_CONN_HANDLE_NONE;
      }
      break;
    default:
      break;
  }
  return 0;
}

static struct ble_gap_event_listener s_gap_event_listener;

void accessory_transport_service_init(void) {
  // Load/generate the keypair here — off the host task, where the settings store is
  // up — so flash I/O stays off the subscribe path. Idempotent + best-effort; the
  // subscribe handler retries if the store wasn't ready.
  prv_ensure_keypair();
  // Runs on every bt_driver_start. If a DISCONNECT was missed across a
  // pbl_bt_stop/start, s_conn_handle would be stale — send_response's precheck
  // would pass and report "Sent" while the notify silently fails. Clear it; a
  // fresh subscribe re-establishes the handle.
  s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
  s_session_conn_handle = BLE_HS_CONN_HANDLE_NONE;
  prv_rx_reasm_reset();
  int rc = ble_gatts_count_cfg(s_an_transport_svc);
  PBL_ASSERTN(rc == 0);
  rc = ble_gatts_add_svcs(s_an_transport_svc);
  PBL_ASSERTN(rc == 0);
  // Called on every bt_driver_start; the listener list is only cleared on
  // nimble_port_init, so tolerate EALREADY.
  rc = ble_gap_event_listener_register(&s_gap_event_listener, prv_handle_gap_event, NULL);
  PBL_ASSERTN(rc == 0 || rc == BLE_HS_EALREADY);
}
