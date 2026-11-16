// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

use core::ffi::c_int;
use core::fmt::{self, Write};

use crate::buffer::Buffer;
use crate::sys;

const CHUNK: usize = sys::CONFIG_KERNEL_PRINT_BUFFER_SIZE as usize - 1;

/// Hands text to pbl_print() in pieces its buffer takes whole.
struct Printer(Buffer<CHUNK>);

impl Printer {
    fn flush(&mut self) {
        let text = self.0.as_bytes();
        if !text.is_empty() {
            // SAFETY: the format reads exactly the length and the pointer.
            unsafe { sys::pbl_print(c"%.*s".as_ptr(), text.len() as c_int, text.as_ptr()) };
        }
        self.0.clear();
    }
}

impl Write for Printer {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        let mut bytes = s.as_bytes();
        while !bytes.is_empty() {
            if self.0.is_full() {
                self.flush();
            }
            bytes = &bytes[self.0.push(bytes)..];
        }
        Ok(())
    }
}

pub fn _print(args: fmt::Arguments) {
    let mut printer = Printer(Buffer::new());
    let _ = printer.write_fmt(args);
    printer.flush();
}

/// Prints to the console with pbl_print(), from any context.
#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => {
        $crate::print::_print(format_args!($($arg)*))
    };
}

/// Prints to the console with pbl_print(), from any context, with a newline.
#[macro_export]
macro_rules! println {
    () => {
        $crate::print!("\n")
    };
    ($($arg:tt)*) => {
        $crate::print::_print(format_args!("{}\n", format_args!($($arg)*)))
    };
}
