// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

use core::fmt;

/// Text formatted on the stack, cut at its capacity.
pub(crate) struct Buffer<const N: usize> {
    data: [u8; N],
    len: usize,
}

impl<const N: usize> Buffer<N> {
    pub(crate) const fn new() -> Self {
        Self {
            data: [0; N],
            len: 0,
        }
    }

    pub(crate) fn as_bytes(&self) -> &[u8] {
        &self.data[..self.len]
    }

    pub(crate) fn is_full(&self) -> bool {
        self.len == N
    }

    pub(crate) fn clear(&mut self) {
        self.len = 0;
    }

    /// Appends what fits of `bytes`, returning how much did.
    pub(crate) fn push(&mut self, bytes: &[u8]) -> usize {
        let n = bytes.len().min(N - self.len);
        self.data[self.len..self.len + n].copy_from_slice(&bytes[..n]);
        self.len += n;
        n
    }
}

impl<const N: usize> fmt::Write for Buffer<N> {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        self.push(s.as_bytes());
        Ok(())
    }
}
