/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <kernel/events.h>

/**
 * @defgroup services_firmware_update Firmware update
 * @ingroup services
 * @brief Tracks a firmware update pushed by the phone.
 *
 * A firmware update start system message switches to the firmware update runlevel, launches the
 * progress UI and waits for the phone to send the firmware over put_bytes. The update is refused
 * when the battery is critically low. Progress is tracked from put_bytes events; on failure the
 * normal runlevel is restored, on success the watch reboots into the new firmware.
 * @{
 */

/** @brief Initialize the firmware update service. */
void firmware_update_init(void);

/**
 * @brief Get the progress of the running update.
 *
 * @return Percentage of the update transferred, 0 if no update is running.
 */
unsigned int firmware_update_get_percent_progress(void);

/**
 * @brief Handle a firmware update start, failure or completion system message.
 *
 * Replies to the start message with the resulting @ref FirmwareUpdateStatus.
 *
 * @param event System message event; other message types are ignored.
 */
void firmware_update_event_handler(PebbleSystemMessageEvent *event);

/**
 * @brief Handle a put_bytes event during an update.
 *
 * Ignored if no update is running.
 *
 * @param event Put bytes event.
 */
void firmware_update_pb_event_handler(PebblePutBytesEvent *event);

/** @brief State of the firmware update, also sent to the phone in the start response. */
typedef enum {
  /** No update running, or the last one completed. */
  FirmwareUpdateStopped = 0,
  /** An update is running. */
  FirmwareUpdateRunning = 1,
  /** The update was refused because the battery is critically low. */
  FirmwareUpdateCancelled = 2,
  /** The last update failed. */
  FirmwareUpdateFailed = 3,
} FirmwareUpdateStatus;

/**
 * @brief Get the state of the firmware update.
 *
 * @return Current state.
 */
FirmwareUpdateStatus firmware_update_current_status(void);

/**
 * @brief Check whether a firmware update is running.
 *
 * @return true if an update is running.
 */
bool firmware_update_is_in_progress(void);

/** @} */
