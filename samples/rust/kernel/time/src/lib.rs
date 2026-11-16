// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::time::Duration;

use pebbleos::println;
use pebbleos::thread;
use pebbleos::time::{self, Instant};

// 2026-01-01 00:00:00 UTC
const NEW_YEAR: i64 = 1767225600;

#[pebbleos::main]
fn main() {
    // Uptime only moves forward: use it to measure and to time out.
    let start = Instant::now();
    thread::sleep(Duration::from_millis(50));
    println!(
        "time: slept 50 ms, uptime moved by at least 50 ms: {}",
        yes_no(start.elapsed() >= Duration::from_millis(50))
    );

    // The wall clock follows the uptime, and jumps when it is set.
    time::set_realtime(NEW_YEAR);
    thread::sleep(Duration::from_secs(1));
    println!(
        "time: one second after it was set, the wall clock moved by {} s",
        time::realtime() - NEW_YEAR
    );

    // Waiting for a condition, with the time limit every such wait needs.
    let mut polls = 0;
    let ret = time::poll(Duration::from_millis(5), Duration::from_millis(100), || {
        polls += 1;
        polls >= 3
    });
    println!("time: device ready: {}", yes_no(ret.is_ok()));

    let ret = time::poll(Duration::from_millis(5), Duration::from_millis(20), || {
        false
    });
    println!("time: a condition that never holds returns {ret:?}");

    println!("Sample done");
}

fn yes_no(b: bool) -> &'static str {
    if b { "yes" } else { "no" }
}
