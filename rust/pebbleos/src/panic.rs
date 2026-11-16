// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

use core::ffi::{c_char, c_int};
use core::fmt::Write;
use core::panic::PanicInfo;

use crate::buffer::Buffer;
use crate::sys;

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    let mut message = Buffer::<128>::new();
    let _ = write!(message, "{}", info.message());
    let message = message.as_bytes();

    let (file, line) = info.location().map_or(("?", 0), |l| (l.file(), l.line()));
    // The file's name alone, as the C code logs it.
    let name = file.rsplit(['/', '\\']).next().unwrap_or(file);
    let mut file = Buffer::<32>::new();
    file.push(&name.as_bytes()[..name.len().min(31)]);
    file.push(b"\0");

    // SAFETY: the file name is NUL-terminated and the format reads exactly
    // the length and the pointer.
    unsafe {
        sys::pbl_panic_at(
            file.as_bytes().as_ptr() as *const c_char,
            line as c_int,
            c"%.*s".as_ptr(),
            message.len() as c_int,
            message.as_ptr(),
        )
    }
}

// Referenced by the host's prebuilt core, which unwinds; never called, as
// crates abort on panic.
#[cfg(not(target_os = "none"))]
#[unsafe(no_mangle)]
extern "C" fn rust_eh_personality() {}
