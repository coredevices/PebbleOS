/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup mpsl_pebble MPSL
 * @brief PebbleOS glue of the Nordic multiprotocol service layer, from the nonfree libraries, on
 * the nRF54L.
 *
 * The MPSL schedules the radio protocols and owns the RADIO, TIMER10, the GRTC channels 7 to 11
 * with the GRTC_3 interrupt, and the CLOCK. Its radio, timer and GRTC interrupts are
 * zero-latency, so no lock delays them.
 *
 * The clock driver starts it before anything uses a clock, and from then on takes the crystals
 * through it. Its low-priority processing runs on a thread of its own, under mpsl_pebble_lock(),
 * which the protocol stacks also take around their own calls into it.
 * @{
 */

/**
 * @brief Start the MPSL.
 *
 * Called once, by the clock driver.
 *
 * @return 0 or a negative errno.
 */
int mpsl_pebble_init(void);

/**
 * @brief Request or release the HFXO.
 *
 * A request waits until the crystal runs.
 *
 * @param on Request if true, release otherwise.
 * @return 0 or a negative errno.
 */
int mpsl_pebble_hfxo(bool on);

/** @brief Take the lock around calls into the MPSL and the stacks on it. */
void mpsl_pebble_lock(void);

/** @brief Release the lock taken with mpsl_pebble_lock(). */
void mpsl_pebble_unlock(void);

/** @} */
