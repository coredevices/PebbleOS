// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::time::Duration;

use pebbleos::sync::{Mutex, Semaphore};
use pebbleos::thread::{self, Priority, Thread};
use pebbleos::time::Timeout;
use pebbleos::{Result, println};

const NUM_WORKERS: usize = 3;

/// A point no thread passes until all of them have reached it.
struct Barrier {
    waiting: Mutex<usize>,
    gate: Semaphore,
    parties: usize,
}

impl Barrier {
    const fn new(parties: usize) -> Barrier {
        Barrier {
            waiting: Mutex::new(0),
            gate: Semaphore::new(0, parties as u32),
            parties,
        }
    }

    fn wait(&self) {
        let mut waiting = self.waiting.lock();
        *waiting += 1;
        if *waiting == self.parties {
            // The last one in lets the others through, and leaves the barrier
            // ready for another round.
            for _ in 1..self.parties {
                self.gate.give();
            }
            *waiting = 0;
            return;
        }
        drop(waiting);

        let _ = self.gate.take(Timeout::FOREVER);
    }
}

struct Stats {
    ready: usize,
    passed: usize,
    passed_early: usize,
}

static START_LINE: Barrier = Barrier::new(NUM_WORKERS);
static STATS: Mutex<Stats> = Mutex::new(Stats {
    ready: 0,
    passed: 0,
    passed_early: 0,
});
static WORKERS: [Thread; NUM_WORKERS] = [const { Thread::new() }; NUM_WORKERS];

fn worker(id: usize) {
    // Getting ready takes each worker a different time.
    thread::sleep(Duration::from_millis(20 * (id as u64 + 1)));
    STATS.lock().ready += 1;
    println!("worker {id}: ready");

    START_LINE.wait();

    let mut stats = STATS.lock();
    if stats.ready != NUM_WORKERS {
        stats.passed_early += 1;
    }
    stats.passed += 1;
}

#[pebbleos::main]
fn main() -> Result<()> {
    for (id, t) in WORKERS.iter().enumerate() {
        t.spawn(c"worker", Priority::MAIN.lower(1), move || worker(id))?;
    }
    for t in &WORKERS {
        t.join(Duration::from_secs(1))?;
    }

    let stats = STATS.lock();
    println!(
        "main: {} past the barrier, {} of them before the last was ready",
        stats.passed, stats.passed_early
    );

    println!("Sample done");

    Ok(())
}
