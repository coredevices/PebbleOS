/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg/mfg_info.h"

#include "console/dbgserial.h"

#include "services/common/legacy/factory_registry.h"
#include "util/net.h"

#define COLOR_KEY "mfg_color"
#define RTC_FREQ_KEY "mfg_rtcfreq"

static uint32_t prv_get_uint32(const char* key) {
  Record *rec = factory_registry_get(key, strlen(key), REGISTRY_SYSTEM_UUID);
  if (rec && (rec->value_length == sizeof(uint32_t))) {
    uint32_t *value = (uint32_t*)&rec->value;
    return ntohl(*value);
  } else {
    return 0;
  }
}

static void prv_set_uint32(const char* key, uint32_t value) {
  // We store everything in the factory registry in network order, I'm not sure why.
  value = htonl(value);

  int error = factory_registry_add(key, strlen(key), REGISTRY_SYSTEM_UUID, 0,
                                   (uint8_t*)&value, sizeof(value));

  if (error) {
    dbgserial_putstr("Error");
    return;
  }

  factory_registry_write_to_flash();
}

WatchInfoColor mfg_info_get_watch_color(void) {
  return prv_get_uint32(COLOR_KEY);
}

void mfg_info_set_watch_color(WatchInfoColor color) {
  prv_set_uint32(COLOR_KEY, color);
}

uint32_t mfg_info_get_rtc_freq(void) {
  return prv_get_uint32(RTC_FREQ_KEY);
}

void mfg_info_set_rtc_freq(uint32_t rtc_freq) {
  prv_set_uint32(RTC_FREQ_KEY, rtc_freq);
}

void mfg_info_get_model(char* buffer) {
  // Not implemented, tintin/bianca's won't have this programmed.
  // FIXME: We could approximate this based on the watch color value.
}

void mfg_info_set_model(const char* model) {
  // Not implemented, we won't be using this firmware for manufacturing tintin/biancas.
}

GPoint mfg_info_get_disp_offsets(void) {
  // Not implemented. Can just assume no offset
  return (GPoint) {};
}

void mfg_info_set_disp_offsets(GPoint p) {
  // Not implemented.
}

void mfg_info_update_constant_data(void) {
  // No constant data required for tintin/bianca.
}
