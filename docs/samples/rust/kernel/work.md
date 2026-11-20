# Rust: Work queues

The {doc}`work queues sample <../../kernel/work>` written in {doc}`Rust
<../../../development/build_system/rust>`, on the `pebbleos` crate.

`Work<T>` holds the data its function gets, in place of the C structure that
embeds the item; `WorkQueue::start()` runs a queue on a `Thread` of its own.

```{pebbleos-sample}
:path: samples/rust/kernel/work
:sources: src/lib.rs
:output:
```
