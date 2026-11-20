# Rust: Threads

The {doc}`threads sample <../../kernel/threads>` written in {doc}`Rust
<../../../development/build_system/rust>`, on the `pebbleos` crate.

Each `Thread` is a static that owns its stack and runs a closure; `spawn()`
fails while the previous one still runs, and `join()` waits for it to end.

```{pebbleos-sample}
:path: samples/rust/kernel/threads
:sources: src/lib.rs
:output:
```
