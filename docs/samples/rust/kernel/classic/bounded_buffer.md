# Rust: Bounded buffer

The {doc}`bounded buffer sample <../../../kernel/classic/bounded_buffer>`
written in {doc}`Rust <../../../../development/build_system/rust>`, on the
`pebbleos` crate.

The ring and its counts are in one `Mutex`, which holds them only while the
semaphores say there is room or an item.

```{pebbleos-sample}
:path: samples/rust/kernel/classic/bounded_buffer
:sources: src/lib.rs
:output:
```
