/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/section.h>

/**
 * @defgroup coredump Core dumps
 * @ingroup subsys
 * @brief Save the state of the system when it crashes, for analysis after the reset.
 *
 * A dump holds the memory regions defined with @ref PBL_COREDUMP_REGION_DEFINE and the registers
 * of every thread. It is written to every backend, see @ref coredump_backend, and read back after
 * the reset through the @c pbl_coredump_stored_* functions.
 * @{
 */

struct pbl_coredump_backend;

/** @brief Memory region to dump. */
struct pbl_coredump_region {
  /** Start address. */
  const void *start;
  /** Size in bytes. */
  size_t size;
  /** Make the region readable, or NULL. Runs without OS services. */
  void (*prepare)(void);
  /** Dump after the threads, because reading the region could hang or fault. */
  bool late;
};

#ifdef PBL_NO_LINKER_SCRIPT
#define PBL_COREDUMP__REGION_SECTION(_name) PBL_UNSORTED_SECTION(pbl_cd_regs)
#else
#define PBL_COREDUMP__REGION_SECTION(_name) \
  PBL_ALIGNED(4) PBL_SECTION(".pbl_coredump_regions." #_name)
#endif

/**
 * @brief Define a memory region to dump.
 *
 * @param _name Region name, also its identifier.
 * @param _start Start address, a constant expression.
 * @param _size Size in bytes, a constant expression.
 */
#define PBL_COREDUMP_REGION_DEFINE(_name, _start, _size)                        \
  static const struct pbl_coredump_region _pbl_coredump_region_##_name PBL_USED \
  PBL_COREDUMP__REGION_SECTION(_name) = {.start = (_start), .size = (_size)}

/**
 * @brief Define a memory region that is dumped after the threads.
 *
 * For memory whose read could hang or fault, so the rest of the dump is saved first.
 *
 * @param _name Region name, also its identifier.
 * @param _start Start address, a constant expression.
 * @param _size Size in bytes, a constant expression.
 * @param _prepare Function that makes the region readable, or NULL.
 */
#define PBL_COREDUMP_LATE_REGION_DEFINE(_name, _start, _size, _prepare)         \
  static const struct pbl_coredump_region _pbl_coredump_region_##_name PBL_USED \
  PBL_COREDUMP__REGION_SECTION(                                                 \
      _name) = {.start = (_start), .size = (_size), .prepare = (_prepare), .late = true}

/**
 * @brief Write a core dump and reset.
 *
 * Can be called from any context. Does not return, unless entering the dump failed.
 *
 * @param forced Replace a stored dump that has not been read yet, however recent.
 */
void pbl_coredump_trigger(bool forced);

/** @brief Identification of the system that wrote a dump. */
struct pbl_coredump_metadata {
  /** Device serial number. */
  char serial_number[16];
  /** Firmware build ID. */
  char build_id[64];
};

/**
 * @brief Prepare the system for a dump, implemented by the platform.
 *
 * Runs without OS services, once the backends are initialized and before they touch what they
 * store. Drivers that a backend uses, like flash, already work.
 *
 * @param[out] metadata Identification to store in the dump.
 */
void pbl_coredump_platform_prepare(struct pbl_coredump_metadata *metadata);

/** @brief Dump stored by a backend. */
struct pbl_coredump_stored {
  /** Backend storing the dump. */
  const struct pbl_coredump_backend *backend;
  /** Identifier of the dump in the backend. */
  uint32_t id;
};

/**
 * @brief Find the most recent stored dump of the first backend that has one.
 *
 * @param unread_only Only consider dumps that have not been marked as read.
 * @param[out] stored The dump.
 * @return 0 on success, or -ENOENT if there is none.
 */
int pbl_coredump_stored_find(bool unread_only, struct pbl_coredump_stored *stored);

/**
 * @brief Get the size of a stored dump.
 *
 * @param stored The dump.
 * @param[out] size Size in bytes.
 * @return 0 on success, or a negative errno, -EBADMSG if the dump is corrupted.
 */
int pbl_coredump_stored_size(const struct pbl_coredump_stored *stored, uint32_t *size);

/**
 * @brief Read a stored dump.
 *
 * @param stored The dump.
 * @param offset Offset in the dump.
 * @param[out] buf Destination.
 * @param len Number of bytes to read.
 * @return 0 on success, or a negative errno.
 */
int pbl_coredump_stored_read(const struct pbl_coredump_stored *stored, uint32_t offset, void *buf,
                             size_t len);

/**
 * @brief Mark a stored dump as read, so it can be replaced.
 *
 * @param stored The dump.
 * @return 0 on success, or a negative errno.
 */
int pbl_coredump_stored_mark_read(const struct pbl_coredump_stored *stored);

#ifdef CONFIG_COREDUMP_TEST_FAULT
/** @brief Fault to inject while a dump is being written, to test recovering from it. */
enum pbl_coredump_test_fault {
  /** No fault. */
  PBL_COREDUMP_TEST_FAULT_NONE,
  /** Jump to an invalid address. */
  PBL_COREDUMP_TEST_FAULT_BUS,
  /** Loop forever. */
  PBL_COREDUMP_TEST_FAULT_LOOP,
  /** Fail an assertion. */
  PBL_COREDUMP_TEST_FAULT_ASSERT,
};

/** @brief Inject @p fault in the next dump. */
void pbl_coredump_test_fault_set(enum pbl_coredump_test_fault fault);
#endif

/** @} */
