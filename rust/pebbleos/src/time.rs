// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

//! Uptime, wall clock and time limits.

use core::ops::{Add, Sub};
use core::time::Duration;

use crate::errno::{Errno, Result};
use crate::sys;
use crate::thread;

/// How long a call may block.
#[derive(Clone, Copy, Debug)]
pub struct Timeout(pub(crate) sys::pbl_timeout_t);

impl Timeout {
    /// Do not block.
    pub const NO_WAIT: Timeout = Timeout(sys::pbl_timeout_t { ticks: 0 });
    /// Block for as long as it takes.
    pub const FOREVER: Timeout = Timeout(sys::pbl_timeout_t {
        ticks: sys::pbl_tick_t::MAX,
    });

    /// A limit in kernel ticks.
    pub const fn from_ticks(ticks: u32) -> Timeout {
        Timeout(sys::pbl_timeout_t { ticks })
    }

    /// The limit in kernel ticks.
    pub const fn ticks(self) -> u32 {
        self.0.ticks
    }
}

impl PartialEq for Timeout {
    fn eq(&self, other: &Timeout) -> bool {
        self.ticks() == other.ticks()
    }
}

impl Eq for Timeout {}

impl From<Duration> for Timeout {
    fn from(d: Duration) -> Timeout {
        // SAFETY: a pure conversion.
        Timeout::from_ticks(unsafe { sys::pbl_ms_to_ticks(millis(d)) })
    }
}

/// `d` in milliseconds, rounded up and saturated.
pub(crate) fn millis(d: Duration) -> u32 {
    u32::try_from(d.as_micros().div_ceil(1000)).unwrap_or(u32::MAX)
}

/// A duration of kernel ticks.
pub fn ticks_to_duration(ticks: u32) -> Duration {
    // SAFETY: a pure conversion.
    Duration::from_millis(unsafe { sys::pbl_ticks_to_ms(ticks) }.into())
}

/// A point in the uptime, which only moves forward: for measuring, and for
/// time limits.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct Instant(u64);

impl Instant {
    /// Now.
    pub fn now() -> Instant {
        // SAFETY: reads the uptime.
        Instant(unsafe { sys::pbl_uptime_ms() })
    }

    /// Time since `earlier`, zero if it is later.
    pub fn duration_since(self, earlier: Instant) -> Duration {
        Duration::from_millis(self.0.saturating_sub(earlier.0))
    }

    /// Time since this instant.
    pub fn elapsed(self) -> Duration {
        Instant::now().duration_since(self)
    }

    /// Milliseconds since boot.
    pub const fn as_millis(self) -> u64 {
        self.0
    }
}

impl Add<Duration> for Instant {
    type Output = Instant;

    fn add(self, d: Duration) -> Instant {
        Instant(self.0 + d.as_millis() as u64)
    }
}

impl Sub<Instant> for Instant {
    type Output = Duration;

    fn sub(self, earlier: Instant) -> Duration {
        self.duration_since(earlier)
    }
}

/// Seconds since the epoch on the wall clock.
// time_t is narrower on some C libraries.
#[allow(clippy::useless_conversion)]
pub fn realtime() -> i64 {
    // SAFETY: reads the wall clock.
    unsafe { sys::pbl_realtime() }.into()
}

/// Sets the wall clock, in seconds since the epoch.
pub fn set_realtime(t: i64) {
    // SAFETY: sets the wall clock.
    unsafe { sys::pbl_realtime_set(t as sys::time_t) }
}

/// Waits for `cond` to hold, checking it every `period`, for up to `timeout`.
///
/// Fails with [`Errno::ETIMEDOUT`] if it still does not hold once the time
/// is up.
pub fn poll(period: Duration, timeout: Duration, mut cond: impl FnMut() -> bool) -> Result<()> {
    let start = Instant::now();
    loop {
        let expired = start.elapsed() > timeout;
        if cond() {
            return Ok(());
        }
        if expired {
            return Err(Errno::ETIMEDOUT);
        }
        thread::sleep(period);
    }
}
