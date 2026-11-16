// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::time::Duration;

use pebbleos::sync::{Mutex, Semaphore};
use pebbleos::thread::{self, Priority, Thread};
use pebbleos::time::Timeout;
use pebbleos::timer::Timer;
use pebbleos::{Errno, Result, println};

const NUM_WORKERS: usize = 4;
const NUM_CHANNELS: u32 = 2;

// Binary: an interrupt tells a thread that what it waits for has happened.
static TRANSFER_DONE: Semaphore = Semaphore::new(0, 1);

// Counting: as many tokens as there are channels to share.
static CHANNELS: Semaphore = Semaphore::new(NUM_CHANNELS, NUM_CHANNELS);

struct Stats {
    busy: u32,
    max_busy: u32,
}

static STATS: Mutex<Stats> = Mutex::new(Stats {
    busy: 0,
    max_busy: 0,
});

static WORKERS: [Thread; NUM_WORKERS] = [const { Thread::new() }; NUM_WORKERS];

// Runs in the tick interrupt, standing for the one of a peripheral.
static TRANSFER_TIMER: Timer = Timer::new(|_| TRANSFER_DONE.give(), ());

fn transfer(duration: Duration) -> Result<()> {
    TRANSFER_TIMER.start(duration);

    if TRANSFER_DONE.take(Duration::from_millis(100)).is_err() {
        TRANSFER_TIMER.stop();
        return Err(Errno::ETIMEDOUT);
    }

    Ok(())
}

fn worker() {
    let _ = CHANNELS.take(Timeout::FOREVER);

    {
        let mut stats = STATS.lock();
        stats.busy += 1;
        stats.max_busy = stats.max_busy.max(stats.busy);
    }

    thread::sleep(Duration::from_millis(20));

    STATS.lock().busy -= 1;

    CHANNELS.give();
}

#[pebbleos::main]
fn main() -> Result<()> {
    let ret = transfer(Duration::from_millis(10));
    println!("main: 10 ms transfer returns {ret:?}");

    let ret = transfer(Duration::from_secs(5));
    println!("main: 5 s transfer returns {ret:?}");

    for worker_thread in &WORKERS {
        worker_thread.spawn(c"worker", Priority::MAIN.lower(1), worker)?;
    }
    for worker_thread in &WORKERS {
        worker_thread.join(Duration::from_secs(1))?;
    }

    println!(
        "main: {NUM_WORKERS} workers shared {NUM_CHANNELS} channels, at most {} busy at once",
        STATS.lock().max_busy
    );
    println!("main: {} channels free again", CHANNELS.count());

    println!("Sample done");

    Ok(())
}
