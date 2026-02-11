/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

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

