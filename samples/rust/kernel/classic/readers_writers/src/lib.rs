// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::sync::atomic::{AtomicU32, Ordering};
use core::time::Duration;

use pebbleos::sync::{Mutex, Semaphore};
use pebbleos::thread::{self, Priority, Thread};
use pebbleos::time::Timeout;
use pebbleos::{Result, println};

const NUM_READERS: usize = 3;
const READS: u32 = 10;
const WRITES: u32 = 5;

/// Any number of readers, or one writer. The room is a semaphore and not a
/// mutex because the reader that leaves it last is not always the one that
/// entered first.
struct RwLock {
    readers: Mutex<u32>,
    room: Semaphore,
}

struct ReadGuard<'a>(&'a RwLock);
struct WriteGuard<'a>(&'a RwLock);

impl RwLock {
    const fn new() -> RwLock {
        RwLock {
            readers: Mutex::new(0),
            room: Semaphore::new(1, 1),
        }
    }

    fn read(&self) -> ReadGuard<'_> {
        let mut readers = self.readers.lock();
        *readers += 1;
        if *readers == 1 {
            // The first reader in takes the room for all of them.
            let _ = self.room.take(Timeout::FOREVER);
        }
        ReadGuard(self)
    }

    fn write(&self) -> WriteGuard<'_> {
        let _ = self.room.take(Timeout::FOREVER);
        WriteGuard(self)
    }
}

impl Drop for ReadGuard<'_> {
    fn drop(&mut self) {
        let mut readers = self.0.readers.lock();
        *readers -= 1;
        if *readers == 0 {
            self.0.room.give();
        }
    }
}

impl Drop for WriteGuard<'_> {
    fn drop(&mut self) {
        self.0.room.give();
    }
}

/// Written in two steps, which no reader may see half done.
struct Record {
    version: AtomicU32,
    copy_of_version: AtomicU32,
}

struct Stats {
    reading: u32,
    max_reading: u32,
    torn_reads: u32,
    writes_with_readers: u32,
}

static LOCK: RwLock = RwLock::new();
static RECORD: Record = Record {
    version: AtomicU32::new(0),
    copy_of_version: AtomicU32::new(0),
};
// Readers share the room, so what they count needs a lock of its own.
static STATS: Mutex<Stats> = Mutex::new(Stats {
    reading: 0,
    max_reading: 0,
    torn_reads: 0,
    writes_with_readers: 0,
});

static READERS: [Thread; NUM_READERS] = [const { Thread::new() }; NUM_READERS];
static WRITER: Thread = Thread::new();

fn reader() {
    for _ in 0..READS {
        let guard = LOCK.read();

        {
            let mut stats = STATS.lock();
            stats.reading += 1;
            stats.max_reading = stats.max_reading.max(stats.reading);
        }

        let version = RECORD.version.load(Ordering::Relaxed);
        thread::sleep(Duration::from_millis(2));
        let torn = version != RECORD.copy_of_version.load(Ordering::Relaxed);

        {
            let mut stats = STATS.lock();
            stats.torn_reads += torn as u32;
            stats.reading -= 1;
        }

        drop(guard);

        // Out of the room for a while, or a writer would never find it empty.
        thread::sleep(Duration::from_millis(3));
    }
}

fn writer() {
    for _ in 0..WRITES {
        let guard = LOCK.write();

        {
            let mut stats = STATS.lock();
            if stats.reading != 0 {
                stats.writes_with_readers += 1;
            }
        }

        let version = RECORD.version.fetch_add(1, Ordering::Relaxed) + 1;
        thread::sleep(Duration::from_millis(2));
        RECORD.copy_of_version.store(version, Ordering::Relaxed);

        drop(guard);
        thread::sleep(Duration::from_millis(5));
    }
}

#[pebbleos::main]
fn main() -> Result<()> {
    let prio = Priority::MAIN.lower(1);

    for t in &READERS {
        t.spawn(c"reader", prio, reader)?;
    }
    WRITER.spawn(c"writer", prio, writer)?;

    for t in READERS.iter().chain([&WRITER]) {
        t.join(Duration::from_secs(2))?;
    }

    let stats = STATS.lock();
    println!(
        "record: version {} after {WRITES} writes",
        RECORD.version.load(Ordering::Relaxed)
    );
    println!(
        "record: up to {} readers at once, {} half written records read",
        stats.max_reading, stats.torn_reads
    );
    println!(
        "record: {} writes made with a reader in the room",
        stats.writes_with_readers
    );

    println!("Sample done");

    Ok(())
}
