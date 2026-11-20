# Rust: Heaps

The {doc}`heap sample <../../kernel/heap>` written in {doc}`Rust
<../../../development/build_system/rust>`, on the `pebbleos` crate.

`Heap<N>` sets itself up and takes a lock on first use; each `Block` it
returns goes back to it when dropped, so freeing every other block is
emptying every other slot of an array.

```{pebbleos-sample}
:path: samples/rust/kernel/heap
:sources: src/lib.rs
:output:
```
