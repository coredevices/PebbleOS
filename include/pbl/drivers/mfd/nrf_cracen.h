/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_mfd_nrf_cracen Nordic nRF54L CRACEN
 * @ingroup drivers
 * @brief Core of the CRACEN crypto accelerator.
 *
 * The core is a struct pbl_nrf_cracen device. It turns the parts of the accelerator on and off
 * for its users: the random number generator (@ref drivers_rng) and the cipher and hash engine
 * (@ref drivers_crypto).
 * @{
 */

/** @brief CRACEN. */
struct pbl_nrf_cracen {
  /** Device. */
  struct pbl_device dev;
  /** Wrapper registers. */
  uintptr_t wrapper;
  /** Engine registers. */
  uintptr_t core;
};

/** @brief The CRACEN of the SoC. */
extern const struct pbl_nrf_cracen pbl_nrf_cracen_inst;

/**
 * @brief Turn parts of the accelerator on.
 *
 * @param cracen CRACEN.
 * @param parts CRACEN_ENABLE_* bits.
 */
void pbl_nrf_cracen_enable(const struct pbl_nrf_cracen *cracen, uint32_t parts);

/**
 * @brief Turn parts of the accelerator off.
 *
 * @param cracen CRACEN.
 * @param parts CRACEN_ENABLE_* bits.
 */
void pbl_nrf_cracen_disable(const struct pbl_nrf_cracen *cracen, uint32_t parts);

/** @cond INTERNAL_HIDDEN */
int pbl_nrf_cracen_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the CRACEN.
 *
 * @param sym Symbol of the device.
 * @param _name Name.
 * @param _wrapper Wrapper registers.
 * @param _core Engine registers.
 */
#define PBL_NRF_CRACEN_DEFINE(sym, _name, _wrapper, _core)                                   \
  PBL_DEVICE_STATE_DEFINE(sym);                                                              \
  PBL_DEVICE_INSTANCE_DEV(sym, struct pbl_nrf_cracen) {                                      \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_nrf_cracen_init, nullptr), .wrapper = (_wrapper), \
    .core = (_core),                                                                         \
  }

/** @} */
