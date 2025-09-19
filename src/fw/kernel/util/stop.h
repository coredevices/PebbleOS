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

#pragma once

#include <stdbool.h>

typedef enum {
  InhibitorMain,
  InhibitorDbgSerial,
  InhibitorButton,
  InhibitorBluetooth,
  InhibitorDisplay,
  InhibitorBacklight,
  InhibitorCommMode,
  InhibitorFlash,
  InhibitorI2C1,
  InhibitorI2C2,
  InhibitorMic,
  InhibitorAccessory,
  InhibitorVibes,
  InhibitorCompositor,
  InhibitorI2C3,
  InhibitorI2C4,
  InhibitorBluetoothWatchdog,
  InhibitorPWM,

  InhibitorNumItems
} StopModeInhibitor;

/** Enter stop mode.
 *
 *  \note Probably no good reason to call this function from most application 
 *  code. Let the FreeRTOS scheduler do its job.
 */
void enter_stop_mode(void);

/** Prevent the scheduler from entering stop mode in idle.  Usually called when
 * we know that there is some resource or peripheral being used that does not 
 * require the use of the CPU, but that going into stop mode would interrupt.
 * \note Internally this is implemented as a reference counter, so it is
 * necessary to balance each call to disallow_stop_mode with a matching call to
 * allow_stop_mode.
 *  CAUTION: This function cannot be called at priorities > Systick
 */
void stop_mode_disable(StopModeInhibitor inhibitor);

/** Allow the scheduler to enter stop mode in idle again.
 *  CAUTION: This function cannot be called at priorities > Systick
 */
void stop_mode_enable(StopModeInhibitor inhibitor);

//! Check whether we are permitted to go into stop mode
bool stop_mode_is_allowed(void);


//! Enable or disable sleep mode.
//! Note: When sleep mode is disabled so is stop mode. When sleep mode is enabled, stop mode is
//! controlled by stop_mode_is_allowed.
void sleep_mode_enable(bool enable);

//! Check whether we are permitted to go into sleep mode.
bool sleep_mode_is_allowed(void);

