# Rust: Poll groups

The {doc}`poll sample <../../kernel/poll>` written in {doc}`Rust
<../../../development/build_system/rust>`, on the `pebbleos` crate.

`PollGroup::wait()` returns the queue a message is in, which `is()` compares
with each of them.

```{pebbleos-sample}
:path: samples/rust/kernel/poll
:sources: src/lib.rs
:output:
```
