// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use pebbleos::println;

#[pebbleos::main]
fn main() {
    println!("Hello World! {}", pebbleos::kconfig::BOARD);
}
