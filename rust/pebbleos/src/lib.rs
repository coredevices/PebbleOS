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

pub mod kconfig;
#[doc(hidden)]
pub mod print;

mod buffer;
mod panic;
