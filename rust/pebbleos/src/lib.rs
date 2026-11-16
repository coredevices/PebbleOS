// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

//! PebbleOS API for Rust.
//!
//! An application is a `no_std` static library whose entry is marked with
//! [`#[main]`](main):
//!
//! ```ignore
//! #![no_std]
//!
//! use pebbleos::println;
//!
//! #[pebbleos::main]
//! fn main() {
//!     println!("Hello World! {}", pebbleos::kconfig::BOARD);
//! }
//! ```

#![no_std]

pub use pebbleos_macros::main;
pub use pebbleos_sys as sys;

pub mod errno;
pub mod heap;
pub mod kconfig;
pub mod msgq;
pub mod poll;
#[doc(hidden)]
pub mod print;
pub mod sync;
pub mod thread;
pub mod time;
pub mod timer;
pub mod work;

mod buffer;
mod irq;
mod panic;

pub use errno::{Errno, Result};

/// What `main()` returns: nothing, or a [`Result`] whose error is printed and
/// returned as a negative error number.
pub trait Termination {
    /// The value `main()` returns to the kernel.
    fn report(self) -> core::ffi::c_int;
}

impl Termination for () {
    fn report(self) -> core::ffi::c_int {
        0
    }
}

impl Termination for Result<()> {
    fn report(self) -> core::ffi::c_int {
        match self {
            Ok(()) => 0,
            Err(e) => {
                println!("main: failed with {e}");
                -e.code()
            }
        }
    }
}
