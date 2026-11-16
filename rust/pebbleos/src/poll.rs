// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

//! Waiting on several message queues at once.

use core::cell::UnsafeCell;
use core::mem;
use core::ptr;

use crate::msgq::MsgQueue;
use crate::sys;
use crate::time::Timeout;

/// Message queues one thread waits on together, as an event loop does.
///
/// Queues join it before the first wait, and for good.
pub struct PollGroup(UnsafeCell<sys::pbl_poll_group>);

// SAFETY: the kernel serializes access.
unsafe impl Sync for PollGroup {}

impl PollGroup {
    /// A group without queues.
    pub const fn new() -> PollGroup {
        // SAFETY: an all-zero group is an empty one.
        PollGroup(UnsafeCell::new(unsafe { mem::zeroed() }))
    }

    /// Adds `q`.
    ///
    /// # Panics
    ///
    /// If `q` is in a group already.
    pub fn add<T: Copy + Send, const N: usize>(&'static self, q: &'static MsgQueue<T, N>) {
        // SAFETY: both are static; the kernel asserts q is in no group.
        unsafe { sys::pbl_poll_group_add(self.0.get(), q.raw()) }
    }

    /// Waits for up to `timeout` for a message in any of the queues, and
    /// returns that queue, where the message still is; `None` if none came.
    ///
    /// # Panics
    ///
    /// If the group has no queues.
    pub fn wait(&'static self, timeout: impl Into<Timeout>) -> Option<Ready> {
        // SAFETY: the group is valid.
        let q = unsafe { sys::pbl_poll_group_wait(self.0.get(), timeout.into().0) };
        (!q.is_null()).then_some(Ready(q))
    }
}

impl Default for PollGroup {
    fn default() -> Self {
        Self::new()
    }
}

/// The queue a [`PollGroup::wait`] found a message in.
#[derive(Clone, Copy)]
pub struct Ready(*mut sys::pbl_msgq);

impl Ready {
    /// Whether it is `q`.
    pub fn is<T: Copy + Send, const N: usize>(self, q: &'static MsgQueue<T, N>) -> bool {
        ptr::eq(self.0, q.raw())
    }
}
