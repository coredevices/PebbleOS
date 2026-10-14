# Native simulator

The native simulator builds PebbleOS as an ordinary program for the machine
you are working on, Linux or macOS, with a window standing in for the
watch. It boots in a fraction of the time the emulator takes, and runs
under the host's own debugger, sanitizers and profilers. CI builds it on
both hosts and boots it to the launcher on every change, and the
{doc}`integration tests <../tests/integration>` run against it.

## What is simulated

The simulator reproduces a platform, but not an architecture:

The platform
: The `native_emery` board is the Emery platform of Pebble Time 2: its
  200 × 228 color display, its buttons and its touchscreen, and the same
  firmware services, UI and apps built into the firmware.

Not the architecture
: The firmware is compiled for the host CPU, with the host compiler and C
  library, and the kernel runs its threads on host threads. That is what
  makes it fast and debuggable, but third-party apps cannot run: their
  `.pbw` binaries are ARM code. Use the {doc}`emulator <emulator>` to run
  them.

Below the platform, each driver has a host implementation:

| Device | In the simulator |
| --- | --- |
| Display, buttons, touchscreen | An SDL window, the keyboard and the mouse |
| Serial console | The terminal, or a TCP port |
| External flash | A file, kept across runs |
| RTC | The host clock |
| Accelerometer | A watch lying still, face up |
| Battery | Fixed values |
| Ambient light | Always dark |
| Vibration | Does nothing |
| Microphone | Silent |
| Bluetooth | A fake controller, or a real one over an HCI UART |

Log messages are not hashed in native builds, so the console shows them
without a dictionary.

## Building and running

Install a host C compiler and [SDL2](https://www.libsdl.org), both part of
the dependencies in the {doc}`../../getting_started/index`. Then configure,
build and start the program:

```shell
pbl configure --board native_emery
pbl build
build/pebbleos
```

A reset of the watch restarts the process.

## Interacting

### Buttons and touch

| Key | Button |
| --- | --- |
| Up | Up |
| Down | Down |
| Right, Enter, Space | Select |
| Left, Backspace, Escape | Back |

The mouse drives the touchscreen: click and drag, or scroll with the wheel
or two fingers on a trackpad, which drags a finger from the pointer.

### Console

The terminal is the watch's serial console: logs go there, and as on the
watch, `Ctrl-C` opens the shell prompt and `Ctrl-D` leaves it. `Ctrl-\`
quits.

### Options

`build/pebbleos` takes these options:

- `-f FILE`: file backing the external flash, `pebbleos-flash.bin` in the
  current directory by default. It keeps the watch's state across runs.
- `-r FILE`: system resources installed at boot, the ones from the build by
  default.
- `-s N`: window scale factor.
- `-t SECONDS`: quit after that long.
- `-c PORT`: serve the console on a TCP port, one client at a time, instead
  of the terminal. Its output still goes to stdout.
- `-p PORT`: serve the QEMU serial protocol on a TCP port, where a phone
  connects with libpebble2's QEMU transport (`pebble --qemu`).
- `-b PORT|DEVICE`: with `CONFIG_BT_HCI_UART`, connect the Bluetooth HCI
  UART to an H4 controller: a TCP port on the local host (e.g. Bumble's
  `tcp-server` transport) or a serial device, such as an nRF52840 dongle
  running `hci_uart`, as described for the {doc}`emulator <emulator>`.
- `-S FILE`: save the last frame to a BMP file on quitting.

## Debugging

A fatal error aborts the process, so a debugger stops right there:

```shell
lldb build/pebbleos
gdb build/pebbleos
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

Drivers come in two halves: the top half is
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
