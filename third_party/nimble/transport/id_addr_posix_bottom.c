/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "id_addr_posix_bottom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <posix_host.h>

static uint8_t s_octets[6];
static bool s_given;

bool id_addr_posix_bottom_get(uint8_t octets[6]) {
  if (!s_given) {
    return false;
  }

  memcpy(octets, s_octets, sizeof(s_octets));

  return true;
}

static void prv_set_addr(const char *value) {
  unsigned int o[6];
  char end;

  if (sscanf(value, "%2x:%2x:%2x:%2x:%2x:%2x%c", &o[5], &o[4], &o[3], &o[2], &o[1], &o[0], &end) !=
      6) {
    fprintf(stderr, "invalid Bluetooth address: %s\n", value);
    exit(1);
  }

  for (size_t i = 0; i < sizeof(s_octets); i++) {
    s_octets[i] = (uint8_t)o[i];
  }
  s_given = true;
}

POSIX_HOST_OPTION(.flag = 'a', .arg = "address",
                  .help = "use this random static Bluetooth address (XX:XX:XX:XX:XX:XX)",
                  .set = prv_set_addr)
