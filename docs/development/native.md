# Native

The `native_emery` board builds PebbleOS as an application for the machine
you are working on (Linux or macOS), with a window standing in for the
display. It boots much faster than QEMU and runs under the host's debugger,
sanitizers and profilers.

```{important}
The native port is a sketch. Drivers are emulated or stubbed, Bluetooth uses
the fake HCI controller, and third-party apps cannot run: they are ARM
binaries. Only macOS has been tried so far.
```

## Requirements

A host C compiler and [SDL2](https://www.libsdl.org), both part of the
dependencies in [](getting_started.md).

## Build and run

```shell
pbl configure --board=native_emery
pbl build
build/pebbleos
```

| Key                         | Button |
| --------------------------- | ------ |
| Up                          | Up     |
| Down                        | Down   |
| Right, Enter, Space         | Select |
| Left, Backspace, Escape     | Back   |

The mouse drives the touchscreen: click and drag, or scroll with the wheel or
two fingers on a trackpad, which drags a finger from the pointer.

The terminal is the watch's serial console: logs go there, and as on the
watch, `Ctrl-C` opens the shell prompt and `Ctrl-D` leaves it. `Ctrl-\`
quits.

Options:

- `-f FILE`: file backing the external flash, `pebbleos-flash.bin` in the
  current directory by default. It keeps the watch's state across runs.
- `-r FILE`: system resources installed at boot, the ones from the build by
  default.
- `-s N`: window scale factor.
- `-t SECONDS`: quit after that long.
- `-S FILE`: save the last frame to a BMP file on quitting.

A reset of the watch restarts the process. A fatal error aborts it, so that a
debugger stops right there:

```shell
lldb build/pebbleos
```

## Sanitizers

The compiler's sanitizers can be built in with Kconfig options:

```shell
pbl configure --board=native_emery -DCONFIG_ASAN=y -DCONFIG_UBSAN=y
```

- `CONFIG_ASAN`: AddressSanitizer, out-of-bounds and use-after-free accesses.
  The firmware heaps keep what they have not handed out poisoned.
  `CONFIG_ASAN_RECOVER` reports errors and goes on, with
  `ASAN_OPTIONS=halt_on_error=0`.
- `CONFIG_UBSAN`: UndefinedBehaviorSanitizer, without the alignment check.
- `CONFIG_TSAN`: ThreadSanitizer, not together with AddressSanitizer. Firmware
  threads take turns on one CPU, so it mostly checks the host side.

## How it works

The kernel's `posix` architecture (`kernel/arch/posix`) runs each kernel
thread as a pthread. Only the thread holding the CPU lock runs, and switches
hand the lock over explicitly, so scheduling stays the kernel's.

Interrupts come from host threads: the 1 kHz tick, the window's input and the
terminal. A
host thread takes the CPU lock between kernel threads, runs the handler as an
ISR and releases it, much like an interrupt arriving between instructions.
Kernel threads are not preempted while they hold the CPU, so an interrupt
waits for the running thread to block.

Drivers come in two halves, as in Zephyr's `native_sim`: the top half is
firmware code, and the bottom half (`*_bottom.c`, listed with
`pbl_host_sources()`) talks to the host and sees none of the firmware headers.
The `posix` SoC (`soc/posix`) owns the process `main()`, which knows no
driver: it starts the firmware on a thread of its own and runs the main thread
hooks and command line options that bottoms register (`posix_host.h`). SDL is
one such bottom (`soc/posix/sdl`), selected by the drivers that use it: the
display (`DISPLAY_SDL`), the buttons (`BUTTON_SDL`, the keyboard) and the
touchscreen (`TOUCH_SDL`, the mouse). It runs on the main thread, as macOS
requires.

A native build is linked without the firmware linker script
(`PBL_NO_LINKER_SCRIPT`, decided in `cmake/modules/linker.cmake`). Code that
gathers objects from many files, like the shell commands, then uses
`PBL_UNSORTED_SECTION()`: the host linker collects the section on its own but
cannot sort it, so the shell picks commands by name when looking them up.

Things the linker script provides on the target, such as the heaps, the app
RAM and the build ID, are plain objects in `soc/posix/memory.c`. Firmware
`malloc()` and `free()` are renamed to the firmware heaps at compile time, as
`--wrap` does on the target.
