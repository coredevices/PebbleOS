/*
 * Copyright 2025 Core Devices LLC
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

#pragma once

//! nPM1300 configuration
typedef struct {
  //! Charge current (32-800mA, 2mA steps)
  uint16_t chg_current_ma;
  //! Discharge limit (200mA or 1000mA)
  uint16_t dischg_limit_ma;
  //! Termination current (% of charge current, 10 or 20%)
  uint8_t term_current_pct;
  //! Thermistor beta value
  uint16_t thermistor_beta;
} Npm1300Config;