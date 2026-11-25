/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @file
 * Runs GATT client procedures one at a time.
 *
 * The stack runs them concurrently, and hands an answer of the peer to the first procedure that
 * takes answers of that kind: two that do, such as a discovery of characteristics and a read by
 * UUID, would get each other's. Operations are also started on the host task, where disconnections
 * are handled: the stack sends a request before it tracks its procedure, so a disconnection handled
 * in between would leave the procedure waiting for its timeout.
 */

/**
 * @brief Start an operation, on the host task.
 *
 * @param ctx As pushed.
 * @return 0 if it started: pbl_bt__gattc_op_queue_complete() is then called exactly once when it
 *         is done. Anything else drops the operation, which is reported by the function itself.
 */
typedef int (*pbl_bt__gattc_op_start_fn_t)(void *ctx);

/**
 * @brief Queue an operation. Callable from any thread.
 *
 * @param start Starts it.
 * @param ctx Heap allocated, or NULL: freed when the operation is done, fails to start or is
 *            dropped.
 * @retval 0 Queued.
 * @retval -ENOMEM Out of memory: @p ctx is the caller's still.
 */
int pbl_bt__gattc_op_queue_push(pbl_bt__gattc_op_start_fn_t start, void *ctx);

/** @brief Tell that the running operation is done, and start the next one. */
void pbl_bt__gattc_op_queue_complete(void);

/** @brief Drop every operation. */
void pbl_bt__gattc_op_queue_init(void);
