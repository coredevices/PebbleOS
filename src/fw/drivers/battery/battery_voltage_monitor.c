/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/battery.h"

#include "board/board.h"
#include "drivers/voltage_monitor.h"

ADCVoltageMonitorReading battery_read_voltage_monitor(void) {
  VoltageReading info;
  voltage_monitor_read(VOLTAGE_MONITOR_BATTERY, &info);
  return (ADCVoltageMonitorReading) {
    .vref_total = info.vref_total,
    .vmon_total = info.vmon_total,
  };
}