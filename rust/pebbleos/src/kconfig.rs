// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

//! Kconfig values that need converting. Every symbol is in [`crate::sys`]
//! as `CONFIG_*`.

use crate::sys;

const fn c_str(bytes: &'static [u8]) -> &'static str {
    let Some((_, text)) = bytes.split_last() else {
        panic!("not a C string");
    };
    match core::str::from_utf8(text) {
        Ok(text) => text,
        Err(_) => panic!("not UTF-8"),
    }
}

/// Board the firmware is built for, `CONFIG_BOARD`.
pub const BOARD: &str = c_str(sys::CONFIG_BOARD);
