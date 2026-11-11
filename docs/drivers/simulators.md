# Simulators

A simulator is a driver of a class with no hardware behind it: tests set what
it reads, feed it data or watch what it is told to do through a simulator
API next to its class. The emulator and native boards use them for the
devices they do not emulate, and any board can swap one in for testing.

| Class | Kconfig | Device | API |
| --- | --- | --- | --- |
| Accelerometer | `ACCEL_SIM` | `SIM_ACCEL` | `pbl/drivers/imu/sim.h` |
| Ambient light | `ALS_SIM` | `SIM_ALS` | `pbl/drivers/als/sim.h` |
| Audio capture | `AUDIO_SIM_MIC` | `SIM_MIC` | `pbl/drivers/audio/sim.h` |
| Battery | `BATTERY_SIM` | | `pbl/drivers/battery/sim.h` |
| Temperature | `TEMP_SIM` | `SIM_TEMP` | `pbl/drivers/temp/sim.h` |
| Vibration | `VIBE_SIM` | `SIM_VIBE` | `pbl/drivers/vibe/sim.h` |

Each simulator is a single device its driver defines, with no devicetree
node. A board or an application selects it for a role in its `roles.yml`
(see [](../development/build_system/devicetree.md)) and enables it in
Kconfig:

```yaml
accel:
  symbol: SIM_ACCEL
  include: pbl/drivers/imu/sim.h
```

Simulators know nothing of where their data comes from. On the emulator and
native boards, the QEMU host channel (`pebble/comm/qemu/serial.c`) decodes
the host's messages and calls the simulator APIs, so the host tools drive
them unchanged. Unit tests call the same APIs directly.
