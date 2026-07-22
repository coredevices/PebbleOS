---
name: working-with-qemu
description: Use when building, launching, debugging, or capturing screenshots of PebbleOS under QEMU.
---

# Working with QEMU

Read `docs/development/qemu.md` — it documents the full workflow: qemu_*
boards, `./pbl qemu` (monitor sockets, serial ports, uart1.log), `./pbl
console`, `./pbl screenshot`, programmatic key input via `sendkey`, and
`./pbl debug`.

Agent notes:

- Use `./pbl screenshot` to validate UI changes; read the resulting PNG.
- Drive the UI over the socket monitor (`build/qemu-mon.sock`) with
  `sendkey` rather than the interactive QEMU window.
