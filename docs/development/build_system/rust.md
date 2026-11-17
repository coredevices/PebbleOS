# Rust

An {doc}`application <applications>` can be written in Rust. It is a
`no_std` crate, built by cargo as a static library and linked into the image
along with PebbleOS, on every board.

## Requirements

- [rustup](https://rustup.rs). The toolchain is pinned by
  `rust-toolchain.toml` at the root of the checkout, which rustup installs
  with the targets the boards need the first time cargo runs.
- [bindgen](https://rust-lang.github.io/rust-bindgen/) 0.71 or later, which
  generates the bindings to the C API. CI uses 0.73.2:

  ```shell
  cargo install bindgen-cli --locked --version 0.73.2
  ```

  It loads libclang: Xcode's command line tools have it on macOS,
  `libclang-dev` on Ubuntu and `clang-devel` on Fedora.

## A minimal application

`samples/rust/hello_world` is the smallest one:

```text
samples/rust/hello_world/
  CMakeLists.txt
  Cargo.toml
  prj.conf
  src/lib.rs
```

```cmake
cmake_minimum_required(VERSION 3.20)

find_package(PebbleOS REQUIRED HINTS $ENV{PEBBLEOS_BASE})
project(hello_world)

pbl_rust_app()
```

```toml
[package]
name = "hello_world"
version = "0.1.0"
edition = "2024"
license = "Apache-2.0"
publish = false

[lib]
crate-type = ["staticlib"]

[dependencies]
pebbleos = { path = "../../../rust/pebbleos" }
```

```rust
#![no_std]

use pebbleos::println;

#[pebbleos::main]
fn main() {
    println!("Hello World! {}", pebbleos::kconfig::BOARD);
}
```

`pbl_rust_app()` builds the crate next to `CMakeLists.txt`.
`#[pebbleos::main]` marks the function the kernel runs as `main()`. It is
configured, built and run as any other application:

```shell
pbl configure --board qemu_emery samples/rust/hello_world
pbl build
pbl run
```

An application is a crate of its own, the samples too: the cargo workspace
at the root of the checkout only has the crates of PebbleOS. cargo writes
the application's `Cargo.lock` next to its `Cargo.toml`. The build sets the
profile, so an application needs none.

## Crates

| Crate | Contents |
| --- | --- |
| `rust/pebbleos-sys` | The C API as bindgen makes it: unsafe functions, C structs, and every Kconfig symbol as a `CONFIG_*` constant |
| `rust/pebbleos-macros` | `#[main]`, which `pebbleos` re-exports |
| `rust/pebbleos` | The API for applications, safe and in the language's terms: `print!` and `println!`, `#[main]`, the panic handler, and the kernel API. `pebbleos::sys` is `pebbleos-sys` |

`main()` returns nothing or a `pebbleos::Result`, whose error, an `Errno`,
is printed and returned as a negative error number.

## Kernel API

| Module | Contents |
| --- | --- |
| `thread` | `Thread`, `Priority`, `current()`, `sleep()`, `yield_now()` |
| `sync` | `Mutex`, `ReentrantMutex`, `Semaphore` |
| `msgq`, `poll` | `MsgQueue`, `PollGroup` |
| `timer` | `Timer` |
| `work` | `WorkQueue`, `Work`, `DelayedWork` |
| `heap` | `Heap` and its `Block`s |
| `time` | `Timeout`, `Instant`, the wall clock, `poll()` |

Kernel objects are statics, built by `const fn`, and need no memory
allocation. A `Thread` owns its stack, of 1 KiB unless its type says
otherwise, and runs a closure, which waits at the top of the stack buffer
for the thread to start: the closure takes its size out of the stack. A
stack too small for it fails to build.

```rust
static WORKER: Thread<2048> = Thread::new();

WORKER.spawn(c"worker", Priority::MAIN.lower(1), || work())?;
WORKER.join(Duration::from_secs(1))?;
```

The data of a `Mutex` is only reached through the guard `lock()` returns,
which unlocks it when dropped. A `Timer`, `Work` or `DelayedWork` holds data
of its own, which its function gets through it, where C embeds the object in
a structure of its own. Calls that block take a `Duration`, or a `Timeout`
for `NO_WAIT` and `FOREVER`.

The kernel samples are also in Rust, in `samples/rust/kernel`.

A panic is a kernel panic: its message and location go to the log and the
system stops as a failed assertion does, through
`pbl_fatal_error_handler()`. Crates are built with `panic = "abort"`.

`print!` and `println!` write to the console with `pbl_print()`, so they
work from any context and do not need `CONFIG_CONSOLE_STDOUT`.

## How it is built

The C API is generated for each build directory, as its structs follow the
configuration: `struct pbl_thread` has fields only with
`CONFIG_USERSPACE`, for instance. `rust/pebbleos-sys/wrapper.h` lists the
headers. bindgen parses them with the include directories, definitions and
C library headers of the C code, and with `-fshort-enums`, as the ARM EABI
sizes enums. Ninja tracks the headers it read and runs it again when one
changes.

bindgen also writes C wrappers around the `static inline` functions of these
headers, which Rust cannot call otherwise; they are compiled into `app`.

| Build directory | Contents |
| --- | --- |
| `rust/bindings.rs` | The bindings `pebbleos-sys` includes |
| `rust/static_fns.c` | The wrappers of the `static inline` functions |
| `rust/target/` | cargo's output |

The configuration decides how the crate is compiled:

| Configuration | Rust |
| --- | --- |
| Cortex-M4 | `thumbv7em-none-eabi`, with `-Ctarget-cpu=cortex-m4` when it has an FPU |
| Cortex-M33, STAR-MC1 | `thumbv8m.main-none-eabi`, with `-Ctarget-cpu=cortex-m33` when it has an FPU and the DSP extension |
| An FPU, without `CONFIG_FP_SOFTFP_ABI` | The `eabihf` target of the core |
| `CONFIG_ARCH_POSIX` | The host's target |
| `CONFIG_ARCH_POSIX` with Emscripten | `wasm32-unknown-emscripten` |
| `CONFIG_NO_OPTIMIZATIONS`, `CONFIG_DEBUG_OPTIMIZATIONS` | `opt-level` 0 and 1, `s` otherwise |
| `CONFIG_DEBUG_INFO` | `debug` |

Rust follows the float ABI of the C code. With an FPU that is the hard-float
calling convention (`-mfloat-abi=hard`), so Rust uses the `eabihf` targets.
Where `CONFIG_FP_SOFTFP_ABI` is selected, as in the Pebble firmware, the C
code passes floating-point arguments in core registers
(`-mfloat-abi=softfp`), and Rust uses the `eabi` targets and only uses the
FPU within functions.

With Emscripten, the firmware is linked as C++ with exception catching on:
Rust's prebuilt `compiler_builtins` has landing pads, which need the
exception runtime, though nothing throws.
