// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

#![no_std]

use pebbleos::heap::{Block, Heap};
use pebbleos::{Errno, Result, println};

const BLOCK_SIZE: usize = 100;

// A heap of its own bounds what one user can take, here a 2 KiB cache.
static HEAP: Heap<2048> = Heap::new();

fn log_usage(when: &str) {
    let stats = HEAP.stats();
    println!(
        "heap: {when}: {} used, {} free, largest free block {}",
        stats.used, stats.free, stats.largest_free
    );
}

#[pebbleos::main]
fn main() -> Result<()> {
    let mut blocks: [Option<Block>; 32] = [const { None }; 32];

    log_usage("empty");

    // Allocation fails with None, which the caller handles.
    let name = HEAP
        .alloc_copy(b"a string on the heap")
        .ok_or(Errno::ENOMEM)?;
    println!(
        "heap: block is in the heap: {}, allocated: {}",
        yes_no(HEAP.contains(name.as_ptr())),
        yes_no(HEAP.is_allocated(name.as_ptr()))
    );
    // A block goes back to its heap when dropped.
    drop(name);

    let mut num_blocks = 0;
    for slot in &mut blocks {
        *slot = HEAP.alloc(BLOCK_SIZE);
        if slot.is_none() {
            break;
        }
        num_blocks += 1;
    }
    println!("heap: full after {num_blocks} blocks of {BLOCK_SIZE} bytes");
    log_usage("full");

    // Every other block freed: plenty of memory in total, but in pieces.
    for slot in blocks.iter_mut().step_by(2) {
        *slot = None;
    }
    log_usage("every other block freed");
    let big = HEAP.alloc(2 * BLOCK_SIZE);
    println!(
        "heap: {} bytes from a fragmented heap: {}",
        2 * BLOCK_SIZE,
        if big.is_none() {
            "refused"
        } else {
            "allocated"
        }
    );

    // Freed neighbors merge, so the pieces become one block again.
    drop(blocks);
    log_usage("all freed");

    println!(
        "heap: least memory it ever had left: {} of {} bytes",
        HEAP.min_headroom(),
        HEAP.size()
    );

    println!("Sample done");

    Ok(())
}

fn yes_no(b: bool) -> &'static str {
    if b { "yes" } else { "no" }
}
