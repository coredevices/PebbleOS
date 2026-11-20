# Rust: Time

The {doc}`time sample <../../kernel/time>` written in {doc}`Rust
<../../../development/build_system/rust>`, on the `pebbleos` crate.

Durations are `core::time::Duration`, uptime is an `Instant`, and
`time::poll()` waits for a condition with a time limit.

```{pebbleos-sample}
:path: samples/rust/kernel/time
:sources: src/lib.rs
:output:
```
