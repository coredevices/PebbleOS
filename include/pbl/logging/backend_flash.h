/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup logging_backend_flash Flash backend
 * @ingroup logging
 * @brief Log messages stored in the debug flash region, one generation per boot.
 * @{
 */

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Called with each stored message of a dumped generation.
 *
 * @param message Binary log message.
 * @param length Message length, in bytes.
 *
 * @return true if the message was consumed, false to retry it.
 */
typedef bool (*pbl_log_flash_dump_line_cb_t)(uint8_t *message, uint32_t length);

/**
 * @brief Called once a dump finishes.
 *
 * @param success false if the generation does not exist or a message could not be consumed.
 */
typedef void (*pbl_log_flash_dump_done_cb_t)(bool success);

/** @brief Start a new log generation and enable the flash backend. */
void pbl_log_flash_init(void);

/**
 * @brief Dump a log generation, one message per background work queue callback.
 *
 * @param generation 0 for the current boot, 1 for the previous one, and so on.
 * @param line_cb Called with each message.
 * @param done_cb Called once the dump finishes, also when the generation does not exist.
 *
 * @return true if the generation exists.
 */
bool pbl_log_flash_dump(int generation, pbl_log_flash_dump_line_cb_t line_cb,
                        pbl_log_flash_dump_done_cb_t done_cb);

/** @} */
