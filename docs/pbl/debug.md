# `pbl debug`

Debug the firmware with GDB.

```shell
pbl debug
```

The board's first {ref}`runner <pbl-runners>` with debug support starts its
GDB server, then GDB on the build's ELF, attached to the running firmware
and with the repository's GDB extensions loaded. Quitting GDB stops the
server.

| Runner | Boards | GDB server | Port |
| --- | --- | --- | --- |
| `qemu` | qemu_* | Proxy to the emulator, with the kernel's threads | 1233 |
| `openocd` | asterix | OpenOCD | 3333 |

On QEMU boards the emulator has to be running already, e.g. with
{doc}`run` in another terminal. A server already listening on the port,
e.g. one started with {doc}`debugserver`, is used as it is; `--gdb-port`
picks another port. {doc}`../development/debugging/index` describes the
GDB extensions the firmware ships with.

## Options

```{pbl-command} debug
```
