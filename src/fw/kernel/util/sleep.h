/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! @internal
//!
//! Waits for a certain amount of milliseconds by suspending the thread in firmware or by just busy
//! waiting in the bootloader.
//!
//! Note that the thread is slept until the current tick + millis of ticks, so that means that if
//! we're currently part way through a tick, we'll actually wait for a time that's less than
//! expected. For example, if we're at tick n and we want to wait for 1 millisecond, we'll sleep
//! the thread until tick n+1, which will result in a sleep of less than a millisecond, as we're
//! probably halfway through the n-th tick at this time. Also note that your thread isn't
//! guaranteed to be scheduled immediately after you're done sleeping, so you may sleep for longer
//! than you expect.
//!
//! @param millis The number of milliseconds to wait for
void psleep(int millis);

