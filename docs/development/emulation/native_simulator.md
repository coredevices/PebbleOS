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
: Each native board is a platform: its display, its buttons and touchscreen
  if it has one, and the same firmware services, UI and apps built into the
  firmware.

  | Board | Platform | Display |
  | --- | --- | --- |
  | `native_flint` | Flint, Pebble 2 Duo | 144 × 168, black and white |
  | `native_emery` | Emery, Pebble Time 2 | 200 × 228, color, touch |
  | `native_gabbro` | Gabbro, Pebble Round 2 | 260 × 260 round, color, touch |

Not the architecture
: The firmware is compiled for the host CPU, with the host compiler and C
  library, and the kernel runs its threads on host threads. That is what
  makes it fast and debuggable. Third-party apps are ARM code, which an
  {ref}`interpreter <native-third-party-apps>` runs.

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
build and start the program, here for Emery:

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
- `-i FILE`: install a `.pbw` app and launch it; repeat for more.

(native-third-party-apps)=

## Third-party apps

The simulator interprets the ARM code of `.pbw` apps and their workers with
[Samsa](https://github.com/coredevices/samsa), a Thumb-2 interpreter, so they
install and run as on a watch. Install them from the command line, or over
the QEMU serial protocol as a phone would:

```shell
build/pebbleos -i app.pbw
build/pebbleos -p 12344
pebble install --qemu localhost:12344 app.pbw
```

In a browser, the page installs a `.pbw` dropped on it, picked with **Open
.pbw…**, or downloaded from a URL, also given as `?pbw=` in the page's
address. The host of that URL has to allow other origins.

Apps call the firmware through the SDK's jump table, using the 32-bit ARM
calling convention, while the firmware is a host program, often a 64-bit one. A thunk
per SDK function, generated from the SDK's `pebble.h` by
`tools/samsa/generate_thunks.py`, converts each call: pointers into the
app's RAM are mapped to the host, firmware objects outside it are handed to
the app as handles, structures whose layout differs are converted field by
field, and function pointers become host trampolines that call back into
the app. Variadic functions are converted by hand in `subsys/samsa`. The
firmware asks the process runtime (`process_management/process_runtime.h`)
for a process's addresses, its SDK table and how to run it, and Samsa
provides one in which processes not built into the firmware are
interpreted.

Some apps read firmware structures the SDK keeps opaque, through copies of
the watch firmware's definitions. Samsa shows them `GBitmap` and the SDK 2
animations as a watch lays them out; other such structures, such as
`Layer`, they see as the host lays them out, and misbehave.

An app that faults, passes a bad pointer or calls an SDK function there is
no thunk for is killed, as on a watch.

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

## In a browser

The `wasm` variant of each native board builds the same virtual watch as
WebAssembly, with [Emscripten](https://emscripten.org):

```shell
pbl configure --board=native_emery/wasm
pbl build
python3 -m http.server --directory build
```

Then open <http://localhost:8000/pebbleos.html>. The watch has the emulator's
frame: click its buttons, or use the arrow keys (left is back, right is
select); on touch platforms the mouse is a finger on the screen. The
firmware log is a click away. The flash lives in memory, so the watch starts
afresh on every load, and there is no shell. The {doc}`/playground` embeds
these builds in the documentation, one per board in `pbl_playground_boards`
in `docs/conf.py`.

Another page can embed a build in a frame and be its phone:

- `?screen` shows the screen alone, filling the frame, with no frame artwork,
  buttons or controls, and takes no input.
- `?serial` gives the parent page the QEMU serial protocol, as `-p` does on
  the command line. The frame posts `{pblReady: true}`, then the watch's bytes
  as `{pblSerial: Uint8Array}`; the parent posts its bytes back the same way,
  and a `.pbw` to install as `{pblInstall: Uint8Array}`. The watch starts
  with no phone: the parent connects as one with a
  `QemuProtocol_BluetoothConnection` message.

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

In a browser there is a single thread. The kernel's posix architecture then
runs kernel threads as Emscripten fibers (`ARCH_POSIX_FIBERS`, with Asyncify)
instead of pthreads, and the host's main loop is the browser's: each turn it
delivers the interrupts, runs the kernel until it idles, and hands the browser
back its thread. A kernel thread that never blocks gives the browser a turn
every 20 ms.

Things the linker script provides on the target, such as the heaps, the app
RAM and the build ID, are plain objects in `soc/posix/memory.c`. Firmware
`malloc()` and `free()` are renamed to the firmware heaps at compile time, as
`--wrap` does on the target.
