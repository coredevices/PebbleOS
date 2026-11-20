# Rust: Mutexes

The {doc}`mutex sample <../../kernel/mutex>` written in {doc}`Rust
<../../../development/build_system/rust>`, on the `pebbleos` crate.

The balance is in a `Mutex<u32>`, only reachable through the guard `lock()`
returns, which unlocks when dropped. Locking it again from its owner panics,
as it would hand out the data twice: `ReentrantMutex` is the one its owner
may lock again, and shares its data instead. The race without a lock is
staged with an atomic, read and written in two steps.

```{pebbleos-sample}
:path: samples/rust/kernel/mutex
:sources: src/lib.rs
:output:
```
