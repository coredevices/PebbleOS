# Rust: Priority inversion

The {doc}`priority inversion sample
<../../../kernel/classic/priority_inversion>` written in {doc}`Rust
<../../../../development/build_system/rust>`, on the `pebbleos` crate.

`ThreadRef::priority()` is the priority a thread runs at, inherited ones
included, and `base_priority()` the one it was given.

```{pebbleos-sample}
:path: samples/rust/kernel/classic/priority_inversion
:sources: src/lib.rs
:output:
```
