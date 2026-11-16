/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sensor.h"

#include <stdint.h>
#include <stdio.h>

#include <pbl/logging/logging.h>

/* The sample's Kconfig defines CONFIG_SAMPLE_LOG_LEVEL from the log level template. */
PBL_LOG_MODULE_DEFINE(sample, CONFIG_SAMPLE_LOG_LEVEL);

static void prv_log_each_level(void) {
  PBL_LOG_ERR("an error");
  PBL_LOG_WRN("a warning");
  PBL_LOG_INFO("an event worth attention");
  PBL_LOG_DBG("a routine state change");
  PBL_LOG_VERBOSE("a detail");
}

int main(void) {
  uint8_t data[16];

  /* The module starts at its configured level, debug: verbose messages are dropped. */
  prv_log_each_level();

  if (sensor_read(data, sizeof(data)) == 0) {
    PBL_LOG_HEXDUMP_INFO(data, sizeof(data));
  }
  (void)sensor_read(data, 32U);

  /* Built with CONFIG_SAMPLE_LOG_LEVEL_RUNTIME, the level can change while running. */
  PBL_LOG_MODULE_LEVEL_SET(sample, LOG_LEVEL_WARNING);
  PBL_LOG_WRN("level now %d, warning", PBL_LOG_MODULE_LEVEL_GET(sample));
  prv_log_each_level();

  printf("Sample done\n");

  return 0;
}
