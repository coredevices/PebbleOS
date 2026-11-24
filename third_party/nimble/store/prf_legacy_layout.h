/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

/**
 * @file
 * On-flash layout of the PRF store entries. Recovery firmware in the field reads it, so it cannot
 * change: its types are its own, and are used nowhere else.
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
 */

/** @brief Size of a stored device name, including the terminator. */
#define SPRF_NAME_SIZE 20

/** @brief Number of root keys: Encryption Root (ER), then Identity Root (IR). */
#define SPRF_NUM_ROOT_KEYS 2

/** @brief A 128-bit key. */
struct PBL_PACKED sprf_key {
  uint8_t data[16];
};

/** @brief A device address, least significant octet first. */
struct PBL_PACKED sprf_addr {
  uint8_t octets[6];
};

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
static_assert(sizeof(SprfValidFields) == 1, "SprfValidFields unexpected size");
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

static_assert(sizeof(SprfMagic) == 4, "SprfMagic unexpected size");

/** @brief BLE root keys field. */
typedef struct PBL_PACKED SprfRootKeys {
  /** Field CRC. */
  uint32_t crc;
  /** The keys. */
  struct sprf_key keys[SPRF_NUM_ROOT_KEYS];
} SprfRootKeys;
static_assert(offsetof(SprfRootKeys, crc) == 0, "crc must be the first field");

/** @brief Pairing flag: paired with Secure Connections. */
#define SPRF_PAIRING_FLAG_SECURE_CONNECTIONS 0x01U
/** @brief Pairing flag: the pairing was authenticated (MITM protected). */
#define SPRF_PAIRING_FLAG_AUTHENTICATED 0x02U

/** @brief Identity flag: the identity address is a random one, a public one otherwise. */
#define SPRF_IDENTITY_FLAG_RANDOM 0x02U

/** @brief BLE pairing field, for the device most recently paired. */
typedef struct PBL_PACKED SprfBlePairingData {
  /** Field CRC. */
  uint32_t crc;

  /** Local LTK. */
  struct sprf_key l_ltk;
  /** Local Rand. */
  uint64_t l_rand;
  /** Local EDIV. */
  uint16_t l_ediv;

  /** Remote EDIV. */
  uint16_t r_ediv;
  /** Remote LTK. */
  struct sprf_key r_ltk;
  /** Remote Rand. */
  uint64_t r_rand;

  /** Remote IRK. */
  struct sprf_key irk;
  /** Remote CSRK. */
  struct sprf_key csrk;
  /** Remote identity address. */
  struct sprf_addr identity_addr;
  /** @c SPRF_IDENTITY_FLAG_* values. */
  uint8_t identity_flags;
  /** Reserved, zero. */
  uint8_t identity_rsvd;

  /** Valid parts, @ref SprfValidFields bits. 0 means no pairing. */
  SprfValidFields fields : 8;
  /** Pairing has MITM protection. */
  bool is_mitm_protection_enabled;
  /** Unused, always false. Older firmware pinned the local address when set. */
  bool requires_address_pinning;

  /** @c SPRF_PAIRING_FLAG_* values. Added in version 2, always 0 in version 1. */
  uint8_t flags;
} SprfBlePairingData;
static_assert(offsetof(SprfBlePairingData, crc) == 0, "crc must be the first field");

/** @brief BLE pairing device name field. */
typedef struct PBL_PACKED SprfBlePairingName {
  /** Field CRC. */
  uint32_t crc;
  /** Remote device name. */
  char name[SPRF_NAME_SIZE];
} SprfBlePairingName;
static_assert(offsetof(SprfBlePairingName, crc) == 0, "crc must be the first field");

/** @brief Pinned address field. Unused, older firmware stored its pinned address here. */
typedef struct PBL_PACKED SprfPinnedAddress {
  /** Field CRC. */
  uint32_t crc;
  /** Pinned address of the device most recently paired. */
  struct sprf_addr pinned_address;
  /** Reserved. */
  uint8_t rsvd[2];
} SprfPinnedAddress;
static_assert(offsetof(SprfPinnedAddress, crc) == 0, "crc must be the first field");

/** @brief Onboarding (getting started) field. */
typedef struct PBL_PACKED SprfGettingStarted {
  /** Field CRC. */
  uint32_t crc;
  /** Onboarding has been completed. */
  bool is_complete;
  /** Reserved. */
  uint8_t rsvd[3];
} SprfGettingStarted;
static_assert(offsetof(SprfGettingStarted, crc) == 0, "crc must be the first field");

/** @brief Custom local device name field. */
typedef struct PBL_PACKED SprfLocalName {
  /** Field CRC. */
  uint32_t crc;
  /** Local device name, replacing the default "Pebble XXXX". */
  char name[SPRF_NAME_SIZE];
} SprfLocalName;
static_assert(offsetof(SprfLocalName, crc) == 0, "crc must be the first field");

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
  struct sprf_addr address;
  /** Reserved. */
  uint8_t rsvd[2];
} SprfLocalIdentityAddress;
static_assert(offsetof(SprfLocalIdentityAddress, crc) == 0, "crc must be the first field");

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

static_assert(sizeof(SharedPRFData) == 256, "SharedPRFData does not match expected size");

// Where recovery firmware in the field looks for each field.
static_assert(offsetof(SprfBlePairingData, l_ltk) == 4);
static_assert(offsetof(SprfBlePairingData, l_rand) == 20);
static_assert(offsetof(SprfBlePairingData, l_ediv) == 28);
static_assert(offsetof(SprfBlePairingData, r_ediv) == 30);
static_assert(offsetof(SprfBlePairingData, r_ltk) == 32);
static_assert(offsetof(SprfBlePairingData, r_rand) == 48);
static_assert(offsetof(SprfBlePairingData, irk) == 56);
static_assert(offsetof(SprfBlePairingData, csrk) == 72);
static_assert(offsetof(SprfBlePairingData, identity_addr) == 88);
static_assert(offsetof(SprfBlePairingData, identity_flags) == 94);
static_assert(offsetof(SprfBlePairingData, is_mitm_protection_enabled) == 97);
static_assert(offsetof(SprfBlePairingData, requires_address_pinning) == 98);
static_assert(offsetof(SprfBlePairingData, flags) == 99);
static_assert(sizeof(SprfBlePairingData) == 100);
static_assert(offsetof(SharedPRFData, root_keys) == 8);
static_assert(offsetof(SharedPRFData, ble_pairing_data) == 44);
static_assert(offsetof(SharedPRFData, ble_pairing_name) == 144);
static_assert(offsetof(SharedPRFData, pinned_address) == 168);
static_assert(offsetof(SharedPRFData, getting_started) == 180);
static_assert(offsetof(SharedPRFData, local_name) == 188);
static_assert(offsetof(SharedPRFData, local_identity_address) == 212);
static_assert(offsetof(SharedPRFData, main_fw_scratch) == 224);
