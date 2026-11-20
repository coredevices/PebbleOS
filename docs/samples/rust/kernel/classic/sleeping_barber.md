# Rust: Sleeping barber

The {doc}`sleeping barber sample <../../../kernel/classic/sleeping_barber>`
written in {doc}`Rust <../../../../development/build_system/rust>`, on the
`pebbleos` crate.

The shop's counts are in one `Mutex`; `Thread::state()` tells the barber is
asleep.

```{pebbleos-sample}
:path: samples/rust/kernel/classic/sleeping_barber
:sources: src/lib.rs
:output:
```
