/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Serializes GATT client procedures.
//!
//! NimBLE runs GATT client procedures concurrently and dispatches incoming ATT
//! responses to the first pending procedure that consumes that response
//! opcode. Two concurrent procedures consuming the same opcode (e.g.
//! characteristic discovery and a read-by-UUID, which both consume ATT Read By
//! Type responses) get responses cross-delivered, corrupting both. This queue
//! guarantees only one GATT client procedure runs at a time.
//!
//! Ops are started from KernelBG, so pushing is safe from any context,
//! including with bt_lock held.

//! Starts the operation. Called from KernelBG without bt_lock held. Returns 0
//! if the operation was started, in which case its completion path must call
//! nimble_gattc_op_queue_complete() exactly once. On a non-zero return the op
//! is dropped and the next one runs; error reporting is the starter's
//! responsibility.
typedef int (*NimbleGattClientOpStartFn)(void *ctx);

//! Enqueues an operation. ctx must be heap-allocated (or NULL); it is
//! kernel_free()'d when the op completes, fails to start, or is dropped.
void nimble_gattc_op_queue_push(NimbleGattClientOpStartFn start, void *ctx);

//! Signals completion of the running operation and kicks the next one.
void nimble_gattc_op_queue_complete(void);

void nimble_gattc_op_queue_init(void);
