// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

//! Threads.
//!
//! A [`Thread`] is a static object that owns its stack, and runs one closure
//! at a time:
//!
//! ```ignore
//! use pebbleos::thread::{self, Priority, Thread};
//!
//! static WORKER: Thread = Thread::new();
//!
//! WORKER.spawn(c"worker", Priority::MAIN.lower(1), || {
//!     println!("hello from {}", thread::current().name());
//! })?;
//! WORKER.join(Duration::from_secs(1))?;
//! ```

use core::cell::UnsafeCell;
use core::ffi::{CStr, c_void};
use core::mem::{self, MaybeUninit};
use core::ptr::{self, NonNull};
use core::sync::atomic::{AtomicBool, Ordering};
use core::time::Duration;

use crate::errno::{Errno, Result};
use crate::sys;
use crate::time;

/// A scheduling priority: the higher, the more urgent.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct Priority(u8);

impl Priority {
    /// The idle thread's, the lowest.
    pub const IDLE: Priority = Priority(0);
    /// The highest.
    pub const MAX: Priority = Priority(sys::CONFIG_KERNEL_NUM_PRIORITIES as u8 - 1);
    /// The one `main()` runs at.
    pub const MAIN: Priority = Priority(sys::CONFIG_KERNEL_MAIN_THREAD_PRIO as u8);

    /// A priority from its number, at most [`Priority::MAX`].
    pub const fn new(prio: u8) -> Priority {
        assert!(prio <= Priority::MAX.0, "priority above the highest");
        Priority(prio)
    }

    /// The priority `n` levels below.
    pub const fn lower(self, n: u8) -> Priority {
        assert!(n <= self.0, "priority below the lowest");
        Priority(self.0 - n)
    }

    /// The priority `n` levels above.
    pub const fn raise(self, n: u8) -> Priority {
        Priority::new(self.0 + n)
    }

    /// Its number.
    pub const fn get(self) -> u8 {
        self.0
    }
}

/// What a thread is doing.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum State {
    /// Runnable, waiting for the CPU.
    Ready,
    /// Running.
    Running,
    /// Waiting on an object, or sleeping.
    Blocked,
    /// Suspended.
    Suspended,
    /// Ended, or never started.
    Dead,
}

impl State {
    fn from_raw(state: sys::pbl_thread_state::Type) -> State {
        use sys::pbl_thread_state::*;
        match state {
            PBL_THREAD_READY => State::Ready,
            PBL_THREAD_RUNNING => State::Running,
            PBL_THREAD_BLOCKED => State::Blocked,
            PBL_THREAD_SUSPENDED => State::Suspended,
            _ => State::Dead,
        }
    }
}

const GUARD: usize = sys::CONFIG_KERNEL_STACK_GUARD_SIZE as usize;
const ALIGN: usize = 128;
const MIN_STACK: usize = 128;
const _: () = assert!(ALIGN.is_multiple_of(sys::CONFIG_KERNEL_STACK_ALIGN as usize));

/// A stack of `N` usable bytes above its MPU guard.
#[repr(C, align(128))]
struct Stack<const N: usize> {
    guard: [u8; GUARD],
    data: [u8; N],
}

/// A thread with a stack of `STACK` bytes.
///
/// It runs one closure at a time, and can be spawned again once that one has
/// returned.
pub struct Thread<const STACK: usize = 1024> {
    stack: UnsafeCell<MaybeUninit<Stack<STACK>>>,
    raw: UnsafeCell<sys::pbl_thread>,
    spawning: AtomicBool,
}

// SAFETY: the kernel serializes access to the thread object, and the stack
// only belongs to the thread that runs on it.
unsafe impl<const STACK: usize> Sync for Thread<STACK> {}

impl<const STACK: usize> Thread<STACK> {
    /// A thread that has not been spawned.
    pub const fn new() -> Self {
        Thread {
            stack: UnsafeCell::new(MaybeUninit::uninit()),
            // SAFETY: an all-zero thread is one never created.
            raw: UnsafeCell::new(unsafe { mem::zeroed() }),
            spawning: AtomicBool::new(false),
        }
    }

    /// Runs `f` on this thread, named `name`, at `prio`.
    ///
    /// The new thread may run before this returns, if `prio` is above the
    /// caller's. Fails with [`Errno::EBUSY`] while the previous closure has
    /// not returned.
    pub fn spawn<F>(&'static self, name: &CStr, prio: Priority, f: F) -> Result<()>
    where
        F: FnOnce() + Send + 'static,
    {
        const {
            assert!(
                mem::size_of::<F>() <= STACK / 2,
                "closure too large for the stack"
            );
            assert!(
                mem::align_of::<F>() <= ALIGN,
                "closure aligned beyond the stack"
            );
            // What pbl_thread_create() requires, once the closure is placed.
            assert!(
                STACK >= MIN_STACK + mem::size_of::<F>() + mem::align_of::<F>() + 8,
                "stack too small for the closure"
            );
        }

        if self.spawning.swap(true, Ordering::Acquire) {
            return Err(Errno::EBUSY);
        }
        let ret = if self.is_finished() || self.id() == 0 {
            // SAFETY: the thread is not running, so its stack is free.
            unsafe { self.create(name, prio, f) }
        } else {
            Err(Errno::EBUSY)
        };
        self.spawning.store(false, Ordering::Release);
        ret
    }

    /// # Safety
    ///
    /// The thread must not be running.
    unsafe fn create<F: FnOnce() + Send + 'static>(
        &'static self,
        name: &CStr,
        prio: Priority,
        f: F,
    ) -> Result<()> {
        // The closure waits for the thread at the top of the stack buffer,
        // out of the stack the kernel is given.
        let base = self.stack.get() as *mut u8;
        let align = mem::align_of::<F>().max(8);
        let offset = (mem::size_of::<Stack<STACK>>() - mem::size_of::<F>()) & !(align - 1);

        // SAFETY: the offset is in the buffer and aligned for F.
        let closure = unsafe { base.add(offset) } as *mut F;
        unsafe { closure.write(f) };

        // SAFETY: an all-zero attribute takes the defaults.
        let mut attr: sys::pbl_thread_attr = unsafe { mem::zeroed() };
        attr.name = name.as_ptr();
        attr.entry = Some(trampoline::<F>);
        attr.arg = closure as *mut c_void;
        attr.prio = prio.0;
        attr.privileged = true;
        attr.stack = base as *mut c_void;
        attr.stack_size = offset;

        // SAFETY: the thread object and the stack are static and unused.
        let ret = Errno::result(unsafe { sys::pbl_thread_create(self.raw.get(), &attr) });
        if ret.is_err() {
            // SAFETY: the thread did not start, so the closure is still there.
            unsafe { ptr::drop_in_place(closure) };
        }
        ret
    }

    fn id(&self) -> u32 {
        // SAFETY: the id only changes in spawn().
        unsafe { (*self.raw.get()).id }
    }

    /// What it is doing.
    pub fn state(&self) -> State {
        if self.id() == 0 {
            return State::Dead;
        }
        // SAFETY: the thread object is valid.
        State::from_raw(unsafe { sys::pbl_thread_state(self.raw.get()) })
    }

    /// Whether the closure has returned, or none was spawned.
    pub fn is_finished(&self) -> bool {
        self.state() == State::Dead
    }

    /// Waits for the closure to return, for up to `timeout`.
    ///
    /// Fails with [`Errno::ETIMEDOUT`] if it is still running.
    pub fn join(&self, timeout: Duration) -> Result<()> {
        time::poll(Duration::from_millis(1), timeout, || self.is_finished())
    }

    /// The thread as [`current()`] returns it.
    pub fn handle(&'static self) -> ThreadRef {
        // SAFETY: never null.
        ThreadRef(unsafe { NonNull::new_unchecked(self.raw.get()) })
    }
}

impl<const STACK: usize> Default for Thread<STACK> {
    fn default() -> Self {
        Self::new()
    }
}

unsafe extern "C" fn trampoline<F: FnOnce()>(arg: *mut c_void) {
    // SAFETY: spawn() left the closure there, and nothing else reads it.
    let f = unsafe { ptr::read(arg as *mut F) };
    f();
}

/// Any thread, including those the C code creates.
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct ThreadRef(NonNull<sys::pbl_thread>);

// SAFETY: the kernel serializes access to the thread object.
unsafe impl Send for ThreadRef {}
unsafe impl Sync for ThreadRef {}

impl ThreadRef {
    pub(crate) fn as_ptr(&self) -> *mut sys::pbl_thread {
        self.0.as_ptr()
    }

    fn created(&self) -> bool {
        // SAFETY: the id only changes when the thread is created.
        unsafe { (*self.0.as_ptr()).id != 0 }
    }

    /// Its name.
    pub fn name(&self) -> &str {
        // SAFETY: the name is a NUL-terminated array.
        let name = unsafe { CStr::from_ptr((*self.0.as_ptr()).name.as_ptr()) };
        name.to_str().unwrap_or("?")
    }

    /// The priority it runs at, raised while it holds a mutex that a more
    /// urgent thread waits for.
    pub fn priority(&self) -> Priority {
        // SAFETY: a byte the kernel updates.
        Priority(unsafe { ptr::read_volatile(&raw const (*self.0.as_ptr()).prio) })
    }

    /// The priority it was given.
    pub fn base_priority(&self) -> Priority {
        // SAFETY: the thread object is valid.
        Priority(unsafe { sys::pbl_thread_prio_get(self.0.as_ptr()) })
    }

    /// Gives it another priority, unless it was never spawned.
    pub fn set_priority(&self, prio: Priority) {
        if !self.created() {
            return;
        }
        // SAFETY: the thread object is valid.
        unsafe { sys::pbl_thread_prio_set(self.0.as_ptr(), prio.0) }
    }

    /// What it is doing.
    pub fn state(&self) -> State {
        if !self.created() {
            return State::Dead;
        }
        // SAFETY: the thread object is valid.
        State::from_raw(unsafe { sys::pbl_thread_state(self.0.as_ptr()) })
    }
}

/// The calling thread.
pub fn current() -> ThreadRef {
    // SAFETY: there is always a current thread.
    ThreadRef(unsafe { NonNull::new_unchecked(sys::pbl_thread_current()) })
}

/// Blocks the calling thread for `d`.
pub fn sleep(d: Duration) {
    // SAFETY: plain sleeps.
    if d < Duration::from_millis(1) {
        unsafe { sys::pbl_sleep_us(d.as_nanos().div_ceil(1000) as u32) }
    } else {
        unsafe { sys::pbl_sleep_ms(time::millis(d)) }
    }
}

/// Lets the other threads of the same priority run.
pub fn yield_now() {
    // SAFETY: a plain yield.
    unsafe { sys::pbl_thread_yield() }
}
