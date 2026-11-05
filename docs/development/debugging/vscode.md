# Visual Studio Code

Visual Studio Code debugs the firmware through the same GDB servers as
{doc}`../../pbl/debug`: {doc}`../../pbl/debugserver` starts one, and the
[Cortex-Debug](https://marketplace.visualstudio.com/items?itemName=marus25.cortex-debug)
extension attaches GDB to it. The native simulator is debugged as any host
program, with the
[C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
extension.

The configurations below go in `.vscode/launch.json` in the checkout.
Cortex-Debug needs the SDK's GDB, the one the build found:

```shell
grep PBL_GDB build/CMakeCache.txt
```

## Emulator

Start the emulator and the GDB server in two terminals:

```shell
pbl run
pbl debugserver
```

Then attach from the **Run and Debug** view:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "PebbleOS (emulator)",
      "type": "cortex-debug",
      "request": "attach",
      "servertype": "external",
      "gdbTarget": "localhost:1233",
      "gdbPath": "<PBL_GDB>",
      "executable": "${workspaceFolder}/build/pebbleos.elf",
      "cwd": "${workspaceFolder}"
    }
  ]
}
```

The **Call Stack** view lists the kernel's threads, and breakpoints are set
in the editor's margin:

```{figure} images/vscode-emulator.webp
Stopped at a breakpoint, with the locals and the kernel's threads.
```

## Watch

The same configuration attaches to a watch, with the port of the board's
runner: 3333 for OpenOCD, 1337 for probe-rs. A serial-based debugger also
needs the programmer's port:

```shell
pbl debugserver
pbl debugserver --tty /dev/cu.usbmodem5B7A1355001
```

```json
{
  "name": "PebbleOS (watch)",
  "type": "cortex-debug",
  "request": "attach",
  "servertype": "external",
  "gdbTarget": "localhost:1337",
  "gdbPath": "<PBL_GDB>",
  "executable": "${workspaceFolder}/build/pebbleos.elf",
  "cwd": "${workspaceFolder}",
  "svdFile": "<SVD>"
}
```

`svdFile` is optional: an SVD file describes the SoC's peripherals for the
**Peripherals** view, e.g. the nRF52840's in
`third_party/hal_nordic/nrfx/mdk/nrf52840.svd`.

When the board's `support/probe-rs.yml` has `gdb-flash`, GDB needs that
flash region too, or it patches software breakpoints into flash; e.g. on
the SF32LB52:

```json
"preAttachCommands": [
  "set mem inaccessible-by-default off",
  "mem 0x12000000 0x20000000 ro"
]
```

## Native simulator

```json
{
  "name": "PebbleOS (native)",
  "type": "cppdbg",
  "request": "launch",
  "program": "${workspaceFolder}/build/pebbleos",
  "args": ["-f", "${workspaceFolder}/build/pebbleos-flash.bin", "-c", "12345"],
  "cwd": "${workspaceFolder}",
  "MIMode": "lldb",
  "sourceFileMap": { "${workspaceFolder}/build": "${workspaceFolder}" }
}
```

Use `"MIMode": "gdb"` on Linux. `-c 12345` moves the console to TCP, where
{doc}`../../pbl/console` attaches. The build names sources relative to the
build directory, which `sourceFileMap` maps back to the checkout.
