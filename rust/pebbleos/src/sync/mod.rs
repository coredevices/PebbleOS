// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

//! Synchronization between threads, and with interrupts.

mod mutex;
mod semaphore;

pub(crate) use mutex::RawMutex;
pub use mutex::{Mutex, MutexGuard, ReentrantMutex, ReentrantMutexGuard};
pub use semaphore::Semaphore;
