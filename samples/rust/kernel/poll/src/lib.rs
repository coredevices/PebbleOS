// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::time::Duration;

use pebbleos::msgq::MsgQueue;
use pebbleos::poll::PollGroup;
use pebbleos::thread::{self, Priority, Thread};
use pebbleos::time::Timeout;
use pebbleos::{Result, println};

#[derive(Clone, Copy)]
struct ButtonEvent {
    button: u8,
    pressed: bool,
}

#[derive(Clone, Copy)]
struct AccelSample {
    x: i16,
    y: i16,
    z: i16,
}

// One queue per source, each with its own message type and depth.
static BUTTONS: MsgQueue<ButtonEvent, 4> = MsgQueue::new();
static ACCEL: MsgQueue<AccelSample, 8> = MsgQueue::new();
static EVENTS: PollGroup = PollGroup::new();

static SOURCE: Thread = Thread::new();

fn source() -> Result<()> {
    BUTTONS.put(
        ButtonEvent {
            button: 1,
            pressed: true,
        },
        Timeout::FOREVER,
    )?;
    thread::sleep(Duration::from_millis(10));
    ACCEL.put(
        AccelSample {
            x: 10,
            y: -20,
            z: 1000,
        },
        Timeout::FOREVER,
    )?;
    thread::sleep(Duration::from_millis(10));
    BUTTONS.put(
        ButtonEvent {
            button: 1,
            pressed: false,
        },
        Timeout::FOREVER,
    )
}

#[pebbleos::main]
fn main() -> Result<()> {
    // Queues join before the first wait, and for good.
    EVENTS.add(&BUTTONS);
    EVENTS.add(&ACCEL);

    SOURCE.spawn(c"source", Priority::MAIN.lower(1), || {
        if let Err(e) = source() {
            println!("source: put failed ({e})");
        }
    })?;

    // The wait leaves the message in its queue.
    while let Some(q) = EVENTS.wait(Duration::from_millis(100)) {
        if q.is(&BUTTONS) {
            if let Ok(event) = BUTTONS.get(Timeout::NO_WAIT) {
                let action = if event.pressed { "pressed" } else { "released" };
                println!("loop: button {} {action}", event.button);
            }
        } else if q.is(&ACCEL)
            && let Ok(sample) = ACCEL.get(Timeout::NO_WAIT)
        {
            println!("loop: accel {} {} {}", sample.x, sample.y, sample.z);
        }
    }
    println!("loop: nothing for 100 ms");

    println!("Sample done");

    Ok(())
}
