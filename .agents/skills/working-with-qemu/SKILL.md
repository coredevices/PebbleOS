---
name: working-with-qemu
description: Use when building, launching, debugging, or capturing screenshots of PebbleOS under QEMU.
---

# Working with QEMU

Read `docs/development/emulation/emulator.md` — it documents the full workflow: qemu_*
boards, `./pbl run` (monitor sockets, serial ports, uart1.log), `./pbl
console`, `./pbl emulator screenshot`, programmatic key input via `sendkey`, touch
injection via `./pbl emulator touch`/`./pbl emulator swipe`, and `./pbl debug`.

Agent notes:

- Use `./pbl emulator screenshot` to validate UI changes; read the resulting PNG.
- Drive the UI over the socket monitor (`build/qemu-mon.sock`) with
  `sendkey` rather than the interactive QEMU window.
- Phone-dependent features get their data from `./pbl emulator feed <feed>` (e.g.
  `./pbl emulator feed weather`); feeds live in `tools/libs/pbl-cli/pbl/feeds/`.

## Touch

On touch-capable boards you can inject touch into a running QEMU (`./pbl run`
exposes the QMP socket used for injection). Coordinates are screen pixels; the
display size is read from the emulated `pebble-touch` device and scaled
automatically.

```sh
./pbl emulator touch 130 130          # tap at (130, 130)
./pbl emulator swipe 130 220 130 40   # swipe up (finger bottom -> top)
./pbl emulator swipe 130 220 130 40 --duration 0.4
```

A tap is a finger down then up; a swipe streams intermediate moves so drag
gestures are seen as continuous. Injection uses the absolute-pointer input
path; multi-touch is not wired up in the device.
