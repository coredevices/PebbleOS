// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::sync::atomic::{AtomicBool, AtomicU32, Ordering};
use core::time::Duration;

use pebbleos::sync::{Mutex, Semaphore};
use pebbleos::thread::{self, Priority, Thread};
use pebbleos::time::Timeout;
use pebbleos::{Result, println};

const NUM_PHILOSOPHERS: usize = 5;
const MEALS: u32 = 3;

struct Philosopher {
    thread: Thread,
    meals: AtomicU32,
    stuck: AtomicBool,
}

static FORKS: [Mutex<()>; NUM_PHILOSOPHERS] = [const { Mutex::new(()) }; NUM_PHILOSOPHERS];
static PHILOSOPHERS: [Philosopher; NUM_PHILOSOPHERS] = [const {
    Philosopher {
        thread: Thread::new(),
        meals: AtomicU32::new(0),
        stuck: AtomicBool::new(false),
    }
}; NUM_PHILOSOPHERS];
static HOLDING_ONE: AtomicU32 = AtomicU32::new(0);

// Stuck philosophers keep their fork until main has counted them.
static GIVE_UP: Semaphore = Semaphore::new(0, NUM_PHILOSOPHERS as u32);

fn dine(seat: usize, ordered: bool) {
    let me = &PHILOSOPHERS[seat];
    let mut first = seat;
    let mut second = (seat + 1) % NUM_PHILOSOPHERS;

    // The fix: everyone takes the lower numbered fork first, so no cycle of
    // waiters can form.
    if ordered && first > second {
        core::mem::swap(&mut first, &mut second);
    }

    while me.meals.load(Ordering::Relaxed) < MEALS {
        let first_fork = FORKS[first].lock();
        // A deadlock needs bad luck with the timing, which is staged here:
        // nobody goes on until every philosopher holds a fork. With the fix
        // they cannot all hold one.
        HOLDING_ONE.fetch_add(1, Ordering::Relaxed);
        loop {
            thread::yield_now();
            if ordered || HOLDING_ONE.load(Ordering::Relaxed) as usize >= NUM_PHILOSOPHERS {
                break;
            }
        }

        // The time limit stands in for the deadlock, which would otherwise
        // end the sample here.
        let Ok(second_fork) = FORKS[second].lock_timeout(Duration::from_millis(50)) else {
            me.stuck.store(true, Ordering::Relaxed);
            let _ = GIVE_UP.take(Timeout::FOREVER);
            return;
        };

        me.meals.fetch_add(1, Ordering::Relaxed);
        thread::sleep(Duration::from_millis(2));

        HOLDING_ONE.fetch_sub(1, Ordering::Relaxed);
        drop(second_fork);
        drop(first_fork);
    }
}

fn dinner(ordered: bool) -> Result<()> {
    HOLDING_ONE.store(0, Ordering::Relaxed);

    for (seat, p) in PHILOSOPHERS.iter().enumerate() {
        p.meals.store(0, Ordering::Relaxed);
        p.stuck.store(false, Ordering::Relaxed);
        p.thread
            .spawn(c"philosopher", Priority::MAIN.lower(1), move || {
                dine(seat, ordered)
            })?;
    }

    // Long enough for every meal, or for every time limit to pass.
    thread::sleep(Duration::from_millis(200));

    let mut meals = 0;
    let mut stuck = 0;
    for p in &PHILOSOPHERS {
        meals += p.meals.load(Ordering::Relaxed);
        stuck += p.stuck.load(Ordering::Relaxed) as u32;
        GIVE_UP.give();
    }
    for p in &PHILOSOPHERS {
        p.thread.join(Duration::from_secs(1))?;
    }
    GIVE_UP.reset();

    println!(
        "dinner: {}: {meals} meals eaten, {stuck} of {NUM_PHILOSOPHERS} philosophers stuck with one fork",
        if ordered {
            "lower fork first"
        } else {
            "left fork first"
        }
    );

    Ok(())
}

#[pebbleos::main]
fn main() -> Result<()> {
    dinner(false)?;
    dinner(true)?;

    println!("Sample done");

    Ok(())
}
