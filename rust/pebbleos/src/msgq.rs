// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

//! Message queues.

use core::cell::UnsafeCell;
use core::ffi::c_void;
use core::mem::{self, MaybeUninit};
use core::sync::atomic::AtomicBool;

use crate::errno::{Errno, Result};
use crate::irq;
use crate::sys;
use crate::time::Timeout;

/// A queue of up to `N` messages of type `T`, copied in and out, between
/// threads and interrupts.
///
/// Putting into a full queue waits for room, which holds a fast producer
/// back; getting from an empty one waits for a message.
#[repr(C)]
pub struct MsgQueue<T: Copy + Send, const N: usize> {
    raw: UnsafeCell<sys::pbl_msgq>,
    buf: UnsafeCell<[MaybeUninit<T>; N]>,
    ready: AtomicBool,
}

// SAFETY: the kernel serializes access, and messages are Send.
unsafe impl<T: Copy + Send, const N: usize> Sync for MsgQueue<T, N> {}

impl<T: Copy + Send, const N: usize> MsgQueue<T, N> {
    /// An empty queue.
    pub const fn new() -> Self {
        const {
            assert!(mem::size_of::<T>() > 0 && N > 0 && N <= u32::MAX as usize);
        }
        MsgQueue {
            // SAFETY: set up on first use, as PBL_MSGQ_INITIALIZER does, so
            // that the queue and its buffer need no initial values.
            raw: UnsafeCell::new(unsafe { mem::zeroed() }),
            buf: UnsafeCell::new([const { MaybeUninit::uninit() }; N]),
            ready: AtomicBool::new(false),
        }
    }

    pub(crate) fn raw(&'static self) -> *mut sys::pbl_msgq {
        // The buffer's address is only known once the queue is in place.
        irq::once(&self.ready, || {
            // SAFETY: no other user before the queue is ready.
            unsafe {
                let raw = self.raw.get();
                (*raw).buf = self.buf.get() as *mut c_void;
                (*raw).msg_size = mem::size_of::<T>();
                (*raw).max_msgs = N as u32;
            }
        });
        self.raw.get()
    }

    /// Puts `msg` at the back, waiting for up to `timeout` for room.
    ///
    /// Fails with [`Errno::EAGAIN`] if there is none by then, or
    /// [`Errno::EBUSY`] without waiting.
    pub fn put(&'static self, msg: T, timeout: impl Into<Timeout>) -> Result<()> {
        let msg = &msg as *const T as *const c_void;
        // SAFETY: the kernel copies the message.
        Errno::result(unsafe { sys::pbl_msgq_put(self.raw(), msg, timeout.into().0) })
    }

    /// Puts `msg` at the front, to come out next.
    pub fn put_front(&'static self, msg: T, timeout: impl Into<Timeout>) -> Result<()> {
        let msg = &msg as *const T as *const c_void;
        // SAFETY: the kernel copies the message.
        Errno::result(unsafe { sys::pbl_msgq_put_front(self.raw(), msg, timeout.into().0) })
    }

    /// Takes the message at the front, waiting for up to `timeout` for one.
    ///
    /// Fails with [`Errno::EAGAIN`] if none came by then, or [`Errno::EBUSY`]
    /// without waiting.
    pub fn get(&'static self, timeout: impl Into<Timeout>) -> Result<T> {
        let mut msg = MaybeUninit::<T>::uninit();
        let out = msg.as_mut_ptr() as *mut c_void;
        // SAFETY: the kernel copies a whole message out.
        Errno::result(unsafe { sys::pbl_msgq_get(self.raw(), out, timeout.into().0) })?;
        // SAFETY: it did.
        Ok(unsafe { msg.assume_init() })
    }

    /// The message at the front, which stays there.
    ///
    /// Fails with [`Errno::EBUSY`] if the queue is empty.
    pub fn peek(&'static self) -> Result<T> {
        let mut msg = MaybeUninit::<T>::uninit();
        let out = msg.as_mut_ptr() as *mut c_void;
        // SAFETY: the kernel copies a whole message out.
        Errno::result(unsafe { sys::pbl_msgq_peek(self.raw(), out) })?;
        // SAFETY: it did.
        Ok(unsafe { msg.assume_init() })
    }

    /// Drops every message, and lets every waiting producer go on.
    pub fn purge(&'static self) {
        // SAFETY: the queue is valid.
        unsafe { sys::pbl_msgq_purge(self.raw()) }
    }

    /// Messages queued.
    pub fn len(&'static self) -> usize {
        // SAFETY: the queue is valid.
        unsafe { sys::pbl_msgq_num_used(self.raw()) as usize }
    }

    /// Whether no message is queued.
    pub fn is_empty(&'static self) -> bool {
        self.len() == 0
    }

    /// Messages it holds when full.
    pub const fn capacity(&self) -> usize {
        N
    }
}

impl<T: Copy + Send, const N: usize> Default for MsgQueue<T, N> {
    fn default() -> Self {
        Self::new()
    }
}
