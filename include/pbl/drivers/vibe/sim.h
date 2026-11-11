/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/vibe.h>

/**
 * @defgroup drivers_vibe_sim Simulator
 * @ingroup drivers_vibe
 * @brief Simulated vibration motor, reporting its strength to an observer.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_vibe pbl_vibe_sim;
/** @endcond */

/** @brief The simulated vibration motor. */
#define SIM_VIBE (&pbl_vibe_sim)

/**
 * @brief Strength change observer.
 *
 * Called with the device lock held.
 *
 * @param vibe Device.
 * @param strength New strength, 0 when stopped.
 * @param arg Argument passed to pbl_vibe_sim_set_observer().
 */
typedef void (*pbl_vibe_sim_observer_t)(const struct pbl_vibe *vibe, int8_t strength, void *arg);

/**
 * @brief Set the observer called whenever the motor strength changes.
 *
 * @param vibe Device.
 * @param observer Observer, or NULL for none.
 * @param arg Argument passed to @p observer.
 */
void pbl_vibe_sim_set_observer(const struct pbl_vibe *vibe, pbl_vibe_sim_observer_t observer,
                               void *arg);

/**
 * @brief Get the motor strength.
 *
 * @param vibe Device.
 * @return Strength, 0 when stopped.
 */
int8_t pbl_vibe_sim_get_strength(const struct pbl_vibe *vibe);

/** @} */
