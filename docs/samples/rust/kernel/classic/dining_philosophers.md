# Rust: Dining philosophers

The {doc}`dining philosophers sample
<../../../kernel/classic/dining_philosophers>` written in {doc}`Rust
<../../../../development/build_system/rust>`, on the `pebbleos` crate.

The forks are `Mutex<()>`, and a philosopher puts one down by dropping its
guard.

```{pebbleos-sample}
:path: samples/rust/kernel/classic/dining_philosophers
:sources: src/lib.rs
:output:
```
