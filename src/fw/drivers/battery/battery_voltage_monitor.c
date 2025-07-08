/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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