// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

//! Errors, as the C API reports them.

use core::ffi::c_int;
use core::fmt;

use crate::sys;

/// An error number, as C functions return it negated.
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct Errno(c_int);

/// The result of a fallible call.
pub type Result<T> = core::result::Result<T, Errno>;

macro_rules! errnos {
    ($($name:ident),* $(,)?) => {
        impl Errno {
            $(pub const $name: Errno = Errno(sys::$name as c_int);)*

            fn name(self) -> Option<&'static str> {
                match self {
                    $(Errno::$name => Some(stringify!($name)),)*
                    _ => None,
                }
            }
        }
    };
}

errnos!(
    EAGAIN, EBUSY, EFAULT, EINVAL, EIO, ENODEV, ENOENT, ENOMEM, ENOSPC, ENOTSUP, EPERM, ETIMEDOUT,
);

impl Errno {
    /// The error of a positive error number.
    pub const fn new(code: c_int) -> Errno {
        Errno(code)
    }

    /// The positive error number.
    pub const fn code(self) -> c_int {
        self.0
    }

    /// The result of a C function that returns 0 or a negative error number.
    pub fn result(rc: c_int) -> Result<()> {
        if rc < 0 { Err(Errno(-rc)) } else { Ok(()) }
    }
}

impl fmt::Debug for Errno {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.name() {
            Some(name) => f.write_str(name),
            None => write!(f, "Errno({})", self.0),
        }
    }
}

impl fmt::Display for Errno {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(self, f)
    }
}
