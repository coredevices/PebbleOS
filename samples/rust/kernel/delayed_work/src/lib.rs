// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::sync::atomic::{AtomicU32, Ordering};
use core::time::Duration;

use pebbleos::println;
use pebbleos::thread;
use pebbleos::work::{DelayedWork, WorkQueue};

const DEBOUNCE: Duration = Duration::from_millis(200);
const HEARTBEAT: Duration = Duration::from_millis(20);

static DEBOUNCE_WORK: DelayedWork<AtomicU32> = DelayedWork::new(button_settled, AtomicU32::new(0));
static HEARTBEAT_WORK: DelayedWork<AtomicU32> = DelayedWork::new(heartbeat, AtomicU32::new(0));

fn button_settled(presses: &'static DelayedWork<AtomicU32>) {
    let presses = presses.fetch_add(1, Ordering::Relaxed) + 1;
    println!("button: settled, press {presses}");
}

// Unlike a timer function this runs on a thread, and may block or take locks.
fn heartbeat(beats: &'static DelayedWork<AtomicU32>) {
    beats.fetch_add(1, Ordering::Relaxed);
    // Counts from the last expiry, not from now, so the period does not drift.
    beats.schedule_next(HEARTBEAT);
}

// What a button interrupt would do on every edge, bounces included.
fn button_edge() {
    DEBOUNCE_WORK.reschedule(WorkQueue::system(), DEBOUNCE);
}

#[pebbleos::main]
fn main() {
    let system = WorkQueue::system();

    println!("main: five edges 5 ms apart, each pushing the work back");
    for _ in 0..5 {
        button_edge();
        thread::sleep(Duration::from_millis(5));
    }
    thread::sleep(2 * DEBOUNCE);

    // schedule() keeps a delay that is running, where reschedule() replaces
    // it.
    DEBOUNCE_WORK.schedule(system, DEBOUNCE);
    let kept = !DEBOUNCE_WORK.schedule(system, Duration::from_secs(10));
    println!("main: scheduled twice, first delay kept: {}", yes_no(kept));
    thread::sleep(2 * DEBOUNCE);

    HEARTBEAT_WORK.schedule(system, HEARTBEAT);
    thread::sleep(5 * HEARTBEAT + HEARTBEAT / 2);

    // Stops it for good, even though its function schedules it again.
    HEARTBEAT_WORK.cancel_sync();
    println!(
        "main: heartbeat stopped after {} beats, pending: {}",
        HEARTBEAT_WORK.load(Ordering::Relaxed),
        yes_no(HEARTBEAT_WORK.is_pending())
    );

    println!("Sample done");
}

fn yes_no(b: bool) -> &'static str {
    if b { "yes" } else { "no" }
}
