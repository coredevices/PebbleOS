# Applications

PebbleOS is the kernel, the drivers, the subsystems and the libraries. What
runs on top of it is an application: the code that provides `main()`, built
against PebbleOS from its own directory. The watch firmware under `pebble/`
is one, and so is anything else, from a driver test to a product that is
not a watch.

## A minimal application

`samples/hello_world` is the smallest one:

```text
samples/hello_world/
  CMakeLists.txt
  src/main.c
```

```cmake
cmake_minimum_required(VERSION 3.20)

find_package(PebbleOS REQUIRED HINTS $ENV{PEBBLEOS_BASE})
project(hello_world)

target_sources(app PRIVATE src/main.c)
```

```c
#include <pbl/logging/logging.h>

int main(void) {
  PBL_LOG_ALWAYS("Hello World! %s", CONFIG_BOARD);

  return 0;
}
```

`find_package(PebbleOS)` configures the build for the board, adds PebbleOS
and defines `app`, the library the application's sources go to. It comes
before `project()`, as it also selects the toolchain. The image is linked
once the application's `CMakeLists.txt` has been read, so the application
can use every `pbl_*` helper described in {doc}`cmake`.

## Building and running

{doc}`../../pbl/configure` takes the application's directory:

```shell
pbl configure --board qemu_emery samples/hello_world
pbl build
pbl run
```

The message goes to the console once, at boot. Attach {doc}`../../pbl/console`
from another shell, then restart the emulator with `system_reset` in the
QEMU monitor:

```text
* - 00:00:00.000 init.c:38> *** Booting PebbleOS v4.45.0 ***
* - 00:00:00.004 main.c:7> Hello World! qemu_emery
```

Plain CMake needs to be told where PebbleOS is, with the `PEBBLEOS_BASE`
environment variable or with `-DPebbleOS_DIR=`:

```shell
cmake -B build -GNinja -S samples/hello_world -DBOARD=qemu_emery \
  -DPebbleOS_DIR=$PWD/cmake
cmake --build build
```

The image is `pebbleos.elf`, `.hex` and `.bin` in the build directory,
whatever the application calls its project.

## Before main()

The kernel brings the system up before it calls `main()`, on the main
thread: logging and the task watchdog are running, when enabled
(`CONFIG_LOG`, `CONFIG_TASK_WDT`), and every device the board defines has
been initialized (see {doc}`../../drivers/device_model`).
`main()` may return; the other threads go on.

The kernel and the task watchdog call a few functions an application may
override, to decide what a failure does. Their defaults log and reset:

| Function | Called when |
| --- | --- |
| `pbl_fatal_error_handler()` | An assertion or a panic stops the system |
| `pbl_heap_oom_handler()` | An allocation that must not fail does |
| `pbl_kernel_syscall_exit()`, `pbl_kernel_syscall_fault()` | A syscall made from unprivileged code returns, or is given bad arguments |
| `pbl_task_wdt_stalled()`, `pbl_task_wdt_recovered()`, `pbl_task_wdt_expired()` | A watched thread stops making progress |

## Configuration

An application configures PebbleOS with files in its own directory, applied
on top of the board's defconfig (see {doc}`kconfig`):

| File | Purpose |
| --- | --- |
| `prj.conf` | What the application enables on every board |
| `prj_<variant>.conf` | What a variant (`--variant`) adds |
| `boards/<board>.conf` | What it enables or tunes on one board |
| `boards/<board>_<variant>.conf` | The same, for one variant only |
| `Kconfig` | The application's own options |

An application with a `Kconfig` makes it the root of the configuration
tree: it defines its options and sources PebbleOS's with
`source "Kconfig.pebbleos"`. Defaults it gives before that line take
precedence over PebbleOS's own.

Subsystems are off unless the application asks for them, e.g.
`CONFIG_BT=y`. A board's defconfig describes its hardware; what an
application does with it, such as the firmware's backlight levels on
`obelix`, goes in the application's `boards/obelix.conf`.

The firmware is built the same way, from `pebble/CMakeLists.txt`, and its
configuration follows the same layout: `pebble/prj.conf`,
`pebble/prj_normal.conf`, `pebble/prj_prf.conf`, `pebble/boards/` and
`pebble/Kconfig`.
