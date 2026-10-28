# Drivers

Drivers live under `drivers/`, one directory per driver class, with the
class interface in `include/pbl/drivers/`. Newer classes are built on the
{doc}`device model <device_model>`; board and SoC specific implementations
document their own definitions in the API reference under their class.

Two classes carry design notes worth reading before changing them:

- **Accelerometer** — `include/pbl/drivers/accel.h` explains the split
  between the dumb low-level driver and the accel service that owns
  buffering, clients and subsampling, so the same service code runs on any
  accel part.
- **PPG front end** — `include/pbl/drivers/ppg.h` describes the slot and
  receiver model: drivers deliver raw samples, and gain control and the
  heart rate algorithms live in the HRM service (`services/hrm`).
- **Flash** — `drivers/flash/README.md` documents the two flash APIs:
  the main one, and a coredump-only path that must work without OS services.

```{toctree}
:maxdepth: 1

device_model.md
crc/index.md
```

The {apidoc}`API reference <drivers>` documents every driver class.
