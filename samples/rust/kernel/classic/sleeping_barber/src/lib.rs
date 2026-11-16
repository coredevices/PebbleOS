// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::time::Duration;

use pebbleos::sync::{Mutex, Semaphore};
use pebbleos::thread::{self, Priority, State, Thread};
use pebbleos::time::{self, Timeout};
use pebbleos::{Result, println};

const NUM_CHAIRS: u32 = 3;
const NUM_CUSTOMERS: usize = 5;

// Customers run above the barber, so all of them arrive before the first
// haircut starts.
const CUSTOMER_PRIO: Priority = Priority::MAIN.lower(1);
const BARBER_PRIO: Priority = Priority::MAIN.lower(2);

struct Shop {
    waiting: u32,
    haircuts: u32,
    turned_away: u32,
}

// Counts the customers waiting, which is what the barber sleeps on.
static CUSTOMERS_WAITING: Semaphore = Semaphore::new(0, NUM_CHAIRS);
// Given by the barber when they are ready for the next one.
static BARBER_CHAIR: Semaphore = Semaphore::new(0, 1);
static SHOP: Mutex<Shop> = Mutex::new(Shop {
    waiting: 0,
    haircuts: 0,
    turned_away: 0,
});

static BARBER: Thread = Thread::new();
static CUSTOMERS: [Thread; NUM_CUSTOMERS] = [const { Thread::new() }; NUM_CUSTOMERS];

fn barber() {
    loop {
        // Asleep until a customer sits down in the waiting room.
        let _ = CUSTOMERS_WAITING.take(Timeout::FOREVER);
        SHOP.lock().waiting -= 1;

        BARBER_CHAIR.give();
        thread::sleep(Duration::from_millis(20));

        SHOP.lock().haircuts += 1;
    }
}

fn customer(id: usize) {
    // Looking for a chair and taking it is one step, or two customers could
    // take the last one.
    let seated = {
        let mut shop = SHOP.lock();
        let seated = shop.waiting < NUM_CHAIRS;
        if seated {
            shop.waiting += 1;
        } else {
            shop.turned_away += 1;
        }
        seated
    };

    if !seated {
        println!("customer {id}: no free chair, leaves");
        return;
    }

    CUSTOMERS_WAITING.give();
    let _ = BARBER_CHAIR.take(Timeout::FOREVER);
    println!("customer {id}: in the barber chair");
}

#[pebbleos::main]
fn main() -> Result<()> {
    BARBER.spawn(c"barber", BARBER_PRIO, barber)?;
    thread::sleep(Duration::from_millis(10));
    println!(
        "shop: empty, barber asleep: {}",
        yes_no(BARBER.state() == State::Blocked)
    );

    println!("shop: {NUM_CUSTOMERS} customers arrive at once, for {NUM_CHAIRS} chairs");
    for (id, t) in CUSTOMERS.iter().enumerate() {
        t.spawn(c"customer", CUSTOMER_PRIO, move || customer(id))?;
    }

    time::poll(Duration::from_millis(5), Duration::from_secs(1), || {
        let shop = SHOP.lock();
        shop.haircuts + shop.turned_away == NUM_CUSTOMERS as u32
    })?;

    thread::sleep(Duration::from_millis(10));
    let shop = SHOP.lock();
    println!(
        "shop: {} haircuts, {} customers turned away",
        shop.haircuts, shop.turned_away
    );
    drop(shop);
    println!(
        "shop: empty, barber asleep: {}",
        yes_no(BARBER.state() == State::Blocked)
    );

    println!("Sample done");

    Ok(())
}

fn yes_no(b: bool) -> &'static str {
    if b { "yes" } else { "no" }
}
