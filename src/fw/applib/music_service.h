/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @file music_service.h
 * @brief Music Service for controlling mobile device volume.
 */

/**
 * @addtogroup Foundation
 * @{
 *   @addtogroup MusicService
 *   @{
 */

/**
 * Sends a "Volume Up" command to the mobile device.
 * This command is sent to the currently active media player on the phone.
 */
void music_volume_up(void);

/**
 * Sends a "Volume Down" command to the mobile device.
 * This command is sent to the currently active media player on the phone.
 */
void music_volume_down(void);

/**
 * Gets the current volume level of the mobile device's media player.
 * @return Volume as a percentage from 0 to 100, or 255 if not available.
 */
uint8_t music_get_volume_percent(void);

/**
 *   @} // end addtogroup MusicService
 * @} // end addtogroup Foundation
 */
