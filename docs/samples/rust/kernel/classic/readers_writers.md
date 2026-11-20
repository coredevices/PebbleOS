# Rust: Readers and writers

The {doc}`readers and writers sample
<../../../kernel/classic/readers_writers>` written in {doc}`Rust
<../../../../development/build_system/rust>`, on the `pebbleos` crate.

The lock hands out guards, which leave the room when dropped.

```{pebbleos-sample}
:path: samples/rust/kernel/classic/readers_writers
:sources: src/lib.rs
:output:
```
