// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::sync::atomic::{AtomicU32, Ordering};
use core::time::Duration;

use pebbleos::sync::Semaphore;
use pebbleos::thread;
use pebbleos::time::Instant;
use pebbleos::timer::Timer;
use pebbleos::{Result, println};

const BLINKS: u32 = 5;
const BLINK_PERIOD: Duration = Duration::from_millis(20);

// What the timer acts on, which its function gets through it.
struct Blinker {
    done: Semaphore,
    blinks: AtomicU32,
    last_ms: AtomicU32,
}

static BLINKER: Timer<Blinker> = Timer::new(
    blink,
    Blinker {
        done: Semaphore::new(0, 1),
        blinks: AtomicU32::new(0),
        last_ms: AtomicU32::new(0),
    },
);

// Runs in the tick interrupt: keep it short, and never block.
fn blink(timer: &'static Timer<Blinker>) {
    let blinks = timer.blinks.fetch_add(1, Ordering::Relaxed) + 1;
    timer
        .last_ms
        .store(Instant::now().as_millis() as u32, Ordering::Relaxed);

    // Timers are one-shot: one that repeats starts itself again.
    if blinks < BLINKS {
        timer.start(BLINK_PERIOD);
    } else {
        timer.done.give();
    }
}

#[pebbleos::main]
fn main() -> Result<()> {
    let start_ms = Instant::now().as_millis() as u32;

    BLINKER.start(BLINK_PERIOD);
    BLINKER.done.take(Duration::from_secs(1))?;

    let elapsed = BLINKER.last_ms.load(Ordering::Relaxed) - start_ms;
    println!(
        "main: {} blinks, the last one {} {} ms after the start",
        BLINKER.blinks.load(Ordering::Relaxed),
        if elapsed >= BLINKS * 20 {
            "at least"
        } else {
            "less than"
        },
        BLINKS * 20
    );

    BLINKER.blinks.store(0, Ordering::Relaxed);
    BLINKER.start(Duration::from_secs(10));
    thread::sleep(Duration::from_millis(10));
    println!(
        "main: started for 10 s, active: {}, more than 5 s left: {}",
        yes_no(BLINKER.is_active()),
        yes_no(BLINKER.remaining() > Duration::from_secs(5))
    );

    BLINKER.stop();
    println!("main: stopped, active: {}", yes_no(BLINKER.is_active()));
    thread::sleep(Duration::from_millis(20));
    println!(
        "main: stopped before it expired, {} blinks since",
        BLINKER.blinks.load(Ordering::Relaxed)
    );

    println!("Sample done");

    Ok(())
}

fn yes_no(b: bool) -> &'static str {
    if b { "yes" } else { "no" }
}
