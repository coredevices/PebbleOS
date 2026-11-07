/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/section.h>

/**
 * @defgroup coredump_backend Backends
 * @ingroup coredump
 * @brief Outputs of core dumps.
 *
 * A backend is defined with @ref PBL_COREDUMP_BACKEND_DEFINE, and every backend gets every dump. A
 * backend that fails, or that has no room left, stops getting the dump without affecting the
 * others. The core only knows backends through this interface.
 *
 * @c init, @c start, @c write, @c space and @c end run while the system is crashing, so they must
 * not use OS services. The @c stored_* operations run in thread context, and backends that only
 * output a dump, rather than keep it, leave them NULL.
 * @{
 */

struct pbl_coredump_backend;

/** @brief Backend operations. */
struct pbl_coredump_backend_api {
  /**
   * @brief Bring up what the backend needs without OS services. Optional.
   *
   * Runs before pbl_coredump_platform_prepare(), and must not change what is stored.
   *
   * @return 0 on success, or a negative errno.
   */
  int (*init)(const struct pbl_coredump_backend *backend);
  /**
   * @brief Get ready to receive a dump. Stored dumps can be replaced from here on.
   *
   * @param backend Backend.
   * @param forced Replace a dump that has not been read yet, however recent.
   * @param timestamp Wall clock time of the dump, in seconds.
   * @return 0 on success, -EEXIST to keep the stored dump and skip this one, or a negative errno.
   */
  int (*start)(const struct pbl_coredump_backend *backend, bool forced, uint32_t timestamp);
  /**
   * @brief Write the next bytes of the dump.
   *
   * @return 0 on success, or a negative errno.
   */
  int (*write)(const struct pbl_coredump_backend *backend, const void *data, size_t len);
  /**
   * @brief Get the bytes that can still be written.
   *
   * Lets the core end the image cleanly, between chunks. Optional, for unbounded outputs.
   */
  size_t (*space)(const struct pbl_coredump_backend *backend);
  /** @brief The image is complete. Optional. */
  void (*end)(const struct pbl_coredump_backend *backend);
  /**
   * @brief Find the most recent stored dump. Optional.
   *
   * @param backend Backend.
   * @param unread_only Only consider dumps that have not been marked as read.
   * @param[out] id Identifier of the dump.
   * @return 0 on success, or -ENOENT if there is none.
   */
  int (*stored_find)(const struct pbl_coredump_backend *backend, bool unread_only, uint32_t *id);
  /**
   * @brief Read a stored dump. Required with @ref stored_find.
   *
   * @return 0 on success, or a negative errno, also for a range outside the storage of the dump.
   */
  int (*stored_read)(const struct pbl_coredump_backend *backend, uint32_t id, uint32_t offset,
                     void *buf, size_t len);
  /**
   * @brief Mark a stored dump as read. Required with @ref stored_find.
   *
   * @return 0 on success, or a negative errno.
   */
  int (*stored_mark_read)(const struct pbl_coredump_backend *backend, uint32_t id);
};

/** @brief Backend. */
struct pbl_coredump_backend {
  /** Name. */
  const char *name;
  /** Operations. */
  const struct pbl_coredump_backend_api *api;
};

/**
 * @brief Define a backend.
 *
 * @param _name Backend name, also its identifier.
 * @param _api Backend operations.
 */
#ifdef PBL_NO_LINKER_SCRIPT
#define PBL_COREDUMP_BACKEND_DEFINE(_name, _api)                                  \
  static const struct pbl_coredump_backend _pbl_coredump_backend_##_name PBL_USED \
  PBL_UNSORTED_SECTION(pbl_cd_be) = {.name = #_name, .api = &(_api)}
#else
#define PBL_COREDUMP_BACKEND_DEFINE(_name, _api)                                                 \
  static const struct pbl_coredump_backend _pbl_coredump_backend_##_name PBL_USED PBL_ALIGNED(4) \
      PBL_SECTION(".pbl_coredump_backends." #_name) = {.name = #_name, .api = &(_api)}
#endif

/** @} */
