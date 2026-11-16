/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sensor.h"

#include <errno.h>

#include <pbl/logging/logging.h>

/* Another file of the same module: it shares its level. */
PBL_LOG_MODULE_DECLARE(sample, CONFIG_SAMPLE_LOG_LEVEL);

int sensor_read(uint8_t *buf, size_t len) {
  if (len > 16U) {
    PBL_LOG_WRN("read of %zu bytes, at most 16", len);
    return -EINVAL;
  }

  for (size_t i = 0; i < len; i++) {
    buf[i] = (uint8_t)(0xa0U + i);
  }
  PBL_LOG_DBG("read %zu bytes", len);

  return 0;
}
