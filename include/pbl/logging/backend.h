/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdatomic.h>
#include <stdint.h>

/**
 * @defgroup logging_backend Backends
 * @ingroup logging
 * @brief Outputs of log messages.
 *
 * A backend is defined with @ref PBL_LOG_BACKEND_DEFINE and receives every message at or below
 * its level through @ref pbl_log_backend_api::process, in the context of the caller. The
 * logging core only knows backends through this interface.
 * @{
 */

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/section.h>

/** @brief The message may be written after @ref pbl_log_backend_api::process returns. */
#define PBL_LOG_MSG_ASYNC (1U << 0)
/** @brief The message comes from a fault handler, where OS services are unavailable. */
#define PBL_LOG_MSG_FAULT (1U << 1)

/** @brief Log message. */
struct pbl_log_msg {
  /** Source file name. */
  const char *file;
  /** Message text, NUL-terminated. */
  const char *text;
  /** UUID of the app that logged the message, or NULL for system messages. */
  const uint8_t *app_uuid;
  /** Time the message was logged at, in seconds, or 0 if unknown. */
  uint32_t timestamp;
  /** Source line number. */
  uint16_t line;
  /** LOG_LEVEL_* level. */
  uint8_t level;
  /** Length of @ref text in the binary log message format. */
  uint8_t text_len;
  /** PBL_LOG_MSG_* flags. */
  uint8_t flags;
};

struct pbl_log_backend;

/** @brief Backend operations. */
struct pbl_log_backend_api {
  /** Bring up the backend's output before main(), from @ref pbl_log_early_init. Optional. */
  void (*early_init)(const struct pbl_log_backend *backend);
  /** Initialize the backend, from @ref pbl_log_init. Optional. */
  void (*init)(const struct pbl_log_backend *backend);
  /** Write a message. */
  void (*process)(const struct pbl_log_backend *backend, const struct pbl_log_msg *msg);
  /** Write out every pending message, the system is about to stop. Optional. */
  void (*panic)(const struct pbl_log_backend *backend);
  /** Get the messages kept since boot in @ref pbl_log_backend_start. */
  bool replay;
};

/** @brief Runtime state of a backend. */
struct pbl_log_backend_state {
  /** Highest LOG_LEVEL_* level written. */
  _Atomic int16_t level;
  /** Whether messages are passed to the backend. */
  atomic_bool enabled;
};

/** @brief Backend. */
struct pbl_log_backend {
  /** Name. */
  const char *name;
  /** Operations. */
  const struct pbl_log_backend_api *api;
  /** Runtime state. */
  struct pbl_log_backend_state *state;
};

/**
 * @brief Define a backend.
 *
 * @param _name Backend name, also its identifier.
 * @param _api Backend operations.
 * @param _level Initial highest LOG_LEVEL_* level written.
 * @param _autostart Whether the backend starts enabled, or waits for
 *                   @ref pbl_log_backend_enable.
 */
#ifdef PBL_NO_LINKER_SCRIPT
#define PBL_LOG_BACKEND_DEFINE(_name, _api, _level, _autostart)                        \
  static struct pbl_log_backend_state _pbl_log_backend_state_##_name = {               \
    .level = (_level),                                                                 \
    .enabled = (_autostart)                                                            \
  };                                                                                   \
  const struct pbl_log_backend _pbl_log_backend_##_name PBL_USED PBL_UNSORTED_SECTION( \
      pbl_log_be) = {.name = #_name, .api = &(_api), .state = &_pbl_log_backend_state_##_name}
#else
#define PBL_LOG_BACKEND_DEFINE(_name, _api, _level, _autostart)                 \
  static struct pbl_log_backend_state _pbl_log_backend_state_##_name = {        \
    .level = (_level),                                                          \
    .enabled = (_autostart)                                                     \
  };                                                                            \
  const struct pbl_log_backend _pbl_log_backend_##_name PBL_USED PBL_ALIGNED(4) \
      PBL_SECTION(".pbl_log_backends." #_name) = {                              \
        .name = #_name,                                                         \
        .api = &(_api),                                                         \
        .state = &_pbl_log_backend_state_##_name,                               \
  }
#endif

/**
 * @brief Get a backend defined with @ref PBL_LOG_BACKEND_DEFINE.
 *
 * @param _name Backend name.
 */
#define PBL_LOG_BACKEND_GET(_name)                                \
  ({                                                              \
    extern const struct pbl_log_backend _pbl_log_backend_##_name; \
    &_pbl_log_backend_##_name;                                    \
  })

/** @brief Start passing messages to @p backend. */
void pbl_log_backend_enable(const struct pbl_log_backend *backend);

/**
 * @brief Replay the messages kept since boot to @p backend, then start passing messages to it.
 *
 * For backends that come up after boot, e.g. once their storage is ready. Messages logged while
 * a backend with @ref pbl_log_backend_api::replay that wanted them was disabled are kept in a
 * @c CONFIG_LOG_BOOT_BUFFER_SIZE buffer, until it is full. Backends without it are only
 * enabled.
 */
void pbl_log_backend_start(const struct pbl_log_backend *backend);

/** @brief Stop passing messages to @p backend. */
void pbl_log_backend_disable(const struct pbl_log_backend *backend);

/** @brief Whether messages are passed to @p backend. */
bool pbl_log_backend_is_enabled(const struct pbl_log_backend *backend);

/** @brief Set the highest LOG_LEVEL_* level written by @p backend. */
void pbl_log_backend_set_level(const struct pbl_log_backend *backend, int level);

/**
 * @brief Find a backend by name.
 *
 * @return The backend, or NULL if there is none called @p name.
 */
const struct pbl_log_backend *pbl_log_backend_get_by_name(const char *name);

/** @brief Bring up the backends' output, called by the kernel before main(). */
void pbl_log_early_init(void);

/** @brief Initialize the backends. */
void pbl_log_init(void);

/** @brief Have every backend write out its pending messages, the system is about to stop. */
void pbl_log_panic(void);

/** @} */
