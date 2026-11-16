// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::sync::atomic::{AtomicBool, Ordering};
use core::time::Duration;

use pebbleos::sync::Mutex;
use pebbleos::thread::{self, Priority, Thread, ThreadRef};
use pebbleos::time::Instant;
use pebbleos::{Result, println};

// main() is the high priority thread.
const MEDIUM_PRIO: Priority = Priority::MAIN.lower(1);
const LOW_PRIO: Priority = Priority::MAIN.lower(2);

static BUS: Mutex<()> = Mutex::new(());
static HIGH_WAITS: AtomicBool = AtomicBool::new(false);
static MEDIUM_DONE: AtomicBool = AtomicBool::new(false);

static LOW: Thread = Thread::new();
static MEDIUM: Thread = Thread::new();

fn use_cpu(d: Duration) {
    let start = Instant::now();
    while start.elapsed() < d {
        core::hint::spin_loop();
    }
}

fn low(high: ThreadRef) {
    let me = thread::current();

    let bus = BUS.lock();
    // A long piece of work, which high comes in the middle of.
    while !HIGH_WAITS.load(Ordering::Relaxed) {
        core::hint::spin_loop();
    }
    use_cpu(Duration::from_millis(10));
    // While high waits for the lock, its holder runs at high's priority.
    println!(
        "low: ends its work at the priority of high: {}",
        yes_no(me.priority() == high.priority())
    );
    drop(bus);

    println!(
        "low: back to its own priority: {}",
        yes_no(me.priority() == me.base_priority())
    );
}

// Needs no lock, only the CPU, and for longer than high can wait.
fn medium() {
    use_cpu(Duration::from_millis(100));
    MEDIUM_DONE.store(true, Ordering::Relaxed);
    println!("medium: done");
}

#[pebbleos::main]
fn main() -> Result<()> {
    let high = thread::current();

    LOW.spawn(c"low", LOW_PRIO, move || low(high))?;
    // Lets low take the lock.
    thread::sleep(Duration::from_millis(5));

    // Ready to run, and above low: without inheritance it would run first,
    // for 100 ms.
    MEDIUM.spawn(c"medium", MEDIUM_PRIO, medium)?;

    HIGH_WAITS.store(true, Ordering::Relaxed);
    let bus = BUS.lock();
    println!(
        "high: got the lock {} medium had the CPU",
        if MEDIUM_DONE.load(Ordering::Relaxed) {
            "after"
        } else {
            "before"
        }
    );
    drop(bus);

    LOW.join(Duration::from_secs(1))?;

    println!("Sample done");

    Ok(())
}

fn yes_no(b: bool) -> &'static str {
    if b { "yes" } else { "no" }
}
