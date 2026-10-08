/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "accel_manager_types.h"

#include <stdbool.h>
#include <stdint.h>

#include <kernel/pebble_tasks.h>

/**
 * @defgroup services_accel_manager Accelerometer manager
 * @ingroup services
 * @brief Shares the accelerometer between subscribers.
 *
 * Samples from the accelerometer driver are queued in a shared buffer and copied, subsampled to
 * each subscriber's rate, into a buffer the subscriber provides. Once a subscriber's buffer holds
 * the requested number of samples, its callback runs on the task chosen at subscription. The
 * manager also arms shake and double tap detection while event service subscribers exist.
 *
 * The @c sys_ functions are syscalls usable from unprivileged code; subscription handles passed in
 * from userspace are validated against the subscriber list.
 * @{
 */

/**
 * @brief Called when a subscriber's sample buffer is full.
 *
 * No further callback is posted until sys_accel_manager_consume_samples() is called, so each call
 * must end with one, even of 0 samples, unless the subscription has been dropped meanwhile. It
 * handles the next batch itself while the consume reports more. The buffer can hold fewer samples
 * than requested if it was changed meanwhile.
 *
 * @param context Context given to sys_accel_manager_data_subscribe().
 */
typedef void (*AccelDataReadyCallback)(void *context);

/** @brief Opaque accelerometer subscription. */
typedef struct AccelManagerState AccelManagerState;

/**
 * @brief Get the maximum number of samples that can be batched per update.
 *
 * @return Depth of the accelerometer's hardware FIFO, in samples.
 */
uint32_t sys_accel_manager_get_max_samples_per_update(void);

/**
 * @brief Initialize the accelerometer manager.
 *
 * Registers the shake and double tap event services and applies the saved motion sensitivity.
 */
void accel_manager_init(void);

/**
 * @brief Enable or disable the accelerometer.
 *
 * While disabled, sampling and shake/double tap detection are stopped; enabling restores the
 * configuration required by the current subscribers.
 *
 * @param on true to enable, false to disable.
 */
void accel_manager_enable(bool on);

/**
 * @brief Enable or disable the kernel's shake subscription for the motion backlight.
 *
 * When disabled, shake detection is only active if apps have subscribed.
 *
 * @param enabled true to subscribe, false to unsubscribe.
 */
void accel_manager_set_motion_backlight_enabled(bool enabled);

/**
 * @brief Read the current accelerometer sample.
 *
 * @param[out] accel_data Latest sample.
 * @return 0 on success, nonzero if the driver failed to read a sample.
 */
int sys_accel_manager_peek(AccelData *accel_data);

/**
 * @brief Subscribe to accelerometer data.
 *
 * @p data_cb is called with @p context on @p handler_task whenever the buffer set with
 * sys_accel_manager_set_sample_buffer() holds the requested number of samples. Unprivileged
 * callers always get the callback on their own task (App or Worker).
 *
 * @param rate Sampling rate.
 * @param data_cb Callback invoked when data is available.
 * @param context Context passed to @p data_cb.
 * @param handler_task Task on which @p data_cb runs: App, Worker, KernelMain, KernelBackground or
 *                     NewTimers.
 * @return Subscription allocated on the kernel heap; free it with
 *         sys_accel_manager_data_unsubscribe().
 */
AccelManagerState *sys_accel_manager_data_subscribe(AccelSamplingRate rate,
                                                    AccelDataReadyCallback data_cb, void *context,
                                                    PebbleTask handler_task);

/**
 * @brief Remove a subscription and free it.
 *
 * @param state Subscription to remove.
 * @return true if a data callback was posted and hasn't been consumed yet, including one running.
 */
bool sys_accel_manager_data_unsubscribe(AccelManagerState *state);

/**
 * @brief Change the sampling rate of a subscription.
 *
 * Jitter-inducing subsampling may be used to reach the requested rate.
 *
 * @param state Subscription to reconfigure.
 * @param rate New sampling rate.
 * @retval 0 Success.
 * @retval -1 @p rate is not one of the AccelSamplingRate values.
 */
int sys_accel_manager_set_sampling_rate(AccelManagerState *state, AccelSamplingRate rate);

/**
 * @brief Use the lowest jitter-free sampling rate of at least @p min_rate_mHz.
 *
 * Only 12.5 Hz is currently supported; requesting more asserts.
 *
 * @param state Subscription to reconfigure.
 * @param min_rate_mHz Lowest acceptable sampling rate, in millihertz.
 * @return Resulting sampling rate in millihertz, 0 if no rate is high enough.
 */
uint32_t accel_manager_set_jitterfree_sampling_rate(AccelManagerState *state,
                                                    uint32_t min_rate_mHz);

/**
 * @brief Set the buffer that receives a subscription's samples.
 *
 * Empties the buffer and bumps its generation. An event already out stays out until its consume,
 * which then sees the new generation.
 *
 * @param state Subscription.
 * @param buffer Buffer of at least @p samples_per_update samples, owned by the caller. Can be NULL
 *               when @p samples_per_update is 0.
 * @param samples_per_update Samples to batch before calling the data callback, 0 to drop all
 *                           data. Must not exceed sys_accel_manager_get_max_samples_per_update().
 * @retval 0 Success.
 * @retval -1 @p samples_per_update is too large.
 */
int sys_accel_manager_set_sample_buffer(AccelManagerState *state, AccelRawData *buffer,
                                        uint32_t samples_per_update);

/**
 * @brief Get the number of samples currently in a subscription's buffer.
 *
 * @param state Subscription.
 * @param[out] timestamp_ms Timestamp of the first buffered sample, in milliseconds.
 * @param[out] generation Buffer generation, to pass to sys_accel_manager_consume_samples().
 * @return Number of buffered samples.
 */
uint32_t sys_accel_manager_get_num_samples(AccelManagerState *state, uint64_t *timestamp_ms,
                                           uint32_t *generation);

/**
 * @brief Finish handling a data callback, so the next one can be posted.
 *
 * With the current generation and @p samples above 0, the buffer is emptied and samples not
 * consumed are dropped. With @p samples 0 or an older generation, the buffer is kept.
 *
 * @param state Subscription.
 * @param samples Number of samples processed by the subscriber.
 * @param generation Generation from sys_accel_manager_get_num_samples().
 * @param[out] more true if the refilled buffer already holds the next batch. The running callback
 *                  handles it, since no new one is posted for it.
 * @return false if @p samples is from the current generation and didn't match the number of
 *         buffered samples.
 */
bool sys_accel_manager_consume_samples(AccelManagerState *state, uint32_t samples,
                                       uint32_t generation, bool *more);

/**
 * @brief Make shake detection sensitive enough to trigger on small movements.
 *
 * Used to leave low power mode as soon as a stationary watch is moved.
 *
 * @param high_sensitivity true for high sensitivity, false for the normal setting.
 */
void accel_enable_high_sensitivity(bool high_sensitivity);

/**
 * @brief Set the motion sensitivity from the user preference.
 *
 * Only has an effect on accelerometers that support adjustable sensitivity.
 *
 * @param sensitivity_percent Sensitivity from 0 (least) to 100 (most sensitive).
 */
void accel_manager_update_sensitivity(uint8_t sensitivity_percent);

/**
 * @brief Check whether the watch has been idle.
 *
 * Compares the last read sample with the position captured on the hourly analytics heartbeat,
 * without reading the hardware.
 *
 * @return true if no significant movement was seen since then.
 */
bool accel_is_idle(void);

/** @} */
