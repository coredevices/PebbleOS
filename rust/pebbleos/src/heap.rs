// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

//! Heaps over a buffer of their own.

use core::cell::UnsafeCell;
use core::ffi::c_void;
use core::mem::{self, MaybeUninit};
use core::ops::{Deref, DerefMut};
use core::ptr::NonNull;
use core::slice;
use core::sync::atomic::AtomicBool;

use crate::irq;
use crate::sync::RawMutex;
use crate::sys;
use crate::time::Timeout;

#[repr(C, align(8))]
struct Region<const N: usize>([MaybeUninit<u8>; N]);

/// A heap of `N` bytes, which bounds what its users can take. Threads share
/// it through a mutex; interrupts cannot use it.
pub struct Heap<const N: usize> {
    raw: UnsafeCell<sys::pbl_heap>,
    region: UnsafeCell<Region<N>>,
    lock: RawMutex,
    ready: AtomicBool,
}

// SAFETY: the heap takes its lock around every use.
unsafe impl<const N: usize> Sync for Heap<N> {}

/// How much of a heap is in use.
#[derive(Clone, Copy, Debug)]
pub struct Stats {
    /// Bytes allocated, with their headers.
    pub used: usize,
    /// Bytes free.
    pub free: usize,
    /// The largest free block.
    pub largest_free: usize,
}

unsafe extern "C" fn lock(ctx: *mut c_void) {
    // SAFETY: ctx is the heap's mutex.
    unsafe { &*(ctx as *const RawMutex) }
        .lock(Timeout::FOREVER)
        .expect("heap lock");
}

unsafe extern "C" fn unlock(ctx: *mut c_void) {
    // SAFETY: ctx is the heap's mutex, which lock() took on this thread.
    unsafe { &*(ctx as *const RawMutex) }.unlock()
}

impl<const N: usize> Heap<N> {
    /// An empty heap.
    pub const fn new() -> Self {
        Heap {
            // SAFETY: set up on first use.
            raw: UnsafeCell::new(unsafe { mem::zeroed() }),
            region: UnsafeCell::new(Region([const { MaybeUninit::uninit() }; N])),
            lock: RawMutex::new(),
            ready: AtomicBool::new(false),
        }
    }

    fn raw(&'static self) -> *mut sys::pbl_heap {
        // The region's address is only known once the heap is in place.
        irq::once(&self.ready, || {
            let start = self.region.get() as *mut c_void;
            // SAFETY: no other user before the heap is ready; the region and
            // the mutex are static.
            unsafe {
                sys::pbl_heap_init(self.raw.get(), start, start.byte_add(N), false);
                sys::pbl_heap_set_lock(
                    self.raw.get(),
                    sys::pbl_heap_lock {
                        lock: Some(lock),
                        unlock: Some(unlock),
                        ctx: &self.lock as *const RawMutex as *mut c_void,
                    },
                );
            }
        });
        self.raw.get()
    }

    /// A block of `len` zeroed bytes, or `None` if no free block is that
    /// large.
    pub fn alloc(&'static self, len: usize) -> Option<Block> {
        let heap = self.raw();
        // SAFETY: the heap is valid.
        let ptr = NonNull::new(unsafe { sys::pbl_heap_zalloc(heap, len, 0) } as *mut u8)?;
        Some(Block { heap, ptr, len })
    }

    /// A block holding a copy of `bytes`.
    pub fn alloc_copy(&'static self, bytes: &[u8]) -> Option<Block> {
        let mut block = self.alloc(bytes.len())?;
        block.copy_from_slice(bytes);
        Some(block)
    }

    /// Whether `ptr` points into the heap.
    pub fn contains(&'static self, ptr: *const u8) -> bool {
        // SAFETY: the heap is valid.
        unsafe { sys::pbl_heap_contains(self.raw(), ptr as *const c_void) }
    }

    /// Whether `ptr` is the start of an allocated block.
    pub fn is_allocated(&'static self, ptr: *const u8) -> bool {
        // SAFETY: the heap is valid; the pointer is only compared.
        unsafe { sys::pbl_heap_is_allocated(self.raw(), ptr as *mut c_void) }
    }

    /// How much of it is in use.
    pub fn stats(&'static self) -> Stats {
        let (mut used, mut free, mut largest_free) = (0, 0, 0);
        // SAFETY: the heap is valid.
        unsafe { sys::pbl_heap_stats(self.raw(), &mut used, &mut free, &mut largest_free) };
        Stats {
            used: used as usize,
            free: free as usize,
            largest_free: largest_free as usize,
        }
    }

    /// Bytes it manages, block headers included.
    pub fn size(&'static self) -> usize {
        // SAFETY: the heap is valid.
        unsafe { sys::pbl_heap_size(self.raw()) }
    }

    /// The least free memory it ever had.
    pub fn min_headroom(&'static self) -> usize {
        // SAFETY: the heap is valid.
        unsafe { sys::pbl_heap_min_headroom(self.raw()) }
    }
}

impl<const N: usize> Default for Heap<N> {
    fn default() -> Self {
        Self::new()
    }
}

/// Memory from a [`Heap`], freed when dropped.
pub struct Block {
    heap: *mut sys::pbl_heap,
    ptr: NonNull<u8>,
    len: usize,
}

// SAFETY: the bytes belong to the block alone, and the heap is locked.
unsafe impl Send for Block {}
unsafe impl Sync for Block {}

impl Deref for Block {
    type Target = [u8];

    fn deref(&self) -> &[u8] {
        // SAFETY: the block owns len initialized bytes.
        unsafe { slice::from_raw_parts(self.ptr.as_ptr(), self.len) }
    }
}

impl DerefMut for Block {
    fn deref_mut(&mut self) -> &mut [u8] {
        // SAFETY: as above, and the block is borrowed mutably.
        unsafe { slice::from_raw_parts_mut(self.ptr.as_ptr(), self.len) }
    }
}

impl Drop for Block {
    fn drop(&mut self) {
        // SAFETY: allocated from this static heap.
        unsafe { sys::pbl_heap_free(self.heap, self.ptr.as_ptr() as *mut c_void, 0) }
    }
}
