// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::sync::atomic::{AtomicU32, Ordering};

use pebbleos::thread::{self, Priority, Thread};
use pebbleos::work::{Work, WorkQueue};
use pebbleos::{Result, println};

// What the work item acts on, which its function gets through it.
struct Sensor {
    name: &'static str,
    pending_events: AtomicU32,
    runs: AtomicU32,
}

impl Sensor {
    const fn new(name: &'static str) -> Sensor {
        Sensor {
            name,
            pending_events: AtomicU32::new(0),
            runs: AtomicU32::new(0),
        }
    }
}

fn sensor_work(sensor: &'static Work<Sensor>) {
    // One run serves every event since the last one, however often it was
    // submitted.
    println!(
        "{}: handling {} events on {}",
        sensor.name,
        sensor.pending_events.swap(0, Ordering::Relaxed),
        thread::current().name()
    );
    sensor.runs.fetch_add(1, Ordering::Relaxed);
}

fn sensor_event(q: &'static WorkQueue, sensor: &'static Work<Sensor>) {
    sensor.pending_events.fetch_add(1, Ordering::Relaxed);
    if !sensor.submit(q) {
        println!("{}: already pending, not queued again", sensor.name);
    }
}

static ACCEL: Work<Sensor> = Work::new(sensor_work, Sensor::new("accel"));
static TOUCH: Work<Sensor> = Work::new(sensor_work, Sensor::new("touch"));

// A queue of its own keeps slow work from delaying the system's.
static SENSOR_Q: WorkQueue = WorkQueue::new();
static SENSOR_Q_THREAD: Thread = Thread::new();

#[pebbleos::main]
fn main() -> Result<()> {
    let system = WorkQueue::system();

    // Both queues run below main, so nothing runs until main waits for them.
    println!("main: three accel events for the system queue");
    for _ in 0..3 {
        sensor_event(system, &ACCEL);
    }
    system.flush();
    println!(
        "main: accel work ran {} time(s)",
        ACCEL.runs.load(Ordering::Relaxed)
    );

    println!("main: touch and accel events for a queue of our own");
    SENSOR_Q.start(&SENSOR_Q_THREAD, c"sensor_q", Priority::MAIN.lower(1))?;
    sensor_event(&SENSOR_Q, &TOUCH);
    sensor_event(&SENSOR_Q, &ACCEL);
    SENSOR_Q.flush();

    println!("main: a touch event, canceled before the queue gets to it");
    sensor_event(&SENSOR_Q, &TOUCH);
    if TOUCH.cancel_sync() {
        println!("main: touch work was pending and will not run");
    }
    SENSOR_Q.flush();
    println!(
        "main: touch work ran {} time(s)",
        TOUCH.runs.load(Ordering::Relaxed)
    );

    println!("Sample done");

    Ok(())
}
