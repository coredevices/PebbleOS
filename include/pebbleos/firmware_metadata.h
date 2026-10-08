/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

/**
 * @defgroup pebbleos_firmware_metadata Firmware metadata
 * @ingroup pebbleos
 * @brief Build information stored at the end of every firmware image.
 *
 * The firmware places a @ref FirmwareMetadata in its @c .pbl_fw_version section, linked last, so
 * a @c .bin image ends with the 20-byte GNU build ID followed by this structure. The firmware
 * reads it back from flash for the recovery and update images, and host tools
 * (@c tools/fw_binary_info.py, the GDB scripts) parse it from @c .bin and @c .elf files. All
 * fields are little-endian.
 * @{
 */

/** @brief Value of @ref FirmwareMetadata::metadata_version written by this firmware. */
#define FW_METADATA_CURRENT_STRUCT_VERSION 0x1
/** @brief Size of @ref FirmwareMetadata::version_short in bytes. */
#define FW_METADATA_VERSION_SHORT_BYTES 8
/** @brief Size of @ref FirmwareMetadata::version_tag in bytes. */
#define FW_METADATA_VERSION_TAG_BYTES 32

/**
 * @brief Hardware platform a firmware image is built for.
 *
 * Values are stable: they are stored in images. Bigboards (development boards) and emulators
 * count down from 0xff.
 *
 * @note When adding a platform that uses the legacy CRC, add it to the list in
 * @c tools/fw_binary_info.py.
 */
typedef enum FirmwareMetadataPlatform {
  /** Unknown platform. */
  FirmwareMetadataPlatformUnknown = 0,
  /** Original Pebble, EV1. */
  FirmwareMetadataPlatformPebbleOneEV1 = 1,
  /** Original Pebble, EV2. */
  FirmwareMetadataPlatformPebbleOneEV2 = 2,
  /** Original Pebble, EV2.3. */
  FirmwareMetadataPlatformPebbleOneEV2_3 = 3,
  /** Original Pebble, EV2.4. */
  FirmwareMetadataPlatformPebbleOneEV2_4 = 4,
  /** Original Pebble, hardware 1.5. */
  FirmwareMetadataPlatformPebbleOnePointFive = 5,
  /** Original Pebble, hardware 2.0. */
  FirmwareMetadataPlatformPebbleTwoPointZero = 6,
  /** Snowy (Pebble Time), EVT2. */
  FirmwareMetadataPlatformPebbleSnowyEVT2 = 7,
  /** Snowy (Pebble Time), DVT. */
  FirmwareMetadataPlatformPebbleSnowyDVT = 8,
  /** Spalding (Pebble Time Round), EVT. */
  FirmwareMetadataPlatformPebbleSpaldingEVT = 9,
  /** Bobby (Pebble Time Steel), DVT. */
  FirmwareMetadataPlatformPebbleBobbyDVT = 10,
  /** Spalding (Pebble Time Round), production. */
  FirmwareMetadataPlatformPebbleSpalding = 11,
  /** Silk (Pebble 2), EVT. */
  FirmwareMetadataPlatformPebbleSilkEVT = 12,
  /** Robert (Pebble Time 2), EVT. */
  FirmwareMetadataPlatformPebbleRobertEVT = 13,
  /** Silk (Pebble 2), production. */
  FirmwareMetadataPlatformPebbleSilk = 14,
  /** Asterix (Pebble 2 Duo). */
  FirmwareMetadataPlatformPebbleAsterix = 15,
  /** Obelix (Pebble Time 2), EVT. */
  FirmwareMetadataPlatformPebbleObelixEVT = 16,
  /** Obelix (Pebble Time 2), DVT. */
  FirmwareMetadataPlatformPebbleObelixDVT = 17,
  /** Obelix (Pebble Time 2), PVT. */
  FirmwareMetadataPlatformPebbleObelixPVT = 18,
  /** Getafix (Pebble Round 2), EVT. */
  FirmwareMetadataPlatformPebbleGetafixEVT = 19,
  /** Getafix (Pebble Round 2), DVT. */
  FirmwareMetadataPlatformPebbleGetafixDVT = 20,
  /** Getafix (Pebble Round 2), DVT2. */
  FirmwareMetadataPlatformPebbleGetafixDVT2 = 21,

  /** Original Pebble bigboard. */
  FirmwareMetadataPlatformPebbleOneBigboard = 0xff,
  /** Original Pebble bigboard, revision 2. */
  FirmwareMetadataPlatformPebbleOneBigboard2 = 0xfe,
  /** Snowy bigboard. */
  FirmwareMetadataPlatformPebbleSnowyBigboard = 0xfd,
  /** Snowy bigboard, revision 2. */
  FirmwareMetadataPlatformPebbleSnowyBigboard2 = 0xfc,
  /** Spalding bigboard. */
  FirmwareMetadataPlatformPebbleSpaldingBigboard = 0xfb,
  /** Silk bigboard. */
  FirmwareMetadataPlatformPebbleSilkBigboard = 0xfa,
  /** Robert bigboard. */
  FirmwareMetadataPlatformPebbleRobertBigboard = 0xf9,
  /** Silk bigboard, revision 2. */
  FirmwareMetadataPlatformPebbleSilkBigboard2 = 0xf8,
  /** Robert bigboard, revision 2. */
  FirmwareMetadataPlatformPebbleRobertBigboard2 = 0xf7,
  /** Flint emulator (QEMU). */
  FirmwareMetadataPlatformPebbleFlintEmu = 0xf6,
  /** Emery emulator (QEMU). */
  FirmwareMetadataPlatformPebbleEmeryEmu = 0xf5,
  /** Obelix bigboard. */
  FirmwareMetadataPlatformPebbleObelixBigboard = 0xf4,
  /** Obelix bigboard, revision 2. */
  FirmwareMetadataPlatformPebbleObelixBigboard2 = 0xf3,
  /** Gabbro emulator (QEMU). */
  FirmwareMetadataPlatformPebbleGabbroEmu = 0xf2,
} FirmwareMetadataPlatform;

// WARNING: changes in this struct must be reflected in:
// - iOS/PebblePrivateKit/PebblePrivateKit/PBBundle.m

/**
 * @brief Firmware image metadata, 47 bytes, packed.
 *
 * The four flags share one byte, starting at bit 0.
 */
struct PBL_PACKED FirmwareMetadata {
  /** Commit time of the build's source revision, in seconds since the Unix epoch. */
  uint32_t version_timestamp;
  /** Version from @c git @c describe, e.g. "v4.9.0", NUL-terminated, at most 31 characters. */
  char version_tag[FW_METADATA_VERSION_TAG_BYTES];
  /** Abbreviated commit hash (7 characters), NUL-terminated. */
  char version_short[FW_METADATA_VERSION_SHORT_BYTES];
  /** Recovery firmware (PRF) image. */
  bool is_recovery_firmware : 1;
  /** Bluetooth controller firmware image; always false for PebbleOS. */
  bool is_ble_firmware : 1;
  /** Built for the dual-slot layout of the pblboot bootloader. */
  bool is_dual_slot : 1;
  /** Built to run from slot 0 of a dual-slot layout; false for slot 1 and single-slot images. */
  bool is_slot_0 : 1;
  /** Reserved, 0. */
  uint8_t reserved : 4;
  /** Hardware platform, a @ref FirmwareMetadataPlatform value. */
  uint8_t hw_platform;
  /** Structure version, @ref FW_METADATA_CURRENT_STRUCT_VERSION; the last byte of the image. */
  uint8_t metadata_version;
};
/** @brief Firmware image metadata. */
typedef struct FirmwareMetadata FirmwareMetadata;

_Static_assert(sizeof(struct FirmwareMetadata) ==
                   (sizeof(uint32_t) + FW_METADATA_VERSION_SHORT_BYTES +
                    FW_METADATA_VERSION_TAG_BYTES + sizeof(uint8_t) + sizeof(uint8_t) +
                    sizeof(uint8_t)),
               "FirmwareMetadata bitfields not packed correctly");

/**
 * @brief Value of @ref FirmwareMetadata::is_recovery_firmware for this build.
 *
 * true when built with @c CONFIG_RECOVERY_FW.
 */
#ifdef CONFIG_RECOVERY_FW
#define FIRMWARE_METADATA_IS_RECOVERY_FIRMWARE (true)
#else
#define FIRMWARE_METADATA_IS_RECOVERY_FIRMWARE (false)
#endif

/**
 * @brief Value of @ref FirmwareMetadata::hw_platform for the board being built.
 *
 * @ref FirmwareMetadataPlatformUnknown for boards without an entry.
 */
#ifdef CONFIG_BOARD_ASTERIX
#define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleAsterix)
#elif defined(CONFIG_BOARD_OBELIX_DVT)
#define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleObelixDVT)
#elif defined(CONFIG_BOARD_OBELIX_PVT)
#define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleObelixPVT)
#elif defined(CONFIG_BOARD_OBELIX_BB2)
#define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleObelixBigboard2)
#elif defined(CONFIG_BOARD_GETAFIX_DVT)
#define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleGetafixDVT)
#elif defined(CONFIG_BOARD_GETAFIX_DVT2)
#define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleGetafixDVT2)
#elif defined(CONFIG_BOARD_QEMU_EMERY) || defined(CONFIG_BOARD_NATIVE_EMERY)
#define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleEmeryEmu)
#elif defined(CONFIG_BOARD_QEMU_FLINT)
#define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleFlintEmu)
#elif defined(CONFIG_BOARD_QEMU_GABBRO)
#define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleGabbroEmu)
#else
#define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformUnknown)
#endif

/** @} */
