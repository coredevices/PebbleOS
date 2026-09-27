/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#define NVDS_BUFF_START 0x2040FE00

static const uint8_t s_ble_slp_default[] = {
// Control pre-wakeup time for the sleep of BT subsytem in LCPU.
// See SiFli-SDK EXT_WAKEUP_TIME_LXT32K/EXT_WAKEUP_TIME_RC10K for details.
// RC10K -> 4500us (0x1194)
// LXT32K -> 3500us (0x0DAC)
#ifdef LXT_DISABLE
  0x0D,
  0x02,
  0x19,
  0x11,
#else
  0x0D,
  0x02,
  0xAC,
  0x0D,
#endif
  // Control maximum sleep duration of BT subsystem.
  // The last 0x01 means 10s in BLE only and 30s in dual mode. 0 means 500ms
  0x12,
  0x01,
  0x01,
  // Control the log in controller
  // Changed to 0x20, 0x00, 0x09, 0x00 will enable HCI logs by default
  0x2F,
  0x04,
  0x20,
  0x00,
  0x00,
  0x00,
  // Internal usage, for scheduling
  0x15,
  0x01,
  0x01,
};

void lcpu_custom_nvds_config(const uint8_t *bd_addr) {
  uint8_t *nvds_addr = (uint8_t *)NVDS_BUFF_START;
  uint8_t *p = nvds_addr + 8;

  if (bd_addr != NULL) {
    *p++ = 0x01;
    *p++ = 0x06;
    memcpy(p, bd_addr, 6);
    p += 6;
  }

  memcpy(p, s_ble_slp_default, sizeof(s_ble_slp_default));
  p += sizeof(s_ble_slp_default);

  *(uint32_t *)nvds_addr = 0x4E564453;
  *(uint16_t *)(nvds_addr + 4) = p - (nvds_addr + 8);
  *(uint16_t *)(nvds_addr + 6) = 0;
}
