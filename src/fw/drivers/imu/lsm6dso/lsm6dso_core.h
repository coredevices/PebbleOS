/* SPDX-FileCopyrightText: 2026 Dave Bortz */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lsm6dso_reg.h"

/*! LSM6DSO shared core
 *  ===================
 *
 * The LSM6DSO is a combined accelerometer + gyroscope. The accelerometer half
 * is exposed through drivers/accel.h (lsm6dso.c) and the gyroscope half
 * through drivers/gyro.h (lsm6dso_gyro.c). Either half may be compiled in
 * independently (CONFIG_ACCEL_LSM6DSO / CONFIG_GYRO_LSM6DSO); this core owns
 * everything the two halves share: the I2C register interface, one-time chip
 * initialization, and the single hardware FIFO.
 */

//! Shared ST HAL context. Defined in lsm6dso_core.c.
extern stmdev_ctx_t lsm6dso_ctx;

//! I2C/sensor health tracking shared between the two halves.
typedef struct {
  uint32_t i2c_error_count;
  uint32_t consecutive_errors;
  uint32_t last_successful_read_ms;
  bool sensor_health_ok;
} Lsm6dsoCoreHealth;

Lsm6dsoCoreHealth *lsm6dso_core_health(void);

//! One-time chip initialization: WHO_AM_I check, software reset, I3C disable,
//! block data update, register auto-increment, default full scales, both ODRs
//! off, pulsed interrupt mode. Idempotent; safe to call from both halves in
//! any order.
//! @return true if the chip is initialized
bool lsm6dso_core_init(void);

//! Force re-initialization (used for watchdog recovery). Both halves must
//! re-apply their own configuration afterwards.
bool lsm6dso_core_reinit(void);

bool lsm6dso_core_is_initialized(void);

//! Current time in ms since the epoch (RTC based).
uint64_t lsm6dso_core_timestamp_ms(void);

// Shared FIFO management
// ----------------------
// The chip has a single FIFO. Each half registers its batching needs and the
// core programs the combined watermark and per-sensor batch rates. Records
// are dispatched by FIFO tag when draining.

//! Update the accelerometer's batching request. num_samples <= 1 or
//! interval_us == 0 disables accel batching.
void lsm6dso_core_fifo_request_xl(uint32_t num_samples, uint32_t interval_us);

//! Update the gyroscope's batching request. num_samples <= 1 or
//! interval_us == 0 disables gyro batching.
void lsm6dso_core_fifo_request_gy(uint32_t num_samples, uint32_t interval_us);

//! True when the FIFO is currently in use (either half is batching).
bool lsm6dso_core_fifo_in_use(void);

//! Drain the FIFO, dispatching each record to the half it belongs to. Must
//! be called from a thread context with the owning service lock held (see
//! lsm6dso_core_offload_work).
void lsm6dso_core_fifo_drain(void);

//! FIFO overflow recovery: clear the FIFO and reprogram it with a reduced
//! watermark to prevent repeated overflows.
void lsm6dso_core_fifo_recover(void);

//! Defer work to a thread context with appropriate locking. Routes through
//! the accel manager's offload mechanism when the accel half is compiled in
//! (so FIFO drains shared with accel data are serialized with other accel
//! work), and through the gyro manager's otherwise.
typedef void (*Lsm6dsoCoreWorkCallback)(void);
void lsm6dso_core_offload_work(Lsm6dsoCoreWorkCallback cb);

// Per-half hooks, implemented by the respective driver file. Called by the
// core FIFO drain with the raw 6-byte sample payload and an approximate
// sample timestamp.
#if defined(CONFIG_ACCEL_LSM6DSO)
void lsm6dso_accel_handle_fifo_record(const uint8_t data[6], uint64_t timestamp_us);
//! Re-evaluate the accelerometer power mode. Called by the gyro half when the
//! gyro turns on or off: the accel must not use ultra-low-power mode while
//! the gyro is active (datasheet section 6.2.1).
void lsm6dso_accel_gyro_state_changed(void);
#endif
#if defined(CONFIG_GYRO_LSM6DSO)
void lsm6dso_gyro_handle_fifo_record(const uint8_t data[6], uint64_t timestamp_us);
//! True when the gyroscope is currently powered on.
bool lsm6dso_gyro_is_active(void);
//! Called after a forced core re-initialization (chip reset) so the gyro half
//! can re-apply its configuration.
void lsm6dso_gyro_handle_core_reinit(void);
#endif
