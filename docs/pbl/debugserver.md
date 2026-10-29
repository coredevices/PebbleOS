# `pbl debugserver`

Start a GDB server for the firmware.

```shell
pbl debugserver
```

Runs the GDB server of the board's {ref}`runner <pbl-runners>` in the
foreground, the one {doc}`debug` starts, until `Ctrl-C`. Anything that
speaks the GDB remote protocol attaches to it: GDB, an IDE, or
`pbl debug` from another terminal, which then leaves the server running
when GDB quits.

The server listens on the runner's port, see {doc}`debug`, or on
`--gdb-port`:

```shell
pbl debugserver --gdb-port 2331
```

The `native` runner has no GDB server: the simulator runs under the
debugger itself.

## Options

```{pbl-command} debugserver
```
