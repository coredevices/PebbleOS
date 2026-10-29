# `pbl debug`

Attach GDB to the firmware.

```shell
pbl debug
```

On QEMU boards this attaches to the emulator through a proxy that walks the
kernel's thread list, so `info threads` and per-thread backtraces work. On
a watch it needs a runner with debug support, i.e. OpenOCD; boards flashed
through sftool cannot be debugged this way. {doc}`../development/debugging/index`
describes the GDB extensions the firmware ships with.

## Options

```{pbl-command} debug
```
