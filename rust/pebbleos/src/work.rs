// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

//! Work deferred to a thread: the system's work queue, or one of its own.

use core::cell::UnsafeCell;
use core::ffi::{CStr, c_void};
use core::mem;
use core::ops::Deref;
use core::ptr;
use core::time::Duration;

use crate::errno::{Errno, Result};
use crate::irq;
use crate::sys;
use crate::thread::{Priority, Thread};
use crate::time::{self, Timeout};

/// A thread that runs work items one after the other, in the order they
/// were submitted.
#[repr(transparent)]
pub struct WorkQueue(UnsafeCell<sys::pbl_workq>);

// SAFETY: the kernel serializes access.
unsafe impl Sync for WorkQueue {}

impl WorkQueue {
    /// A queue without its thread yet, see [`WorkQueue::start`].
    pub const fn new() -> WorkQueue {
        // SAFETY: all-zero is a valid start, as PBL_WORKQ_INITIALIZER sets it up.
        let mut q: sys::pbl_workq = unsafe { mem::zeroed() };
        q.wake.limit = 1;
        WorkQueue(UnsafeCell::new(q))
    }

    /// The system's queue for background work.
    pub fn system() -> &'static WorkQueue {
        // SAFETY: the kernel's queue, which WorkQueue wraps transparently.
        unsafe { &*(&raw mut sys::pbl_sys_workq as *const WorkQueue) }
    }

    /// Runs the queue on `thread`, named `name`, at `prio`.
    ///
    /// Fails with [`Errno::EBUSY`] if the queue already has a thread.
    pub fn start<const STACK: usize>(
        &'static self,
        thread: &'static Thread<STACK>,
        name: &CStr,
        prio: Priority,
    ) -> Result<()> {
        let q = self.0.get();
        // SAFETY: the queue is valid, and its thread only changes here.
        let claimed = irq::locked(|| unsafe {
            let free = (*q).thread.is_null();
            if free {
                (*q).thread = thread.handle().as_ptr();
            }
            free
        });
        if !claimed {
            return Err(Errno::EBUSY);
        }

        let ret = thread.spawn(name, prio, move || {
            // SAFETY: the queue is static, and this thread runs it for good.
            unsafe { sys::pbl_workq_thread(self.0.get() as *mut c_void) }
        });
        if ret.is_err() {
            // SAFETY: as above.
            unsafe { (*q).thread = ptr::null_mut() };
        }
        ret
    }

    /// Waits for every item submitted so far to have run.
    pub fn flush(&self) {
        // SAFETY: the queue is valid.
        unsafe { sys::pbl_workq_flush(self.0.get()) }
    }

    fn raw(&self) -> *mut sys::pbl_workq {
        self.0.get()
    }
}

impl Default for WorkQueue {
    fn default() -> Self {
        Self::new()
    }
}

/// A function to run on a work queue, with data of type `T` it gets through
/// the item.
///
/// Submitting it again while it is pending does nothing, so one run serves
/// every submission since the last.
#[repr(C)]
pub struct Work<T: Sync + 'static = ()> {
    raw: UnsafeCell<sys::pbl_work>,
    handler: fn(&'static Work<T>),
    data: T,
}

// SAFETY: the kernel serializes access, and the data is Sync.
unsafe impl<T: Sync> Sync for Work<T> {}

impl<T: Sync> Work<T> {
    /// An item that runs `handler`, with `data`.
    pub const fn new(handler: fn(&'static Work<T>), data: T) -> Self {
        // SAFETY: all-zero is a valid start, set up as pbl_work_init() does.
        let mut raw: sys::pbl_work = unsafe { mem::zeroed() };
        raw.fn_ = Some(run_work::<T>);
        Work {
            raw: UnsafeCell::new(raw),
            handler,
            data,
        }
    }

    /// Queues it on `q`, from a thread or an interrupt; false if it was
    /// pending already.
    pub fn submit(&'static self, q: &'static WorkQueue) -> bool {
        // SAFETY: both are static.
        unsafe { sys::pbl_work_submit(q.raw(), self.raw.get()) }
    }

    /// Takes it off its queue; true if it was pending. It may be running.
    pub fn cancel(&self) -> bool {
        // SAFETY: the item is valid.
        unsafe { sys::pbl_work_cancel(self.raw.get()) }
    }

    /// Takes it off its queue, and waits for it if it is running; true if it
    /// was pending.
    pub fn cancel_sync(&self) -> bool {
        // SAFETY: the item is valid.
        unsafe { sys::pbl_work_cancel_sync(self.raw.get()) }
    }

    /// Waits for it to have run, if it is pending or running; true if it
    /// was.
    pub fn flush(&self) -> bool {
        // SAFETY: the item is valid.
        unsafe { sys::pbl_work_flush(self.raw.get()) }
    }

    /// Whether it is queued.
    pub fn is_pending(&self) -> bool {
        // SAFETY: the item is valid.
        unsafe { sys::pbl_work_is_pending(self.raw.get()) }
    }

    /// Whether it is running.
    pub fn is_running(&self) -> bool {
        // SAFETY: the item is valid.
        unsafe { sys::pbl_work_is_running(self.raw.get()) }
    }
}

impl<T: Sync> Deref for Work<T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.data
    }
}

unsafe extern "C" fn run_work<T: Sync + 'static>(raw: *mut sys::pbl_work) {
    // SAFETY: the kernel item is the first field of a static Work<T>.
    let work = unsafe { &*(raw as *const Work<T>) };
    (work.handler)(work);
}

/// A [`Work`] item that is queued after a delay, as debouncing, periodic
/// jobs and time limits need.
#[repr(C)]
pub struct DelayedWork<T: Sync + 'static = ()> {
    raw: UnsafeCell<sys::pbl_delayed_work>,
    handler: fn(&'static DelayedWork<T>),
    data: T,
}

// SAFETY: the kernel serializes access, and the data is Sync.
unsafe impl<T: Sync> Sync for DelayedWork<T> {}

impl<T: Sync> DelayedWork<T> {
    /// An item that runs `handler`, with `data`.
    pub const fn new(handler: fn(&'static DelayedWork<T>), data: T) -> Self {
        // SAFETY: all-zero is a valid start, as PBL_DELAYED_WORK_INITIALIZER
        // sets it up.
        let mut raw: sys::pbl_delayed_work = unsafe { mem::zeroed() };
        raw.work.fn_ = Some(run_delayed_work::<T>);
        raw.timer.fn_ = Some(sys::pbl_delayed_work_expired);
        DelayedWork {
            raw: UnsafeCell::new(raw),
            handler,
            data,
        }
    }

    /// Queues it on `q` after `delay`, unless it is pending or waiting
    /// already, which keeps the delay that runs; false then.
    pub fn schedule(&'static self, q: &'static WorkQueue, delay: impl Into<Timeout>) -> bool {
        let delay = delay.into();
        assert!(delay != Timeout::FOREVER, "delayed work must expire");
        // SAFETY: both are static.
        unsafe { sys::pbl_delayed_work_schedule(q.raw(), self.raw.get(), delay.0) }
    }

    /// Queues it on `q` after `delay`, replacing any delay that runs; false
    /// if it was neither pending nor waiting.
    pub fn reschedule(&'static self, q: &'static WorkQueue, delay: impl Into<Timeout>) -> bool {
        let delay = delay.into();
        assert!(delay != Timeout::FOREVER, "delayed work must expire");
        // SAFETY: both are static.
        unsafe { sys::pbl_delayed_work_reschedule(q.raw(), self.raw.get(), delay.0) }
    }

    /// From its own function: queues it again `period` after it last
    /// expired, on the queue it last ran on, so a periodic job does not
    /// drift.
    ///
    /// # Panics
    ///
    /// If it never ran on a queue.
    pub fn schedule_next(&'static self, period: Duration) {
        // SAFETY: the item is valid.
        assert!(
            !unsafe { (*self.raw.get()).work.q }.is_null(),
            "delayed work never scheduled"
        );
        // SAFETY: called from the item's function, as the kernel requires.
        unsafe {
            let ticks = sys::pbl_ms_to_ticks(time::millis(period));
            sys::pbl_delayed_work_schedule_next(self.raw.get(), ticks)
        }
    }

    /// Stops the delay, or takes it off its queue; true if either was the
    /// case. It may be running.
    pub fn cancel(&self) -> bool {
        // SAFETY: the item is valid.
        unsafe { sys::pbl_delayed_work_cancel(self.raw.get()) }
    }

    /// As [`DelayedWork::cancel`], and waits for it if it is running, which
    /// also stops one that schedules itself again.
    pub fn cancel_sync(&self) -> bool {
        // SAFETY: the item is valid.
        unsafe { sys::pbl_delayed_work_cancel_sync(self.raw.get()) }
    }

    /// Whether it is waiting for its delay or queued.
    pub fn is_pending(&self) -> bool {
        // SAFETY: the item is valid.
        unsafe { sys::pbl_delayed_work_is_pending(self.raw.get()) }
    }

    /// Time until it is queued, zero if it is not waiting.
    pub fn remaining(&self) -> Duration {
        // SAFETY: the item is valid.
        time::ticks_to_duration(unsafe { sys::pbl_delayed_work_remaining(self.raw.get()) })
    }
}

impl<T: Sync> Deref for DelayedWork<T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.data
    }
}

unsafe extern "C" fn run_delayed_work<T: Sync + 'static>(raw: *mut sys::pbl_work) {
    // SAFETY: the kernel item is the first field of the delayed one, itself
    // the first field of a static DelayedWork<T>.
    let work = unsafe { &*(raw as *const DelayedWork<T>) };
    (work.handler)(work);
}
