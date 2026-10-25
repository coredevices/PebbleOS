/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Stand-in for NimBLE's header, with what ctl/zbt_ctl.c uses.
#pragma once
#include <stdint.h>

#define BLE_ADDR_PUBLIC 0x00
#define BLE_ADDR_RANDOM 0x01

typedef struct {
  uint8_t type;
  uint8_t val[6];
} ble_addr_t;

#define BLE_ADDR_ANY (&(ble_addr_t){0})

struct ble_store_key_sec {
  ble_addr_t peer_addr;
  uint8_t idx;
};

struct ble_store_value_sec {
  ble_addr_t peer_addr;
  uint8_t key_size;
  uint16_t ediv;
  uint64_t rand_num;
  uint8_t ltk[16];
  uint8_t ltk_present : 1;
  uint8_t irk[16];
  uint8_t irk_present : 1;
  uint8_t csrk[16];
  uint8_t csrk_present : 1;
  unsigned authenticated : 1;
  uint8_t sc : 1;
};

int ble_store_read_our_sec(const struct ble_store_key_sec *key_sec,
                           struct ble_store_value_sec *value_sec);
int ble_store_read_peer_sec(const struct ble_store_key_sec *key_sec,
                            struct ble_store_value_sec *value_sec);
