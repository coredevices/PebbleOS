# Emulator and simulator

PebbleOS runs on a desktop in two ways, without a watch:

| | {doc}`Emulator <emulator>` | {doc}`Native simulator <native_simulator>` |
| --- | --- | --- |
| Runs | The ARM firmware, in QEMU | The firmware compiled for the host |
| Boards | `qemu_flint`, `qemu_emery`, `qemu_gabbro` | `native_emery` |
| Platforms | Flint, Emery, Gabbro | Emery |
| Third-party apps | Yes: `.pbw` files install and run | No: they are ARM code |
| Speed | Boots as on a watch | Builds and boots much faster |
| Debugging | GDB, through QEMU | The host's debugger, sanitizers and profilers |
| Integration tests | Yes | Yes |

Both reproduce a platform's display, buttons and touch, and neither
exercises the real hardware: use a watch for drivers, power and radio
behaviour.

Use the emulator to run apps, to check the firmware as it ships for each
platform, and for anything ARM-specific. Use the native simulator for fast
iterations on the firmware's logic and UI, and to debug them with host
tools.

```{toctree}
:hidden:

emulator.md
native_simulator.md
```
