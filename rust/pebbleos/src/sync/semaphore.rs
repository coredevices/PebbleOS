// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

use core::cell::UnsafeCell;
use core::mem;

use crate::errno::{Errno, Result};
use crate::sys;
use crate::time::Timeout;

/// A count of tokens, taken by threads and given by threads or interrupts.
///
/// With a limit of 1 it signals an event, with more it shares a number of
/// resources.
pub struct Semaphore(UnsafeCell<sys::pbl_sem>);

// SAFETY: the kernel serializes access.
unsafe impl Send for Semaphore {}
unsafe impl Sync for Semaphore {}

impl Semaphore {
    /// A semaphore of `initial` tokens, which never holds more than `limit`.
    pub const fn new(initial: u32, limit: u32) -> Semaphore {
        assert!(limit > 0 && initial <= limit);
        // SAFETY: all-zero is a valid start, as PBL_SEM_INITIALIZER sets it up.
        let mut sem: sys::pbl_sem = unsafe { mem::zeroed() };
        sem.initial = initial;
        sem.limit = limit;
        sem.backend.count = initial;
        Semaphore(UnsafeCell::new(sem))
    }

    /// Takes a token, waiting for up to `timeout` for one.
    ///
    /// Fails with [`Errno::EAGAIN`] if none came by then, or [`Errno::EBUSY`]
    /// without waiting.
    pub fn take(&self, timeout: impl Into<Timeout>) -> Result<()> {
        // SAFETY: the semaphore is valid.
        Errno::result(unsafe { sys::pbl_sem_take(self.0.get(), timeout.into().0) })
    }

    /// Gives a token back, from a thread or an interrupt, unless it is at
    /// its limit.
    pub fn give(&self) {
        // SAFETY: the semaphore is valid.
        unsafe { sys::pbl_sem_give(self.0.get()) }
    }

    /// Back to its initial count.
    pub fn reset(&self) {
        // SAFETY: the semaphore is valid.
        unsafe { sys::pbl_sem_reset(self.0.get()) }
    }

    /// Tokens it holds.
    pub fn count(&self) -> u32 {
        // SAFETY: the semaphore is valid.
        unsafe { sys::pbl_sem_count(self.0.get()) }
    }
}
