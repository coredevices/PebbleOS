# `pbl debug`

Debug the firmware with GDB.

```shell
pbl debug
```

The board's first {ref}`runner <pbl-runners>` with debug support starts its
GDB server, then GDB on the build's ELF, attached to the running firmware
and with the repository's GDB extensions loaded. Quitting GDB stops the
server.

| Runner | GDB server | Port |
| --- | --- | --- |
| `qemu` | Proxy to the emulator, with the kernel's threads | 1233 |
| `openocd` | OpenOCD | 3333 |
| `probe-rs` | probe-rs | 1337 |
| `native` | None: runs the simulator under gdb or lldb | |

On QEMU boards the emulator has to be running already, e.g. with
{doc}`run` in another terminal. A server already listening on the port,
e.g. one started with {doc}`debugserver`, is used as it is; `--gdb-port`
picks another port.

Platforms with a serial-based debugger, e.g. over the programmer's debug
UART, need its port, given with `--tty`:

```shell
pbl debug --tty /dev/cu.usbmodem5B7A1355001
```

`--probe` selects one of several debug probes, as `VID:PID[:SERIAL]`.
{doc}`../development/debugging/index` covers the
setup, and {doc}`../development/debugging/gdb_tutorial` walks through a
session.

## Options

```{pbl-command} debug
```
