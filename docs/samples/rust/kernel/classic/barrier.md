# Rust: Barrier

The {doc}`barrier sample <../../../kernel/classic/barrier>` written in
{doc}`Rust <../../../../development/build_system/rust>`, on the `pebbleos`
crate.

The count of arrivals is in a `Mutex`, whose guard is dropped before waiting
at the gate.

```{pebbleos-sample}
:path: samples/rust/kernel/classic/barrier
:sources: src/lib.rs
:output:
```
