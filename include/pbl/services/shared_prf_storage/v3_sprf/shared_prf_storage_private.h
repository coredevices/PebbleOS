/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>
#include <pbl/kernel/compiler.h>

/**
 * @defgroup services_shared_prf_storage_v3_sprf Shared PRF storage layout
 * @ingroup services_shared_prf_storage
 * @brief On-flash layout of shared PRF storage entries.
 *
 * The region holds a rolling list of @ref SharedPRFData entries of 256 bytes, so that a whole
 * number of them fits in a flash sector. Exactly one entry is valid (or unpopulated); entries
 * before it are invalidated.
 *
 * An entry is made of fields, each starting with a CRC-32 (pbl_crc32()) of the rest of the
 * field, written once the field is complete. A field is:
 * - unpopulated if it is all 0xFF;
 * - valid if its CRC matches its contents;
 * - corrupted, e.g. partially written, otherwise, which invalidates the whole entry.
 *
 * Rewriting a field invalidates the current entry and copies its other fields to the next one.
 * @{
 */

/** @brief Page index meaning no valid entry has been found. */
#define SPRF_PAGE_IDX_INVALID ((uint16_t)~0)

/**
 * @brief Page index beyond which the region is compacted at boot.
 *
 * @param num Number of pages in the region.
 */
#define SPRF_MAX_NUM_PAGES_MULT(num) ((num) * 3 / 4)

/** @brief Valid parts of @ref SprfBlePairingData. */
typedef enum {
  /** Local encryption info (l_ltk, l_rand, l_ediv). */
  SprfValidFields_LocalEncryptionInfoValid = (1 << 0),
  /** Remote encryption info (r_ltk, r_rand, r_ediv). */
  SprfValidFields_RemoteEncryptionInfoValid = (1 << 1),
  /** Remote identity info (irk, identity). */
  SprfValidFields_RemoteIdentityInfoValid = (1 << 2),
  /** Remote signing info (csrk). */
  SprfValidFields_RemoteSigningInfoValid = (1 << 3),
} SprfValidFields;

#ifndef __clang__
_Static_assert(sizeof(SprfValidFields) == 1, "SprfValidFields unexpected size");
#endif

/** @brief Entry header magic. */
typedef enum {
  /** Valid entry, "SPRF". */
  SprfMagic_ValidEntry = 0x46525053,
  /** Erased entry, not written yet. */
  SprfMagic_UnpopulatedEntry = 0xFFFFFFFF,
  /** Invalidated entry, superseded by a later one. */
  SprfMagic_InvalidatedEntry = 0x0
} SprfMagic;

_Static_assert(sizeof(SprfMagic) == 4, "SprfMagic unexpected size");

/** @brief BLE root keys field. */
typedef struct PBL_PACKED SprfRootKeys {
  /** Field CRC. */
  uint32_t crc;
  /** Keys indexed by @ref pbl_bt_sm_root_key_type. */
  struct pbl_bt_sm_key keys[PBL_BT_SM_ROOT_KEY_TYPE_NUM];
} SprfRootKeys;
_Static_assert(offsetof(SprfRootKeys, crc) == 0, "crc must be the first field");

/** @brief BLE pairing field, for the device most recently paired. */
typedef struct PBL_PACKED SprfBlePairingData {
  /** Field CRC. */
  uint32_t crc;

  /** Local LTK. */
  struct pbl_bt_sm_key l_ltk;
  /** Local Rand. */
  uint64_t l_rand;
  /** Local EDIV. */
  uint16_t l_ediv;

  /** Remote EDIV. */
  uint16_t r_ediv;
  /** Remote LTK. */
  struct pbl_bt_sm_key r_ltk;
  /** Remote Rand. */
  uint64_t r_rand;

  /** Remote IRK. */
  struct pbl_bt_sm_key irk;
  /** Remote CSRK. */
  struct pbl_bt_sm_key csrk;
  /** Remote identity address. */
  struct pbl_bt_device_internal identity;

  /** Valid parts, @ref SprfValidFields bits. 0 means no pairing. */
  SprfValidFields fields : 8;
  /** Pairing has MITM protection. */
  bool is_mitm_protection_enabled;
  /** Pairing requires address pinning. */
  bool requires_address_pinning;

  /** Pairing flags. Added in version 2, always 0 in version 1. */
  uint8_t flags;
} SprfBlePairingData;
_Static_assert(offsetof(SprfBlePairingData, crc) == 0, "crc must be the first field");

/** @brief BLE pairing device name field. */
typedef struct PBL_PACKED SprfBlePairingName {
  /** Field CRC. */
  uint32_t crc;
  /** Remote device name. */
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
} SprfBlePairingName;
_Static_assert(offsetof(SprfBlePairingName, crc) == 0, "crc must be the first field");

/** @brief Pinned address field. */
typedef struct PBL_PACKED SprfPinnedAddress {
  /** Field CRC. */
  uint32_t crc;
  /** Pinned address of the device most recently paired. */
  struct pbl_bt_addr pinned_address;
  /** Reserved. */
  uint8_t rsvd[2];
} SprfPinnedAddress;
_Static_assert(offsetof(SprfPinnedAddress, crc) == 0, "crc must be the first field");

/** @brief Onboarding (getting started) field. */
typedef struct PBL_PACKED SprfGettingStarted {
  /** Field CRC. */
  uint32_t crc;
  /** Onboarding has been completed. */
  bool is_complete;
  /** Reserved. */
  uint8_t rsvd[3];
} SprfGettingStarted;
_Static_assert(offsetof(SprfGettingStarted, crc) == 0, "crc must be the first field");

/** @brief Custom local device name field. */
typedef struct PBL_PACKED SprfLocalName {
  /** Field CRC. */
  uint32_t crc;
  /** Local device name, replacing the default "Pebble XXXX". */
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
} SprfLocalName;
_Static_assert(offsetof(SprfLocalName, crc) == 0, "crc must be the first field");

/**
 * @brief Local identity address field.
 *
 * Carved out of @c main_fw_scratch, so firmware that predates it neither
 * validates it nor drops it when rewriting the entry. An invalid value only reads as absent.
 */
typedef struct PBL_PACKED SprfLocalIdentityAddress {
  /** Field CRC. */
  uint32_t crc;
  /** Random static identity address of the watch, for controllers without one of their own. */
  struct pbl_bt_addr address;
  /** Reserved. */
  uint8_t rsvd[2];
} SprfLocalIdentityAddress;
_Static_assert(offsetof(SprfLocalIdentityAddress, crc) == 0, "crc must be the first field");

/** @brief Shared PRF storage entry. */
typedef struct PBL_PACKED SharedPRFData {
  /** Entry state. */
  SprfMagic magic;
  /** Layout version, currently 2. */
  uint8_t version;
  /** Reserved. */
  uint8_t rsvd[3];

  /** BLE root keys. */
  SprfRootKeys root_keys;
  /** BLE pairing. */
  SprfBlePairingData ble_pairing_data;
  /** BLE pairing device name. */
  SprfBlePairingName ble_pairing_name;
  /** Pinned address. */
  SprfPinnedAddress pinned_address;
  /** Onboarding state. */
  SprfGettingStarted getting_started;
  /** Custom local device name. */
  SprfLocalName local_name;
  /** Local identity address. */
  SprfLocalIdentityAddress local_identity_address;

  /** Area reserved for the normal firmware to stash pairing related data. */
  struct PBL_PACKED {
    /** Reserved. */
    uint8_t rsvd[32];
  } main_fw_scratch;
} SharedPRFData;

_Static_assert(PBL_BT_DEVICE_NAME_BUFFER_SIZE == 20, "Changing the length will break SharedPRF");
_Static_assert(sizeof(SharedPRFData) == 256, "SharedPRFData does not match expected size");

/** @} */
