/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/mcu.h>

static const uint8_t s_serial[] = {'Q', 'E', 'M', 'U', '0', '0', '0', '0'};

int mcu_get_serial(void *buf, size_t *buf_sz) {
  if (*buf_sz < sizeof(s_serial)) {
    return -ENOSPC;
  }

  memcpy(buf, s_serial, sizeof(s_serial));
  *buf_sz = sizeof(s_serial);

  return 0;
}
