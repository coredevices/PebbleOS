# Debugging

The firmware can be debugged live, with a debugger attached to a watch, the
emulator or the native simulator, and after the fact, from its logs and
the coredumps it writes when it crashes.

- {doc}`gdb_tutorial` walks through a GDB session on the command line.
- {doc}`vscode` debugs from Visual Studio Code.

```{toctree}
:hidden:

gdb_tutorial.md
vscode.md
```

## Build options

Debug a development build: release builds (`-DCONFIG_RELEASE=y`) are not
meant for debugging. They leave out the debug console commands, the window
names and the adaptations below to an attached debugger.

:`CONFIG_DEBUGGER_AWARE`:
On by default outside release builds. While a debugger is attached, the
task watchdog only logs the stalls it detects, and the hardware watchdog
is stopped where it would keep counting while the CPU is stopped. The
task watchdog notices the debugger within its check period, so the first
stop after attaching still counts towards the hardware watchdog's
timeout. Debuggers leave debugging enabled when they detach, so this
lasts until the watch reboots.

:`-DCONFIG_WATCHDOG=n`:
Build without the hardware watchdog driver. Software failures then halt
with interrupts disabled for a debugger instead of rebooting, and the task
watchdog only logs the stalls it detects.

:`-DCONFIG_DEBUG_OPTIMIZATIONS=y`:
Compile with `-Og` rather than `-Os`, so fewer variables are optimized out
and stepping follows the source more closely. The firmware grows, so it may
not fit every board.

:`-DCONFIG_DEBUG_INFO_MACROS=y`:
Compile with `-g3` rather than `-g`, so a debugger can expand the
firmware's macros. It costs around 7% of the compile time and makes the
objects, and with them the link, noticeably larger.

:`-DCONFIG_LINKER_MAP=y`:
Write `build/pebbleos.map`, the cross-referenced link map that
`tools/analyze_fw_static_memory_usage.py` reads. It is tens of megabytes
and costs around a fifth of the link.

## Live debugging

{doc}`../../pbl/debug` starts a GDB server for the target, then GDB on the
build's ELF, attached to the running firmware. {doc}`../../pbl/debugserver`
starts the server alone, for an IDE or a GDB of one's own. The board's
{ref}`runner <pbl-runners>` decides how the target is reached:

| Runner | Target | GDB server | Port |
| --- | --- | --- | --- |
| `qemu` | The emulator | A proxy to the emulator's own server | 1233 |
| `openocd` | A watch, through an SWD probe | OpenOCD | 3333 |
| `probe-rs` | A watch, through an SWD probe or a serial-based debugger | probe-rs | 1337 |
| `native` | The native simulator | None: it runs under the host's debugger | |

GDB is the one in the SDK, and starts with the repository's `.gdbinit`,
which loads the {ref}`PebbleOS GDB commands <pebbleos-gdb-commands>`.

### Emulator

The emulator always runs a GDB server. `pbl debug` attaches to it through a
proxy, `tools/qemu/qemu_gdb_proxy.py`, that walks the kernel's thread list,
so `info threads` and per-thread backtraces work. Start the emulator first:

```shell
pbl run      # in one terminal
pbl debug    # in another
```

### Native simulator

`pbl debug` runs the simulator under the host's debugger, LLDB on macOS
and GDB on Linux. It is the quickest way to debug the firmware's logic and
UI: it builds and boots in seconds and the host's tools all work on it.

```shell
pbl configure --board native_emery
pbl build
pbl debug
```

Unlike on a watch:

- Each kernel thread is a host thread, named after it, and only one of
  them runs at a time. The others wait in `posix_park()`, below the frames
  where they switched away.
- The terminal belongs to the debugger, so `Ctrl-C` interrupts the
  firmware. The console moves to TCP port 12345, where
  {doc}`../../pbl/console` attaches.
- Asserts and other fatal errors abort the process, and the debugger stops
  there.
- Rebooting the watch re-executes the program.
- The {ref}`PebbleOS GDB commands <pebbleos-gdb-commands>` do not apply: they
  read the ARM firmware.

{doc}`../emulation/native_simulator` covers the sanitizers, which find
memory errors before they turn into crashes.

### Watches

A watch is reached through its {doc}`programmer <../../programmers/index>`,
with the runners its board lists:

```shell
pbl debug
```

Some SoCs expose their debug port over a UART instead of SWD. Such a
serial-based debugger needs the programmer's serial port, given with
`--tty`, and takes it over from the console: close {doc}`../../pbl/console`
first. On the SF32LB52 the console stays silent after the debugger leaves,
until the watch reboots.

```shell
pbl debug --tty /dev/cu.usbmodem5B7A1355001
```

Debugging is not supported in some SoCs' sleep modes: the SF32LB52 cannot
be reached in deep sleep.

(pebbleos-gdb-commands)=

### PebbleOS GDB commands

`tools/gdb_scripts/gdb_tintin.py`, which `.gdbinit` loads, extends GDB with
a `pbl` command set. `pbl` lists them all; the most useful are:

- `pbl heap` / `pbl heapstats`: parse and profile the heaps
- `pbl sbt`: stack backtrace and stack usage statistics
- `pbl stackwizard`: recover a backtrace from a corrupted stack, e.g.
  after a stack overflow
- `pbl lockstats`: check the mutexes for deadlocks
- `pbl layer-tree`: dump the UI layer hierarchy
- `pbl app_symbols` / `pbl worker_symbols`: load symbols for the currently
  running app or worker, so app crashes can be symbolicated
- `pbl fault_wizard`: after a hard fault, restore the faulting `$sp`/`$lr`/
  `$pc` so `bt` shows the crashing stack
- `pbl log-buffer`: dump log messages still buffered in RAM
- `pbl to_png`: render the framebuffer or a `GBitmap` to a PNG file

## Console and logs

{doc}`../../pbl/console` attaches to the firmware console and dehashes log
messages on the fly (see {doc}`../../subsystems/logging`). The dehashing code
lives in `tools/log_hashing/` and `tools/libs/pebble-loghash/`.

Logs are also persisted to a circular buffer in flash;
`tools/dehash_flash_logs.py` parses and dehashes a dump of that region.

## Coredumps

When the firmware crashes, it writes a coredump to SPI flash. The on-flash
format, a header followed by chunked records including the register set of
every thread, is documented in `fw/kernel/core_dump.c`.

- `tools/analyze_coredump.py <symbols.elf> <coredump>` runs GDB in batch
  mode and prints a full report: backtraces for all threads, registers,
  heap and lock statistics, and build metadata.

A coredump also opens interactively in the SDK's GDB, with the `pbl`
commands above:

```shell
arm-none-eabi-gdb build/pebbleos.elf -ex "core-file <coredump>"
```

## Memory usage analysis

`tools/` contains several ELF analyzers for RAM and flash footprint work,
e.g. `analyze_fw_static_memory_usage.py` and
`analyze_mcu_flash_usage_treemap.py` (renders an interactive treemap of
flash usage).
