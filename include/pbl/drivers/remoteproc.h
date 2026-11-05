/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_remoteproc Remote processor
 * @ingroup drivers
 * @brief Remote processor class.
 *
 * A remote processor runs its own firmware, which the provider boots and halts. Clients reach the
 * firmware through resources the provider describes, such as mailbox channels and shared memory,
 * and hold the processor running between pbl_rproc_boot() and pbl_rproc_shutdown():
 *
 * @code{.c}
 * ret = pbl_rproc_boot(rproc);
 * // talk to the firmware
 * pbl_rproc_shutdown(rproc);
 * @endcode
 *
 * Boots are counted: the first one starts the processor and the last shutdown stops it. Both may
 * wait, so call them from thread context.
 * @{
 */

struct pbl_rproc;

/** @brief Class of the remote processors. */
extern const struct pbl_device_class pbl_rproc_class;

/** @brief Remote processor operations, implemented by the providers, called with the lock held. */
struct pbl_rproc_ops {
  /** Start the processor and its firmware. Returns 0 or a negative errno. */
  int (*start)(const struct pbl_rproc *rproc);
  /** Stop the processor. */
  void (*stop)(const struct pbl_rproc *rproc);
};

/** @brief Remote processor state, kept by the class. */
struct pbl_rproc_state {
  /** Serializes boot and shutdown. */
  struct pbl_mutex lock;
  /** Number of boots not yet shut down. */
  uint8_t power;
};

/** @brief Remote processor. */
struct pbl_rproc {
  /** Device. */
  struct pbl_device dev;
  /** Operations. */
  const struct pbl_rproc_ops *ops;
  /** State. */
  struct pbl_rproc_state *state;
};

/**
 * @brief Define the state of a remote processor, for providers.
 *
 * @param sym Remote processor symbol.
 */
#define PBL_RPROC_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);     \
  static struct pbl_rproc_state sym##_rproc_state = {.lock = PBL_MUTEX_INITIALIZER}

/**
 * @brief Initializer of a struct pbl_rproc, for providers.
 *
 * @param sym Remote processor symbol, with its PBL_RPROC_STATE_DEFINE().
 * @param _name Device name.
 * @param _init Device init function.
 * @param _deps Device dependencies.
 * @param _ops Operations.
 */
#define PBL_RPROC_INIT(sym, _name, _init, _deps, _ops)                                 \
  {                                                                                    \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, nullptr, _deps, &pbl_rproc_class), \
    .ops = (_ops),                                                                     \
    .state = &sym##_rproc_state,                                                       \
  }

/**
 * @brief Boot a remote processor, or count one more user if it runs.
 *
 * @param rproc Remote processor.
 * @return 0 on success, -ENODEV if it is not ready, or a negative errno from the provider.
 */
int pbl_rproc_boot(const struct pbl_rproc *rproc);

/**
 * @brief Release a boot, stopping the remote processor if it was the last one.
 *
 * @param rproc Remote processor, booted.
 */
void pbl_rproc_shutdown(const struct pbl_rproc *rproc);

/**
 * @brief Whether a remote processor runs.
 *
 * @param rproc Remote processor.
 */
bool pbl_rproc_is_running(const struct pbl_rproc *rproc);

/** @} */
