/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mfg_command.h"

#include "applib/app_launch_reason.h"
#include "applib/app_watch_info.h"
#include "console/prompt.h"
#include "kernel/util/standby.h"
#include "mfg/mfg_info.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"

void command_enter_standby(void) {
  enter_standby(RebootReasonCode_MfgShutdown);
}

void command_color_read(void) {
  char buffer[10];
  prompt_send_response_fmt(buffer, sizeof(buffer), "%d", mfg_info_get_watch_color());
}

void command_color_write(const char* color_num) {
  char *end;
  int color = strtol(color_num, &end, 10);

  if (*end) {
    prompt_send_response("Invalid color");
    return;
  }

  mfg_info_set_watch_color(color);

  const WatchInfoColor written_color = mfg_info_get_watch_color();
  if (written_color == color) {
    prompt_send_response("OK");
  } else {
    prompt_send_response("ERROR");
  }
}

void command_disp_offset_read(void) {
  char buffer[16];
  prompt_send_response_fmt(buffer, sizeof(buffer), "X: %"PRId16" Y: %"PRId16,
                           mfg_info_get_disp_offsets().x,
                           mfg_info_get_disp_offsets().y);
}

void command_disp_offset_write(const char* offset_x_str, const char* offset_y_str) {
  char *nonnumeric_x, *nonnumeric_y;
  int8_t offset_x = strtol(offset_x_str, &nonnumeric_x, 10);
  if (*nonnumeric_x) {
    prompt_send_response("Invalid x offset");
  }

  int8_t offset_y = strtol(offset_y_str, &nonnumeric_y, 10);
  if (*nonnumeric_y) {
    prompt_send_response("Invalid y offset");
  }

  if (!*nonnumeric_x && !*nonnumeric_y) {
    mfg_info_set_disp_offsets((GPoint) {offset_x, offset_y});
  }
}

void command_rtcfreq_read(void) {
  char buffer[10];
  prompt_send_response_fmt(buffer, sizeof(buffer), "%"PRIu32, mfg_info_get_rtc_freq());
}

void command_rtcfreq_write(const char* rtc_freq_string) {
  char *end;
  uint32_t rtc_freq = strtol(rtc_freq_string, &end, 10);

  if (*end) {
    prompt_send_response("Invalid rtcfreq");
    return;
  }

  mfg_info_set_rtc_freq(rtc_freq);
}

void command_model_read(void) {
  char model_buffer[MFG_INFO_MODEL_STRING_LENGTH];
  mfg_info_get_model(model_buffer);

  // Just send it straight out, as it's already null-terminated
  prompt_send_response(model_buffer);
}

void command_model_write(const char* model) {
  // mfg_info_set_model will truncate if the string is too long, so no need to check
  mfg_info_set_model(model);
  char written_model[MFG_INFO_MODEL_STRING_LENGTH];
  mfg_info_get_model(written_model);
  if (!strncmp(model, written_model, MFG_INFO_MODEL_STRING_LENGTH)) {
    prompt_send_response("OK");
  } else {
    prompt_send_response("ERROR");
  }
}

#if BOOTLOADER_TEST_STAGE1
#include "bootloader_test_bin.auto.h"

#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "system/firmware_storage.h"
#include "system/bootbits.h"
#include "system/logging.h"
#include "system/reset.h"
#include "util/crc32.h"
#include "util/legacy_checksum.h"
static void prv_bootloader_test_copy(uint32_t flash_addr, uint32_t flash_end) {
  const uint8_t *bin;
  size_t size;

  bin = s_bootloader_test_stage2;
  size = sizeof(s_bootloader_test_stage2);
  flash_region_erase_optimal_range(flash_addr,
                                   flash_addr,
                                   flash_addr+size+sizeof(FirmwareDescription),
                                   flash_end);

  FirmwareDescription fw_desc = {
    .description_length = sizeof(FirmwareDescription),
    .firmware_length = size,
    .checksum = 0,
  };
#if CAPABILITY_HAS_DEFECTIVE_FW_CRC
  fw_desc.checksum = legacy_defective_checksum_memory(bin, size);
#else
  fw_desc.checksum = crc32(CRC32_INIT, bin, size);
#endif

  flash_write_bytes((uint8_t *)&fw_desc, flash_addr, sizeof(FirmwareDescription));
  flash_write_bytes(bin, flash_addr+sizeof(FirmwareDescription), size);
}

#define BLTEST_LOG(x...) pbl_log(LOG_LEVEL_ALWAYS, __FILE__, __LINE__, x)

void command_bootloader_test(const char *dest_type) {
  prompt_command_finish();

  BLTEST_LOG("BOOTLOADER TEST STAGE 1");
  boot_bit_set(BOOT_BIT_FW_STABLE);

  BLTEST_LOG("STAGE 1 -- Setting test boot bits");
  boot_bit_clear(BOOT_BIT_BOOTLOADER_TEST_A | BOOT_BIT_BOOTLOADER_TEST_B);
  boot_bit_set(BOOT_BIT_BOOTLOADER_TEST_A);

  bool as_fw = true;
  if (strcmp(dest_type, "prf") == 0) {
    as_fw = false;
  }
  if (as_fw) {
    BLTEST_LOG("STAGE 1 -- Copying STAGE 2 to scratch");
    prv_bootloader_test_copy(FLASH_REGION_FIRMWARE_DEST_BEGIN,
                             FLASH_REGION_FIRMWARE_DEST_END);

    BLTEST_LOG("STAGE 1 -- Marking new FW boot bit");
    boot_bit_set(BOOT_BIT_NEW_FW_AVAILABLE);
  } else {
    BLTEST_LOG("STAGE 1 -- Copying STAGE 2 to PRF");
    flash_prf_set_protection(false);
    prv_bootloader_test_copy(FLASH_REGION_SAFE_FIRMWARE_BEGIN, FLASH_REGION_SAFE_FIRMWARE_END);

    BLTEST_LOG("STAGE 1 -- Marking PRF boot bit");
    boot_bit_set(BOOT_BIT_FORCE_PRF);
  }

  BLTEST_LOG("STAGE 1 -- Rebooting");
  RebootReason reason = { RebootReasonCode_PrfReset, 0 };
  reboot_reason_set(&reason);
  system_hard_reset();
}
#else
void command_bootloader_test(void) {
  prompt_send_response("Not configured for bootloader test");
}
#endif
