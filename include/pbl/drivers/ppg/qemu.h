/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/ppg.h>

/**
 * @defgroup drivers_ppg_qemu QEMU PPG front end
 * @ingroup drivers_ppg
 * @brief Synthetic @ref drivers_ppg driver for QEMU, which emulates no optical sensor.
 *
 * Slots light a green, a red and an infrared LED, plus an ambient slot, on one receiver. The
 * driver models the light coming back from a wrist with a pulse set by the host and produces no
 * samples until one is set.
 * @{
 */

/**
 * @brief Set the simulated pulse.
 *
 * @param bpm Heart rate, in beats per minute.
 * @param perfusion_permille Pulsatile part of the green signal, in permille; 0 for no wrist under
 *                           the sensor.
 */
void qemu_ppg_set_pulse(uint16_t bpm, uint16_t perfusion_permille);

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_ppg pbl_ppg_qemu;
/** @endcond */

/** @brief The QEMU PPG front end. */
#define QEMU_PPG (&pbl_ppg_qemu)

/** @} */
