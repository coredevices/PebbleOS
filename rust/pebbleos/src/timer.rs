// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

//! One-shot timers.

use core::cell::UnsafeCell;
use core::mem;
use core::ops::Deref;
use core::time::Duration;

use crate::sys;
use crate::time::{self, Timeout};

/// A timer that calls a function once it expires, in the tick interrupt: the
/// function must be short and never block.
///
/// It holds data of type `T` its function gets through it, and starts again
/// from its function to repeat.
#[repr(C)]
pub struct Timer<T: Sync + 'static = ()> {
    raw: UnsafeCell<sys::pbl_timer>,
    handler: fn(&'static Timer<T>),
    data: T,
}

// SAFETY: the kernel serializes access, and the data is Sync.
unsafe impl<T: Sync> Sync for Timer<T> {}

impl<T: Sync> Timer<T> {
    /// A stopped timer that calls `handler`, with `data`.
    pub const fn new(handler: fn(&'static Timer<T>), data: T) -> Self {
        // SAFETY: all-zero is a stopped timer, as PBL_TIMER_INITIALIZER sets it up.
        let mut raw: sys::pbl_timer = unsafe { mem::zeroed() };
        raw.fn_ = Some(expired::<T>);
        Timer {
            raw: UnsafeCell::new(raw),
            handler,
            data,
        }
    }

    /// Starts it, or starts it again, to expire after `delay`.
    pub fn start(&'static self, delay: impl Into<Timeout>) {
        let delay = delay.into();
        assert!(delay != Timeout::FOREVER, "a timer must expire");
        // SAFETY: the timer is static.
        unsafe { sys::pbl_timer_start(self.raw.get(), delay.0) }
    }

    /// Stops it; its function no longer runs unless it already does.
    pub fn stop(&self) {
        // SAFETY: the timer is valid.
        unsafe { sys::pbl_timer_stop(self.raw.get()) }
    }

    /// Whether it is running.
    pub fn is_active(&self) -> bool {
        // SAFETY: the timer is valid.
        unsafe { sys::pbl_timer_is_active(self.raw.get()) }
    }

    /// Time until it expires, zero if it is stopped.
    pub fn remaining(&self) -> Duration {
        // SAFETY: the timer is valid.
        time::ticks_to_duration(unsafe { sys::pbl_timer_remaining(self.raw.get()) })
    }
}

impl<T: Sync> Deref for Timer<T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.data
    }
}

unsafe extern "C" fn expired<T: Sync + 'static>(raw: *mut sys::pbl_timer) {
    // SAFETY: the kernel timer is the first field of a static Timer<T>.
    let timer = unsafe { &*(raw as *const Timer<T>) };
    (timer.handler)(timer);
}
