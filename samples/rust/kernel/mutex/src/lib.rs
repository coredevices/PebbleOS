// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::cell::Cell;
use core::sync::atomic::{AtomicU32, Ordering};
use core::time::Duration;

use pebbleos::sync::{Mutex, ReentrantMutex, Semaphore};
use pebbleos::thread::{self, Priority, Thread};
use pebbleos::time::Timeout;
use pebbleos::{Result, println};

const DEPOSITS: u32 = 100;
const WORKER_PRIO: Priority = Priority::MAIN.lower(1);

// Atomic, so each access alone is safe, but a read and a write are two of
// them, which another thread can come in between.
static UNLOCKED_BALANCE: AtomicU32 = AtomicU32::new(0);
// The data is only reached through the lock.
static BALANCE: Mutex<u32> = Mutex::new(0);
// Its owner may lock it again, e.g. in a function called with it held.
static LEDGER: ReentrantMutex<Cell<u32>> = ReentrantMutex::new(Cell::new(0));

static HOLDER_RELEASE: Semaphore = Semaphore::new(0, 1);

static THREAD_A: Thread = Thread::new();
static THREAD_B: Thread = Thread::new();

fn deposit_unlocked() {
    let balance = UNLOCKED_BALANCE.load(Ordering::Relaxed);
    // Stands for being preempted between the read and the write.
    thread::yield_now();
    UNLOCKED_BALANCE.store(balance + 1, Ordering::Relaxed);
}

fn deposit() {
    let mut balance = BALANCE.lock();
    let before = *balance;
    thread::yield_now();
    *balance = before + 1;
}

fn run_depositors(f: fn()) -> Result<()> {
    let depositor = move || {
        for _ in 0..DEPOSITS {
            f();
        }
    };

    THREAD_A.spawn(c"a", WORKER_PRIO, depositor)?;
    THREAD_B.spawn(c"b", WORKER_PRIO, depositor)?;
    THREAD_A.join(Duration::from_secs(1))?;
    THREAD_B.join(Duration::from_secs(1))
}

#[pebbleos::main]
fn main() -> Result<()> {
    run_depositors(deposit_unlocked)?;
    println!(
        "main: without the lock, balance is {} after {} deposits",
        UNLOCKED_BALANCE.load(Ordering::Relaxed),
        2 * DEPOSITS
    );

    run_depositors(deposit)?;
    println!(
        "main: with the lock, balance is {} after {} deposits",
        *BALANCE.lock(),
        2 * DEPOSITS
    );

    // A lock held elsewhere for long is better waited for with a limit.
    THREAD_A.spawn(c"holder", WORKER_PRIO, || {
        let _balance = BALANCE.lock();
        let _ = HOLDER_RELEASE.take(Timeout::FOREVER);
    })?;
    thread::sleep(Duration::from_millis(10));

    let ret = BALANCE.lock_timeout(Duration::from_millis(10)).map(drop);
    println!("main: lock held by another thread, 10 ms wait returns {ret:?}");
    HOLDER_RELEASE.give();
    drop(BALANCE.lock());

    let outer = LEDGER.lock();
    let inner = LEDGER.lock();
    inner.set(inner.get() + 1);
    drop(inner);
    println!(
        "main: locked twice, unlocked once, still the owner: {}",
        if LEDGER.is_owner() { "yes" } else { "no" }
    );
    drop(outer);

    println!("Sample done");

    Ok(())
}
