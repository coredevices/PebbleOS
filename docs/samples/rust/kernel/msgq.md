# Rust: Message queues

The {doc}`message queue sample <../../kernel/msgq>` written in {doc}`Rust
<../../../development/build_system/rust>`, on the `pebbleos` crate.

`MsgQueue<T, N>` holds `N` messages of type `T`, any `Copy` type, which
`put()` and `get()` copy in and out.

```{pebbleos-sample}
:path: samples/rust/kernel/msgq
:sources: src/lib.rs
:output:
```
