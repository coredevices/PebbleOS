/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "mfg/mfg_info.h"
#include "mfg/snowy/mfg_private.h"
#include "flash_region/flash_region_s29vs.h"

#include "fake_spi_flash.c"
#include "stubs_logging.h"

void mfg_info_write_boot_fpga_bitstream(void) {}
bool mfg_info_is_boot_fpga_bitstream_written(void) { return true; }

void test_snowy_mfg_info__initialize(void) {
  fake_spi_flash_init(FLASH_REGION_MFG_INFO_BEGIN,
                      FLASH_REGION_MFG_INFO_END - FLASH_REGION_MFG_INFO_BEGIN);
}

void test_snowy_mfg_info__cleanup(void) {
  fake_spi_flash_cleanup();
}

void test_snowy_mfg_info__color(void) {
  cl_assert_equal_i(mfg_info_get_watch_color(), 0);

  mfg_info_set_watch_color(WATCH_INFO_COLOR_RED);
  cl_assert_equal_i(mfg_info_get_watch_color(), WATCH_INFO_COLOR_RED);

  mfg_info_set_watch_color(WATCH_INFO_COLOR_GREEN);
  cl_assert_equal_i(mfg_info_get_watch_color(), WATCH_INFO_COLOR_GREEN);
}

void test_snowy_mfg_info__rtc_freq(void) {
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 0);

  mfg_info_set_rtc_freq(0xfefefefe);
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 0xfefefefe);

  mfg_info_set_rtc_freq(1337);
  cl_assert_equal_i(mfg_info_get_rtc_freq(), 1337);
}

void test_snowy_mfg_info__model(void) {
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

void test_snowy_mfg_info__1_to_2_conversion(void) {
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


