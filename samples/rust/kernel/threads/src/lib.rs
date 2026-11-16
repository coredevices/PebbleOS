// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::sync::atomic::{AtomicU32, Ordering};
use core::time::Duration;

use pebbleos::thread::{self, Priority, Thread};
use pebbleos::{Result, println};

const ROUNDS: u32 = 3;

// main() runs at Priority::MAIN: one thread above it, two below.
const URGENT_PRIO: Priority = Priority::MAIN.raise(1);
const PEER_PRIO: Priority = Priority::MAIN.lower(1);

static URGENT: Thread = Thread::new();
static PING: Thread = Thread::new();
static PONG: Thread = Thread::new();

fn peer(rounds: &AtomicU32) {
    let me = thread::current();

    while rounds.load(Ordering::Relaxed) < ROUNDS {
        println!("{}: round {}", me.name(), rounds.load(Ordering::Relaxed));
        rounds.fetch_add(1, Ordering::Relaxed);
        // Threads of equal priority also take turns on every tick; yielding
        // does it now.
        thread::yield_now();
    }
}

#[pebbleos::main]
fn main() -> Result<()> {
    static PING_ROUNDS: AtomicU32 = AtomicU32::new(0);
    static PONG_ROUNDS: AtomicU32 = AtomicU32::new(0);

    println!("main: creating a thread of higher priority");
    URGENT.spawn(c"urgent", URGENT_PRIO, || {
        println!("urgent: preempted main, which is still inside spawn()");
    })?;
    println!(
        "main: urgent has {}",
        if URGENT.is_finished() {
            "ended"
        } else {
            "not ended"
        }
    );

    PING.spawn(c"ping", PEER_PRIO, || peer(&PING_ROUNDS))?;
    PONG.spawn(c"pong", PEER_PRIO, || peer(&PONG_ROUNDS))?;
    println!("main: created two threads of lower priority, which wait for main to block");

    PING.join(Duration::from_secs(1))?;
    PONG.join(Duration::from_secs(1))?;

    println!(
        "main: ping ran {} rounds, pong ran {} rounds",
        PING_ROUNDS.load(Ordering::Relaxed),
        PONG_ROUNDS.load(Ordering::Relaxed)
    );
    println!("Sample done");

    Ok(())
}
