/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup pp_main Main thread
 * @ingroup pp
 * @brief The thread the protocol runs on, provided by the application.
 *
 * Transports send from it, and the handlers of the endpoints defined with
 * PBL_PP_ENDPOINT_DEFINE_MAIN() run on it. It must not be the system work queue, where the other
 * handlers run and may wait for the transport to make room.
 * @{
 */

/**
 * @brief Run a function on the main thread. Implemented by the application.
 *
 * Calls run in the order they are made, and none may be dropped.
 *
 * @param fn Function.
 * @param data Argument of @p fn.
 */
void pbl_pp_main_call(void (*fn)(void *data), void *data);

/**
 * @brief Check whether the caller runs on the main thread. Implemented by the application.
 *
 * @return true on the main thread.
 */
bool pbl_pp_main_is_current(void);

/** @} */
