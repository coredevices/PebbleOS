# Rust: Timers

The {doc}`timer sample <../../kernel/timer>` written in {doc}`Rust
<../../../development/build_system/rust>`, on the `pebbleos` crate.

`Timer<T>` holds the data its function gets, in place of the C structure
that embeds the timer; what the interrupt and `main()` share is atomic.

```{pebbleos-sample}
:path: samples/rust/kernel/timer
:sources: src/lib.rs
:output:
```
