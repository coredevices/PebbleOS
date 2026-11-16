// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::sync::atomic::{AtomicU32, Ordering};
use core::time::Duration;

use pebbleos::sync::{Mutex, Semaphore};
use pebbleos::thread::{self, Priority, Thread};
use pebbleos::time::Timeout;
use pebbleos::{Result, println};

const CAPACITY: usize = 4;
const ITEMS_PER_PRODUCER: u32 = 50;
const NUM_PRODUCERS: usize = 2;
const NUM_CONSUMERS: usize = 2;

struct Ring {
    items: [u32; CAPACITY],
    head: usize,
    tail: usize,
    used: usize,
    max_used: usize,
}

/// What a message queue does inside: one semaphore counts the free slots,
/// another the items, and a mutex guards the ring. Prefer MsgQueue, which
/// also works from interrupts.
struct Buffer {
    ring: Mutex<Ring>,
    free_slots: Semaphore,
    queued_items: Semaphore,
}

impl Buffer {
    fn put(&self, item: u32) {
        // Waits for a free slot before the lock, never with it.
        let _ = self.free_slots.take(Timeout::FOREVER);

        {
            let mut ring = self.ring.lock();
            let tail = ring.tail;
            ring.items[tail] = item;
            ring.tail = (tail + 1) % CAPACITY;
            ring.used += 1;
            ring.max_used = ring.max_used.max(ring.used);
        }

        self.queued_items.give();
    }

    fn get(&self) -> u32 {
        let _ = self.queued_items.take(Timeout::FOREVER);

        let item = {
            let mut ring = self.ring.lock();
            let head = ring.head;
            ring.head = (head + 1) % CAPACITY;
            ring.used -= 1;
            ring.items[head]
        };

        self.free_slots.give();

        item
    }
}

static BUFFER: Buffer = Buffer {
    ring: Mutex::new(Ring {
        items: [0; CAPACITY],
        head: 0,
        tail: 0,
        used: 0,
        max_used: 0,
    }),
    free_slots: Semaphore::new(CAPACITY as u32, CAPACITY as u32),
    queued_items: Semaphore::new(0, CAPACITY as u32),
};

static SUMS: [AtomicU32; NUM_CONSUMERS] = [const { AtomicU32::new(0) }; NUM_CONSUMERS];
static PRODUCERS: [Thread; NUM_PRODUCERS] = [const { Thread::new() }; NUM_PRODUCERS];
static CONSUMERS: [Thread; NUM_CONSUMERS] = [const { Thread::new() }; NUM_CONSUMERS];

fn producer() {
    for item in 1..=ITEMS_PER_PRODUCER {
        BUFFER.put(item);
    }
}

fn consumer(sum: &AtomicU32) {
    for _ in 0..ITEMS_PER_PRODUCER as usize * NUM_PRODUCERS / NUM_CONSUMERS {
        sum.fetch_add(BUFFER.get(), Ordering::Relaxed);
        // Slower than the producers, so the buffer fills up.
        thread::yield_now();
    }
}

#[pebbleos::main]
fn main() -> Result<()> {
    let prio = Priority::MAIN.lower(1);
    let expected = NUM_PRODUCERS as u32 * (ITEMS_PER_PRODUCER * (ITEMS_PER_PRODUCER + 1) / 2);

    for t in &PRODUCERS {
        t.spawn(c"producer", prio, producer)?;
    }
    for (t, sum) in CONSUMERS.iter().zip(&SUMS) {
        t.spawn(c"consumer", prio, move || consumer(sum))?;
    }
    for t in PRODUCERS.iter().chain(&CONSUMERS) {
        t.join(Duration::from_secs(1))?;
    }

    let sum: u32 = SUMS.iter().map(|s| s.load(Ordering::Relaxed)).sum();
    println!(
        "buffer: {} items produced, consumed items add up to {sum} of {expected}",
        NUM_PRODUCERS as u32 * ITEMS_PER_PRODUCER
    );
    let ring = BUFFER.ring.lock();
    println!(
        "buffer: held at most {} of {CAPACITY} items, {} left",
        ring.max_used, ring.used
    );

    println!("Sample done");

    Ok(())
}
