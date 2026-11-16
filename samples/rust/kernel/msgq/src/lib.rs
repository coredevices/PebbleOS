// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::time::Duration;

use pebbleos::msgq::MsgQueue;
use pebbleos::thread::{self, Priority, Thread};
use pebbleos::time::Timeout;
use pebbleos::{Errno, Result, println};

const NUM_SAMPLES: u32 = 5;

#[derive(Clone, Copy)]
struct Sample {
    seq: u32,
    value: i32,
}

// Messages are copied in and out, so the queue owns all the storage it needs.
static SAMPLES: MsgQueue<Sample, 2> = MsgQueue::new();

static PRODUCER: Thread = Thread::new();

fn produce() {
    for seq in 0..NUM_SAMPLES {
        let sample = Sample {
            seq,
            value: seq as i32 * 10,
        };

        // Blocks while the queue is full: a slow consumer slows the producer
        // down.
        if let Err(e) = SAMPLES.put(sample, Timeout::FOREVER) {
            println!("producer: put failed ({e})");
            return;
        }
    }
}

fn consume() -> Result<()> {
    let mut received = 0;

    loop {
        match SAMPLES.get(Duration::from_millis(100)) {
            Ok(sample) => {
                println!("consumer: sample {} = {}", sample.seq, sample.value);
                received += 1;
            }
            Err(Errno::EAGAIN) => break,
            Err(e) => return Err(e),
        }
    }

    println!("consumer: no sample for 100 ms, {received} received");

    if received == NUM_SAMPLES {
        Ok(())
    } else {
        Err(Errno::EIO)
    }
}

fn jump_the_queue() -> Result<()> {
    let routine = Sample { seq: 100, value: 0 };
    let urgent = Sample { seq: 999, value: 0 };

    SAMPLES.put(routine, Timeout::NO_WAIT)?;
    SAMPLES.put_front(urgent, Timeout::NO_WAIT)?;

    // The queue is full: without waiting, a third message is refused.
    let ret = SAMPLES.put(routine, Timeout::NO_WAIT);
    println!("main: put on a full queue returns {ret:?}");

    let sample = SAMPLES.get(Timeout::NO_WAIT)?;
    println!(
        "main: sample {} was put last and comes out first",
        sample.seq
    );
    SAMPLES.purge();

    Ok(())
}

#[pebbleos::main]
fn main() -> Result<()> {
    PRODUCER.spawn(c"producer", Priority::MAIN.lower(1), produce)?;

    // Let the producer run until the queue stops it.
    thread::sleep(Duration::from_millis(50));
    println!(
        "main: producer is blocked with {} of {} samples queued",
        SAMPLES.len(),
        NUM_SAMPLES
    );

    let sample = SAMPLES.peek()?;
    println!("main: sample {} is next, and still queued", sample.seq);

    consume()?;
    jump_the_queue()?;

    println!("Sample done");

    Ok(())
}
