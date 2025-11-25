/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/graphics/gtypes.h"
#include "mfg/mfg_info.h"
#include "mfg/snowy/mfg_private.h"
#include "flash_region/flash_region_s29vs.h"

#include "fake_spi_flash.c"
#include "stubs_logging.h"
#include "stubs_pbl_malloc.h"

// This will come from overrides and will include a smaller boot fpga image
#include "mfg/spalding/spalding_boot.fpga.auto.h"

// Stubs for fake_crc.c
#include "services/normal/filesystem/pfs.h"
int pfs_read(int fd, void *buf, size_t size) { return 0; }
int pfs_seek(int fd, int offset, FSeekType seek_type) { return 0; }

// Test Code!

void test_spalding_mfg_info__initialize(void) {
  fake_spi_flash_init(FLASH_REGION_MFG_INFO_BEGIN,
                      FLASH_REGION_MFG_INFO_END - FLASH_REGION_MFG_INFO_BEGIN);
}

void test_spalding_mfg_info__cleanup(void) {
  fake_spi_flash_cleanup();
}

void test_spalding_mfg_info__color(void) {
  cl_assert_equal_i(mfg_info_get_watch_color(), 0);
  
  mfg_info_set_watch_color(WATCH_INFO_COLOR_RED);
  cl_assert_equal_i(mfg_info_get_watch_color(), WATCH_INFO_COLOR_RED);
  
  mfg_info_set_watch_color(WATCH_INFO_COLOR_GREEN);
  cl_assert_equal_i(mfg_info_get_watch_color(), WATCH_INFO_COLOR_GREEN);
}

void test_spalding_mfg_info__rtc_freq(void) {
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 0);
  
  mfg_info_set_rtc_freq(0xfefefefe);
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 0xfefefefe);
  
  mfg_info_set_rtc_freq(1337);
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 1337);
}

void test_spalding_mfg_info__model(void) {
  // Intentionally make the buffer too long so we can check for truncation.
  char buffer[MFG_INFO_MODEL_STRING_LENGTH + 1] = { 0 };
  
  mfg_info_get_model(buffer);
  cl_assert_equal_s(buffer, "");
  
  mfg_info_set_model("test_model");
  
  mfg_info_get_model(buffer);
  cl_assert_equal_s(buffer, "test_model");
  
  {
    char long_string[] = "01234567890123456789";
    mfg_info_set_model(long_string);
    
    // We only expect to see the first 15 (MFG_INFO_MODEL_STRING_LENGTH - 1) characters
    mfg_info_get_model(buffer);
    cl_assert_equal_s(buffer, "012345678901234");
  }
}

void test_spalding_mfg_info__1_to_2_conversion(void) {
  // Force in an old data version.
  typedef struct {
    uint32_t data_version;
    
    uint32_t color;
    uint32_t rtc_freq;
  } MfgDataV1;
  
  MfgDataV1 old_data = {
    .data_version = 1,
    .color = 3,
    .rtc_freq = 4
  };
  
  flash_write_bytes((const uint8_t*) &old_data, FLASH_REGION_MFG_INFO_BEGIN, sizeof(old_data));
  
  // Now use the info functions to read the data and make sure it's sane. A conversion will have
  // happened behind the scenes to the latest version.
  cl_assert_equal_i(mfg_info_get_watch_color(), 3);
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 4);
  
  char buffer[MFG_INFO_MODEL_STRING_LENGTH] = { 0 };
  mfg_info_get_model(buffer);
  cl_assert_equal_s(buffer, "");
  
  // Set color and make sure others don't change.
  mfg_info_set_watch_color(5);
  
  cl_assert_equal_i(mfg_info_get_watch_color(), 5);
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 4);
  
  mfg_info_get_model(buffer);
  cl_assert_equal_s(buffer, "");
  
  // Make sure we have space for the model.
  mfg_info_set_model("test_model");
  
  cl_assert_equal_i(mfg_info_get_watch_color(), 5);
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 4);
  
  mfg_info_get_model(buffer);
  cl_assert_equal_s(buffer, "test_model");
}


void test_spalding_mfg_info__1_to_3_conversion(void) {
  // Force in an old data version.
  typedef struct {
    uint32_t data_version;
    
    uint32_t color;
    uint32_t rtc_freq;
  } MfgDataV1;
  
  MfgDataV1 old_data = {
    .data_version = 1,
    .color = 3,
    .rtc_freq = 4
  };
  
  flash_write_bytes((const uint8_t*) &old_data, FLASH_REGION_MFG_INFO_BEGIN, sizeof(old_data));
  
  // Now use the info functions to read the data and make sure it's sane. A conversion will have
  // happened behind the scenes to the latest version.
  cl_assert_equal_i(mfg_info_get_watch_color(), 3);
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 4);
  
  char buffer[MFG_INFO_MODEL_STRING_LENGTH] = { 0 };
  mfg_info_get_model(buffer);
  cl_assert_equal_s(buffer, "");
  
  cl_assert_equal_i(mfg_info_get_disp_offsets().x, 0);
  cl_assert_equal_i(mfg_info_get_disp_offsets().y, 0);
  
  // Set x and y offsets and make sure others don't change.
  mfg_info_set_disp_offsets((GPoint) {-2, 1});

  cl_assert_equal_i(mfg_info_get_disp_offsets().x, -2);
  cl_assert_equal_i(mfg_info_get_disp_offsets().y, 1);
  
  cl_assert_equal_i(mfg_info_get_watch_color(), 3);
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 4);
  
  mfg_info_get_model(buffer);
  cl_assert_equal_s(buffer, "");
  
  // Make sure we have space for the model.
  mfg_info_set_model("test_model");
  
  cl_assert_equal_i(mfg_info_get_disp_offsets().x, -2);
  cl_assert_equal_i(mfg_info_get_disp_offsets().y, 1);
  
  cl_assert_equal_i(mfg_info_get_watch_color(), 3);
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 4);
  
  mfg_info_get_model(buffer);
  cl_assert_equal_s(buffer, "test_model");
}

void test_spalding_mfg_info__2_to_3_conversion(void) {
  // Force in an old data version.
  typedef struct {
    uint32_t data_version;
    
    uint32_t color;
    uint32_t rtc_freq;
    char model[MFG_INFO_MODEL_STRING_LENGTH];
  } MfgDataV2;
  
  MfgDataV2 old_data = {
    .data_version = 1,
    .color = 3,
    .rtc_freq = 4,
    .model[0] = '\0'
  };
  
  flash_write_bytes((const uint8_t*) &old_data, FLASH_REGION_MFG_INFO_BEGIN, sizeof(old_data));
  
  // Now use the info functions to read the data and make sure it's sane. A conversion will have
  // happened behind the scenes to the latest version.
  cl_assert_equal_i(mfg_info_get_watch_color(), 3);
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 4);
  
  char buffer[MFG_INFO_MODEL_STRING_LENGTH] = { 0 };
  mfg_info_get_model(buffer);
  cl_assert_equal_s(buffer, "");
  
  cl_assert_equal_i(mfg_info_get_disp_offsets().x, 0);
  cl_assert_equal_i(mfg_info_get_disp_offsets().y, 0);
  
  // Set x and y offsets and make sure others don't change.
  mfg_info_set_disp_offsets((GPoint) {-2, 1});
  
  cl_assert_equal_i(mfg_info_get_disp_offsets().x, -2);
  cl_assert_equal_i(mfg_info_get_disp_offsets().y, 1);
  
  cl_assert_equal_i(mfg_info_get_watch_color(), 3);
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 4);
  
  mfg_info_get_model(buffer);
  cl_assert_equal_s(buffer, "");
  
  // Make sure we have space for the model.
  mfg_info_set_model("test_model");
  
  cl_assert_equal_i(mfg_info_get_disp_offsets().x, -2);
  cl_assert_equal_i(mfg_info_get_disp_offsets().y, 1);
  
  cl_assert_equal_i(mfg_info_get_watch_color(), 3);
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 4);
  
  mfg_info_get_model(buffer);
  cl_assert_equal_s(buffer, "test_model");
}

void test_spalding_mfg_info__boot_fpga_persistence(void) {
  // Make sure no FPGA image is stored
  const uintptr_t BOOT_FPGA_FLASH_ADDR = FLASH_REGION_MFG_INFO_BEGIN + 0x10000;
  const uint32_t HEADER_SIZE = 4; // sizeof(BootFPGAHeader)

  uint8_t fpga_buffer[HEADER_SIZE + sizeof(s_boot_fpga)];
  flash_read_bytes(fpga_buffer, BOOT_FPGA_FLASH_ADDR, sizeof(fpga_buffer));
  for (int i = 0; i < sizeof(s_boot_fpga); ++i) {
    cl_assert(fpga_buffer[i] == 0xff);
  }

  // Write some data in
  mfg_info_set_rtc_freq(1);

  // The first time we write something into mfg_info we'll actually write the boot fpga as a side
  // effect. Make sure it's there.
  flash_read_bytes(fpga_buffer, BOOT_FPGA_FLASH_ADDR, sizeof(fpga_buffer));
  cl_assert(memcmp(fpga_buffer + HEADER_SIZE, s_boot_fpga, sizeof(s_boot_fpga)) == 0);

  mfg_info_set_disp_offsets((GPoint) { 2, 3 });

  char model[MFG_INFO_MODEL_STRING_LENGTH] = "123456789012345";
  mfg_info_set_model(model);

  // Now let's write in an fpga image
  mfg_info_update_constant_data();

  // Let's make sure the mfg data is still persisted
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 1);
  cl_assert_equal_i(mfg_info_get_disp_offsets().x, 2);
  cl_assert_equal_i(mfg_info_get_disp_offsets().y, 3);

  char result_model[MFG_INFO_MODEL_STRING_LENGTH];
  mfg_info_get_model(result_model);
  cl_assert_equal_s(model, result_model);

  // Make sure the boot fpga is still correct
  flash_read_bytes(fpga_buffer, BOOT_FPGA_FLASH_ADDR, sizeof(fpga_buffer));
  cl_assert(memcmp(fpga_buffer + HEADER_SIZE, s_boot_fpga, sizeof(s_boot_fpga)) == 0);

  // Now invalidate the section and write it back. Make sure it comes back
  flash_write_bytes((const uint8_t*) "xxxx", BOOT_FPGA_FLASH_ADDR + HEADER_SIZE, 4);

  // Make sure it's corrupted
  flash_read_bytes(fpga_buffer, BOOT_FPGA_FLASH_ADDR, sizeof(fpga_buffer));
  cl_assert(memcmp(fpga_buffer + HEADER_SIZE, s_boot_fpga, sizeof(s_boot_fpga)) != 0);

  // Now update it and make sure we healed the corruption
  mfg_info_update_constant_data();

  flash_read_bytes(fpga_buffer, BOOT_FPGA_FLASH_ADDR, sizeof(fpga_buffer));
  cl_assert(memcmp(fpga_buffer + HEADER_SIZE, s_boot_fpga, sizeof(s_boot_fpga)) == 0);
}
