# Drivers

Drivers live under `drivers/`, one directory per driver class, with the
class interface in `include/pbl/drivers/`. Newer classes are built on the
{doc}`device model <device_model>`; board and SoC specific implementations
document their own definitions in the API reference under their class.

Two classes carry design notes worth reading before changing them:

- **Sensors** — {doc}`sensor/index` explains how drivers describe their
  frames instead of converting them, and how batches flow from a hardware
  FIFO to a consumer without copies.
- **Flash** — `drivers/flash/README.md` documents the two flash APIs:
  the main one, and a coredump-only path that must work without OS services.

```{toctree}
:maxdepth: 1

device_model.md
simulators.md
crc/index.md
crypto/index.md
clk/index.md
sensor/index.md
```

The {apidoc}`API reference <drivers>` documents every driver class.
