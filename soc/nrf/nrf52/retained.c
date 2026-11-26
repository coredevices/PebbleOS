/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <pbl/crc/crc.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/section.h>
#include <pbl/logging/logging.h>
#include <pbl/system/rtc_registers.h>
#include <pbl/util/size.h>

static uint32_t PBL_SECTION(".retained") s_retained[CONFIG_SOC_NRF52_RETAINED_RAM_SIZE / 4];
static_assert(NRF_RETAINED_REGISTER_CRC < PBL_ARRAY_SIZE(s_retained));

static uint32_t prv_crc(void) {
  return pbl_crc32(0, s_retained, NRF_RETAINED_REGISTER_CRC * 4);
}

void retained_write(uint8_t id, uint32_t value) {
  s_retained[id] = value;
  s_retained[NRF_RETAINED_REGISTER_CRC] = prv_crc();
}

void retained_write_many(uint8_t id, const uint32_t *values, size_t count) {
  PBL_ASSERTN((size_t)id + count <= NRF_RETAINED_REGISTER_CRC);

  for (size_t i = 0U; i < count; i++) {
    s_retained[id + i] = values[i];
  }
  s_retained[NRF_RETAINED_REGISTER_CRC] = prv_crc();
}

uint32_t retained_read(uint8_t id) {
  return s_retained[id];
}

bool retained_validate(void) {
  uint32_t crc = prv_crc();

  if (crc == s_retained[NRF_RETAINED_REGISTER_CRC]) {
    return true;
  }

  PBL_LOG_WRN("Retained register CRC failed: expected %08" PRIx32 ", got %08" PRIx32 ", clearing",
              crc, s_retained[NRF_RETAINED_REGISTER_CRC]);
  memset(s_retained, 0, sizeof(s_retained));

  return false;
}
