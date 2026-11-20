# Rust: Semaphores

The {doc}`semaphore sample <../../kernel/semaphore>` written in {doc}`Rust
<../../../development/build_system/rust>`, on the `pebbleos` crate.

The transfer interrupt is a `Timer` whose function gives the semaphore;
errors are `Result`s, printed as Rust prints them.

```{pebbleos-sample}
:path: samples/rust/kernel/semaphore
:sources: src/lib.rs
:output:
```
