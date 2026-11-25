/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/util/bits.h>

/**
 * @file
 * On-flash layout of the PRF store, as records.
 *
 * The partition is a log of records of one size. A record holds everything the store keeps, so the
 * last one is the store: a change appends a record, and the partition is erased when none fits. A
 * record counts if its magic and CRC are good; one of a version that is not known cannot be read,
 * and leaves the store empty.
 */

/** @brief "PRFS". */
#define PRF_RECORD_MAGIC 0x53465250U

/** @brief Version of the record. */
#define PRF_RECORD_VERSION 1U

/** @brief Size of a stored device name, including the terminator. */
#define PRF_RECORD_NAME_SIZE 20

/** @brief Number of root keys: Encryption Root (ER), then Identity Root (IR). */
#define PRF_RECORD_NUM_ROOT_KEYS 2

/** @brief Size of a key. */
#define PRF_RECORD_KEY_SIZE 16

/**
 * @name Flags of a record
 * @{
 */
/** The root keys are there. */
#define PRF_RECORD_FLAG_ROOT_KEYS PBL_BIT(0)
/** A pairing is there: the peer's address, and the keys the flags below tell. */
#define PRF_RECORD_FLAG_PAIRING PBL_BIT(1)
/** The peer's LTK, EDIV and Rand are there. */
#define PRF_RECORD_FLAG_PEER_LTK PBL_BIT(2)
/** The peer's IRK is there. */
#define PRF_RECORD_FLAG_PEER_IRK PBL_BIT(3)
/** The LTK, EDIV and Rand of the watch for the peer are there. */
#define PRF_RECORD_FLAG_OUR_LTK PBL_BIT(4)
/** Paired with Secure Connections. */
#define PRF_RECORD_FLAG_SECURE_CONNECTIONS PBL_BIT(5)
/** The pairing was authenticated (MITM protected). */
#define PRF_RECORD_FLAG_AUTHENTICATED PBL_BIT(6)
/** @} */

/** @brief A record. */
struct PBL_PACKED prf_record {
  /** @c PRF_RECORD_MAGIC. */
  uint32_t magic;
  /** CRC-32 (pbl_crc32()) of what follows. */
  uint32_t crc;
  /** @c PRF_RECORD_VERSION. */
  uint8_t version;
  /** @c PRF_RECORD_FLAG_* values. */
  uint8_t flags;
  /** Type of the peer's identity address: 0 public, 1 random. */
  uint8_t peer_addr_type;
  /** The peer's identity address, least significant octet first. */
  uint8_t peer_addr[6];
  /** Reserved, zero. */
  uint8_t rsvd[3];
  /** EDIV of the peer's LTK. */
  uint16_t peer_ediv;
  /** EDIV of the watch's LTK. */
  uint16_t our_ediv;
  /** Rand of the peer's LTK. */
  uint64_t peer_rand;
  /** Rand of the watch's LTK. */
  uint64_t our_rand;
  /** The peer's LTK. */
  uint8_t peer_ltk[PRF_RECORD_KEY_SIZE];
  /** The peer's IRK. */
  uint8_t peer_irk[PRF_RECORD_KEY_SIZE];
  /** The watch's LTK. */
  uint8_t our_ltk[PRF_RECORD_KEY_SIZE];
  /** The root keys. */
  uint8_t root_keys[PRF_RECORD_NUM_ROOT_KEYS][PRF_RECORD_KEY_SIZE];
  /** Name of the peer, terminated; empty if there is none. */
  char name[PRF_RECORD_NAME_SIZE];
};

static_assert(sizeof(struct prf_record) == 140);
