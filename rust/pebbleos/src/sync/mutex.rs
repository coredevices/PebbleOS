// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

use core::cell::UnsafeCell;
use core::marker::PhantomData;
use core::mem;
use core::ops::{Deref, DerefMut};

use crate::errno::{Errno, Result};
use crate::sys;
use crate::time::Timeout;

/// The kernel's mutex: owned by the thread that locks it, which may lock it
/// again, and whose priority is raised to that of the most urgent waiter.
pub(crate) struct RawMutex(UnsafeCell<sys::pbl_mutex>);

// SAFETY: the kernel serializes access.
unsafe impl Send for RawMutex {}
unsafe impl Sync for RawMutex {}

impl RawMutex {
    pub(crate) const fn new() -> RawMutex {
        // SAFETY: an all-zero mutex is an unlocked one.
        RawMutex(UnsafeCell::new(unsafe { mem::zeroed() }))
    }

    pub(crate) fn lock(&self, timeout: Timeout) -> Result<()> {
        // SAFETY: the mutex is valid; the kernel serializes access.
        Errno::result(unsafe { sys::pbl_mutex_lock(self.0.get(), timeout.0) })
    }

    pub(crate) fn unlock(&self) {
        // SAFETY: called by the owner, see the guards.
        unsafe { sys::pbl_mutex_unlock(self.0.get()) }
    }

    pub(crate) fn is_owner(&self) -> bool {
        // SAFETY: the mutex is valid.
        unsafe { sys::pbl_mutex_is_owner(self.0.get()) }
    }
}

/// Data shared between threads, one at a time.
///
/// Waiting threads queue by priority, and the owner runs at the priority of
/// the most urgent one. Not for interrupts. A thread that locks it again
/// while it holds it panics, as it would get the data twice.
pub struct Mutex<T: ?Sized> {
    raw: RawMutex,
    data: UnsafeCell<T>,
}

// SAFETY: the lock gives the data to one thread at a time.
unsafe impl<T: ?Sized + Send> Send for Mutex<T> {}
unsafe impl<T: ?Sized + Send> Sync for Mutex<T> {}

impl<T> Mutex<T> {
    /// An unlocked mutex holding `data`.
    pub const fn new(data: T) -> Mutex<T> {
        Mutex {
            raw: RawMutex::new(),
            data: UnsafeCell::new(data),
        }
    }
}

impl<T: ?Sized> Mutex<T> {
    /// Waits for the mutex for as long as it takes.
    pub fn lock(&self) -> MutexGuard<'_, T> {
        match self.lock_timeout(Timeout::FOREVER) {
            Ok(guard) => guard,
            Err(e) => panic!("mutex lock failed: {e}"),
        }
    }

    /// Takes the mutex if it is free.
    ///
    /// Fails with [`Errno::EBUSY`] if it is not.
    pub fn try_lock(&self) -> Result<MutexGuard<'_, T>> {
        self.lock_timeout(Timeout::NO_WAIT)
    }

    /// Waits for the mutex for up to `timeout`.
    ///
    /// Fails with [`Errno::EAGAIN`] if it is still taken by then, or
    /// [`Errno::EBUSY`] without waiting.
    pub fn lock_timeout(&self, timeout: impl Into<Timeout>) -> Result<MutexGuard<'_, T>> {
        assert!(!self.raw.is_owner(), "mutex already locked by this thread");
        self.raw.lock(timeout.into())?;
        Ok(MutexGuard {
            mutex: self,
            _not_send: PhantomData,
        })
    }

    /// The data, which `&mut` keeps any other thread from using.
    pub fn get_mut(&mut self) -> &mut T {
        self.data.get_mut()
    }
}

/// Access to the data of a locked [`Mutex`], which unlocks it when dropped.
pub struct MutexGuard<'a, T: ?Sized> {
    mutex: &'a Mutex<T>,
    _not_send: PhantomData<*const ()>,
}

// SAFETY: sharing the guard shares &T.
unsafe impl<T: ?Sized + Sync> Sync for MutexGuard<'_, T> {}

impl<T: ?Sized> Deref for MutexGuard<'_, T> {
    type Target = T;

    fn deref(&self) -> &T {
        // SAFETY: the lock is held.
        unsafe { &*self.mutex.data.get() }
    }
}

impl<T: ?Sized> DerefMut for MutexGuard<'_, T> {
    fn deref_mut(&mut self) -> &mut T {
        // SAFETY: the lock is held, by this guard only.
        unsafe { &mut *self.mutex.data.get() }
    }
}

impl<T: ?Sized> Drop for MutexGuard<'_, T> {
    fn drop(&mut self) {
        self.mutex.raw.unlock();
    }
}

/// A mutex its owner may lock again, which shares its data: wrap what
/// changes in a `Cell` or `RefCell`.
pub struct ReentrantMutex<T: ?Sized> {
    raw: RawMutex,
    data: T,
}

// SAFETY: the lock gives the data to one thread at a time.
unsafe impl<T: ?Sized + Send> Send for ReentrantMutex<T> {}
unsafe impl<T: ?Sized + Send> Sync for ReentrantMutex<T> {}

impl<T> ReentrantMutex<T> {
    /// An unlocked mutex holding `data`.
    pub const fn new(data: T) -> ReentrantMutex<T> {
        ReentrantMutex {
            raw: RawMutex::new(),
            data,
        }
    }
}

impl<T: ?Sized> ReentrantMutex<T> {
    /// Waits for the mutex for as long as it takes.
    pub fn lock(&self) -> ReentrantMutexGuard<'_, T> {
        match self.lock_timeout(Timeout::FOREVER) {
            Ok(guard) => guard,
            Err(e) => panic!("mutex lock failed: {e}"),
        }
    }

    /// Waits for the mutex for up to `timeout`, see [`Mutex::lock_timeout`].
    pub fn lock_timeout(&self, timeout: impl Into<Timeout>) -> Result<ReentrantMutexGuard<'_, T>> {
        self.raw.lock(timeout.into())?;
        Ok(ReentrantMutexGuard {
            mutex: self,
            _not_send: PhantomData,
        })
    }

    /// Whether the calling thread holds it.
    pub fn is_owner(&self) -> bool {
        self.raw.is_owner()
    }
}

/// Access to the data of a locked [`ReentrantMutex`], which unlocks it once
/// when dropped.
pub struct ReentrantMutexGuard<'a, T: ?Sized> {
    mutex: &'a ReentrantMutex<T>,
    _not_send: PhantomData<*const ()>,
}

impl<T: ?Sized> Deref for ReentrantMutexGuard<'_, T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.mutex.data
    }
}

impl<T: ?Sized> Drop for ReentrantMutexGuard<'_, T> {
    fn drop(&mut self) {
        self.mutex.raw.unlock();
    }
}
