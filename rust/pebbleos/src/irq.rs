// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

use core::sync::atomic::{AtomicBool, Ordering};

use crate::sys;

/// Runs `f` with interrupts locked, from a thread or an interrupt.
pub(crate) fn locked<R>(f: impl FnOnce() -> R) -> R {
    // SAFETY: balanced, and nestable.
    unsafe { sys::pbl_irq_lock() };
    let r = f();
    unsafe { sys::pbl_irq_unlock() };
    r
}

/// Runs `init` once, before the first use of what it sets up.
pub(crate) fn once(done: &AtomicBool, init: impl FnOnce()) {
    if !done.load(Ordering::Acquire) {
        locked(|| {
            if !done.load(Ordering::Relaxed) {
                init();
                done.store(true, Ordering::Release);
            }
        });
    }
}
