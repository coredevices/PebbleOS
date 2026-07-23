---
name: working-with-qemu
description: Use when building, launching, debugging, or capturing screenshots of PebbleOS under QEMU.
---

# Working with QEMU

Read `docs/development/qemu.md` — it documents the full workflow: qemu_*
boards, `./pbl qemu` (monitor sockets, serial ports, uart1.log), `./pbl
console`, `./pbl screenshot`, programmatic key input via `sendkey`, touch
injection via `./pbl touch`/`./pbl swipe`, and `./pbl debug`.

Agent notes:

- Use `./pbl screenshot` to validate UI changes; read the resulting PNG.
- Drive the UI over the socket monitor (`build/qemu-mon.sock`) with
  `sendkey` rather than the interactive QEMU window.

## Touch

On touch-capable boards you can inject touch into a running QEMU (`./pbl qemu`
exposes the QMP socket used for injection). Coordinates are screen pixels; the
display size is read from the emulated `pebble-touch` device and scaled
automatically.

```sh
./pbl touch 130 130                          # tap at (130, 130)
./pbl swipe 130 220 130 40                   # swipe up (finger bottom -> top)
./pbl swipe 130 220 130 40 --steps 20 --duration 0.4
```

A tap is a finger down then up; a swipe streams intermediate moves so drag
gestures are seen as continuous. Injection uses the absolute-pointer input
path; multi-touch is not wired up in the device.
