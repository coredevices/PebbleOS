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

#include "drivers/rtc.h"
#include "os/mutex.h"
#include "kernel/util/stop.h"

#include "freertos_types.h"
#include "portmacro.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum I2CTransferEvent {
  I2CTransferEvent_Timeout,
  I2CTransferEvent_TransferComplete,
  I2CTransferEvent_NackReceived,
  I2CTransferEvent_Error,
} I2CTransferEvent;

typedef enum {
  I2CTransferDirection_Read,
  I2CTransferDirection_Write
} I2CTransferDirection;

typedef enum {
  // Send a register address, followed by a repeat start for reads
  I2CTransferType_SendRegisterAddress,

  // Do not send a register address; used for block writes/reads
  I2CTransferType_NoRegisterAddress
} I2CTransferType;

typedef enum I2CTransferState {
  I2CTransferState_WriteAddressTx,
  I2CTransferState_WriteRegAddress,
  I2CTransferState_RepeatStart,
  I2CTransferState_WriteAddressRx,
  I2CTransferState_WaitForData,
  I2CTransferState_ReadData,
  I2CTransferState_WriteData,
  I2CTransferState_EndWrite,
  I2CTransferState_Complete,
} I2CTransferState;

typedef struct I2CTransfer {
  I2CTransferState state;
  uint16_t device_address;
  I2CTransferDirection direction;
  I2CTransferType type;
  uint8_t register_address;
  uint32_t size;
  uint32_t idx;
  uint8_t *data;
} I2CTransfer;

typedef struct I2CBusState {
  I2CTransfer transfer;
  I2CTransferEvent transfer_event;
  int transfer_nack_count;
  RtcTicks transfer_start_ticks;
  int user_count;
#if MICRO_FAMILY_STM32F2 || MICRO_FAMILY_STM32F4 || MICRO_FAMILY_STM32F7
  RtcTicks last_rail_stop_ticks;
#endif
  SemaphoreHandle_t event_semaphore;
  PebbleMutex *bus_mutex;
} I2CBusState;

struct I2CBus {
  I2CBusState *const state;
  const struct I2CBusHal *const hal;
#if MICRO_FAMILY_STM32F2 || MICRO_FAMILY_STM32F4 || MICRO_FAMILY_STM32F7 || MICRO_FAMILY_NRF5
  AfConfig scl_gpio;  ///< Alternate Function configuration for SCL pin
  AfConfig sda_gpio;  ///< Alternate Function configuration for SDA pin
#endif
#if MICRO_FAMILY_STM32F2 || MICRO_FAMILY_STM32F4 || MICRO_FAMILY_STM32F7
  OutputConfig rail_gpio;  ///< Control pin for rail
  void (* const rail_ctl_fn)(I2CBus *device, bool enabled); ///< Control function for this rail.
#endif
  StopModeInhibitor stop_mode_inhibitor;
  const char *name;  //! Device ID for logging purposes
};

struct I2CSlavePort {
  const I2CBus *bus;
  uint16_t address;
};

//! Initialize the I2C driver.
void i2c_init(I2CBus *bus);

//! Transfer event handler implemented in i2c.c and called by HAL implementation
portBASE_TYPE i2c_handle_transfer_event(I2CBus *device, I2CTransferEvent event);

#define I2C_DEBUG(fmt, args...) \
  PBL_LOG_COLOR_D(LOG_DOMAIN_I2C, LOG_LEVEL_DEBUG, LOG_COLOR_LIGHT_MAGENTA, fmt, ## args)
