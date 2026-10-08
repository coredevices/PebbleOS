/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/sem.h>
#include <pbl/drivers/rtc.h>
#include <pbl/kernel/mutex.h>

#include <stdint.h>

/**
 * @defgroup drivers_i2c_definitions I2C bus definitions
 * @ingroup drivers_i2c
 * @brief Bus and device definitions shared by boards, the common I2C code and the bus HALs.
 *
 * The common code runs one transfer at a time per bus: it fills @ref I2CBusState::transfer,
 * starts it through the @ref drivers_i2c_hal and waits for the HAL to report the outcome with
 * i2c_handle_transfer_event().
 * @{
 */

/** @brief Transfer outcome reported by a HAL. */
typedef enum I2CTransferEvent {
  /** Transfer timed out. */
  I2CTransferEvent_Timeout,
  /** Transfer completed. */
  I2CTransferEvent_TransferComplete,
  /** Device did not acknowledge; the transfer is retried. */
  I2CTransferEvent_NackReceived,
  /** Transfer failed. */
  I2CTransferEvent_Error,
} I2CTransferEvent;

/** @brief Transfer direction. */
typedef enum {
  /** Read from the device. */
  I2CTransferDirection_Read,
  /** Write to the device. */
  I2CTransferDirection_Write
} I2CTransferDirection;

/** @brief Transfer type. */
typedef enum {
  /** Send a register address first, followed by a repeated start for reads. */
  I2CTransferType_SendRegisterAddress,

  /** Do not send a register address; used for block reads and writes. */
  I2CTransferType_NoRegisterAddress
} I2CTransferType;

/** @brief Transfer state, for HALs that drive the transfer byte by byte. */
typedef enum I2CTransferState {
  /** Send the device address for writing. */
  I2CTransferState_WriteAddressTx,
  /** Send the register address. */
  I2CTransferState_WriteRegAddress,
  /** Send a repeated start. */
  I2CTransferState_RepeatStart,
  /** Send the device address for reading. */
  I2CTransferState_WriteAddressRx,
  /** Wait for data. */
  I2CTransferState_WaitForData,
  /** Read data. */
  I2CTransferState_ReadData,
  /** Write data. */
  I2CTransferState_WriteData,
  /** Finish the write. */
  I2CTransferState_EndWrite,
  /** Transfer complete. */
  I2CTransferState_Complete,
} I2CTransferState;

/** @brief Transfer in progress on a bus. */
typedef struct I2CTransfer {
  /** Transfer state, for HALs that use it. */
  I2CTransferState state;
  /** Device address, as in I2CSlavePort::address. */
  uint16_t device_address;
  /** Transfer direction. */
  I2CTransferDirection direction;
  /** Transfer type. */
  I2CTransferType type;
  /** Register address, for @ref I2CTransferType_SendRegisterAddress. */
  uint8_t register_address;
  /** Number of data bytes. */
  uint32_t size;
  /** Index of the next data byte, for HALs that use it. */
  uint32_t idx;
  /** Data to write or buffer to read into. */
  uint8_t *data;
  /** @ref data follows the i2c_read_register_block_dma() rules, so the HAL may use DMA. */
  bool dma;
} I2CTransfer;

/** @brief Bus runtime state, owned by the common I2C code. */
typedef struct I2CBusState {
  /** Current transfer. */
  I2CTransfer transfer;
  /** Outcome of the current transfer, set by i2c_handle_transfer_event(). */
  I2CTransferEvent transfer_event;
  /** NACKs received during the current transfer. */
  int transfer_nack_count;
  /** Start time of the current transfer. */
  RtcTicks transfer_start_ticks;
  /** Number of i2c_use() users. */
  int user_count;
  /** Signaled when the current transfer ends. */
  struct pbl_sem event_semaphore;
  /** Serializes bus access. */
  struct pbl_mutex bus_mutex;
} I2CBusState;

/** @brief I2C bus, defined by the board. */
struct I2CBus {
  /** Runtime state. */
  I2CBusState *const state;
  /** HAL-specific configuration. */
  const struct I2CBusHal *const hal;
#ifdef CONFIG_SOC_NRF52
  /** SCL pin. */
  AfConfig scl_gpio;
  /** SDA pin. */
  AfConfig sda_gpio;
#endif
  /** Bus name, for logging. */
  const char *name;
};

/** @brief Device on an I2C bus, defined by the board. */
struct I2CSlavePort {
  /** Bus the device is connected to. */
  const I2CBus *bus;
  /**
   * Device address: 8-bit (7-bit address shifted left by one) on nRF5, 7-bit on SF32LB.
   */
  uint16_t address;
};

/**
 * @brief Initialize a bus.
 *
 * Called once at boot for every bus, before any i2c_use().
 *
 * @param bus Bus to initialize.
 */
void i2c_init(I2CBus *bus);

/**
 * @brief Report the outcome of the current transfer.
 *
 * Called by the HAL, typically from its interrupt handler.
 *
 * @param device Bus the transfer ran on.
 * @param event Transfer outcome.
 */
void i2c_handle_transfer_event(I2CBus *device, I2CTransferEvent event);

/** @} */
