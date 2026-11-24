/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/board/partitions.h>
#include <pbl/drivers/flash.h>
#include <pbl/services/prf_update.h>
#include <pbl/util/units.h>

#include <clar.h>
#include <fake_bootbits.h>
#include <fake_spi_flash.h>
#include <pebbleos/firmware_metadata.h>
#include <system/bootbits.h>
#include <system/firmware_storage.h>

#ifdef CONFIG_PBLBOOT
typedef FirmwareHeader StagedHeader;
#else
typedef FirmwareDescription StagedHeader;
#endif

#define FIRMWARE_LENGTH 4096U
#define TOTAL_LENGTH    (sizeof(StagedHeader) + FIRMWARE_LENGTH)

static const uint32_t PENDING_BITS = BOOT_BIT_NEW_PRF_AVAILABLE |
                                     BOOT_BIT_PRF_UPDATE_FAIL_STRIKE_ONE |
                                     BOOT_BIT_PRF_UPDATE_FAIL_STRIKE_TWO;

// Whether the CRC of the staged image, and of the copy in the PRF slot, hold
static bool s_staged_crc_ok;
static bool s_slot_crc_ok;

// Fakes
//////////////////////////////////////////////////////////

static StagedHeader prv_read_header(uint32_t address) {
  StagedHeader header;

  cl_assert_equal_i(pbl_flash_read(FLASH, address, &header, sizeof(header)), 0);
  return header;
}

static bool prv_check_valid(uint32_t firmware_start_address) {
  if (firmware_start_address == PART_FIRMWARE_DEST_OFFSET) {
    return s_staged_crc_ok;
  }

  cl_assert_equal_i(firmware_start_address, PART_SAFE_FIRMWARE_OFFSET);
  return s_slot_crc_ok;
}

#ifdef CONFIG_PBLBOOT
FirmwareHeader firmware_storage_read_firmware_header(uint32_t address) {
  return prv_read_header(address);
}

bool firmware_storage_check_valid_firmware_header(uint32_t address, const FirmwareHeader *header) {
  return prv_check_valid(address);
}
#else
FirmwareDescription firmware_storage_read_firmware_description(uint32_t firmware_start_address) {
  return prv_read_header(firmware_start_address);
}

bool firmware_storage_check_valid_firmware_description(
    uint32_t firmware_start_address, const FirmwareDescription *firmware_description) {
  return prv_check_valid(firmware_start_address);
}
#endif

// Helpers
//////////////////////////////////////////////////////////

static void prv_stage(uint32_t total_length, bool is_recovery_firmware) {
#ifdef CONFIG_PBLBOOT
  const StagedHeader header = {
    .magic = FIRMWARE_HEADER_MAGIC,
    .header_length = sizeof(StagedHeader),
    .fw_start = sizeof(StagedHeader),
    .fw_length = total_length - sizeof(StagedHeader),
  };
#else
  const StagedHeader header = {
    .description_length = sizeof(StagedHeader),
    .firmware_length = total_length - sizeof(StagedHeader),
  };
#endif
  const FirmwareMetadata metadata = {
    .is_recovery_firmware = is_recovery_firmware,
    .hw_platform = FIRMWARE_METADATA_HW_PLATFORM,
    .metadata_version = FW_METADATA_CURRENT_STRUCT_VERSION,
  };

  cl_assert_equal_i(pbl_flash_erase(FLASH, PART_FIRMWARE_DEST_OFFSET, PBL_KIB(64)), 0);
  cl_assert_equal_i(pbl_flash_write(FLASH, PART_FIRMWARE_DEST_OFFSET, &header, sizeof(header)), 0);
  if (total_length <= PBL_KIB(64)) {
    cl_assert_equal_i(
        pbl_flash_write(FLASH, PART_FIRMWARE_DEST_OFFSET + total_length - sizeof(metadata),
                        &metadata, sizeof(metadata)),
        0);
  }
  fake_boot_bit_set(BOOT_BIT_NEW_PRF_AVAILABLE);
}

static bool prv_slot_holds_staged(void) {
  static uint8_t staged[TOTAL_LENGTH];
  static uint8_t slot[TOTAL_LENGTH];

  cl_assert_equal_i(pbl_flash_read(FLASH, PART_FIRMWARE_DEST_OFFSET, staged, sizeof(staged)), 0);
  cl_assert_equal_i(pbl_flash_read(FLASH, PART_SAFE_FIRMWARE_OFFSET, slot, sizeof(slot)), 0);
  return memcmp(staged, slot, sizeof(staged)) == 0;
}

// Tests
//////////////////////////////////////////////////////////

void test_prf_update__initialize(void) {
  fake_spi_flash_init(0, PART_SAFE_FIRMWARE_OFFSET + PART_SAFE_FIRMWARE_SIZE);
  fake_boot_bits_reset();
  s_staged_crc_ok = true;
  s_slot_crc_ok = true;
}

void test_prf_update__cleanup(void) {
  fake_spi_flash_cleanup();
}

void test_prf_update__nothing_pending(void) {
  check_prf_update();

  cl_assert_equal_i(fake_flash_erase_count(), 0);
  cl_assert_equal_i(fake_boot_bits_get(), 0);
}

void test_prf_update__good_image_is_copied(void) {
  prv_stage(TOTAL_LENGTH, true);

  check_prf_update();

  cl_assert(prv_slot_holds_staged());
  cl_assert_equal_i(fake_boot_bits_get() & PENDING_BITS, 0);
}

void test_prf_update__bad_crc_is_dropped(void) {
  prv_stage(TOTAL_LENGTH, true);
  s_staged_crc_ok = false;
  const uint32_t erases = fake_flash_erase_count();

  check_prf_update();

  cl_assert_equal_i(fake_flash_erase_count(), erases);
  cl_assert_equal_i(fake_boot_bits_get() & PENDING_BITS, 0);
}

void test_prf_update__too_large_is_dropped(void) {
  prv_stage(PART_SAFE_FIRMWARE_SIZE + 1U, true);
  const uint32_t erases = fake_flash_erase_count();

  check_prf_update();

  cl_assert_equal_i(fake_flash_erase_count(), erases);
  cl_assert_equal_i(fake_boot_bits_get() & PENDING_BITS, 0);
}

void test_prf_update__normal_firmware_is_dropped(void) {
  prv_stage(TOTAL_LENGTH, false);
  const uint32_t erases = fake_flash_erase_count();

  check_prf_update();

  cl_assert_equal_i(fake_flash_erase_count(), erases);
  cl_assert_equal_i(fake_boot_bits_get() & PENDING_BITS, 0);
}

void test_prf_update__failed_copy_is_retried_twice(void) {
  prv_stage(TOTAL_LENGTH, true);
  s_slot_crc_ok = false;
  const uint32_t erases = fake_flash_erase_count();

  check_prf_update();
  cl_assert_equal_i(fake_boot_bits_get() & PENDING_BITS,
                    BOOT_BIT_NEW_PRF_AVAILABLE | BOOT_BIT_PRF_UPDATE_FAIL_STRIKE_ONE);

  check_prf_update();
  cl_assert_equal_i(fake_boot_bits_get() & PENDING_BITS, PENDING_BITS);

  check_prf_update();
  cl_assert_equal_i(fake_boot_bits_get() & PENDING_BITS, 0);
  const uint32_t erases_after_three = fake_flash_erase_count();
  cl_assert(erases_after_three > erases);

  // Given up: later boots leave the slot alone
  check_prf_update();
  cl_assert_equal_i(fake_flash_erase_count(), erases_after_three);
}

void test_prf_update__retry_success_clears_strikes(void) {
  prv_stage(TOTAL_LENGTH, true);
  s_slot_crc_ok = false;

  check_prf_update();
  cl_assert(fake_boot_bits_get() & BOOT_BIT_PRF_UPDATE_FAIL_STRIKE_ONE);

  s_slot_crc_ok = true;
  check_prf_update();

  cl_assert(prv_slot_holds_staged());
  cl_assert_equal_i(fake_boot_bits_get() & PENDING_BITS, 0);
}
