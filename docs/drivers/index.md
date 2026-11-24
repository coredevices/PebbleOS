# Drivers

A driver class is the interface of one kind of device, such as a display, an
I2C bus or an accelerometer: its API in `include/pbl/drivers/`, its core and
its drivers in `drivers/<class>/`. Code that uses a device goes through its
class, whatever the part behind it. Classes are built on the
{doc}`device model <device_model>`, which gives every device its
initialization, dependencies and power management.

A board declares its devices in C, or has them generated from its
devicetree, in which case a driver is built when the board, an add-on or the
application has an enabled node with its compatible (see
{ref}`device-model-devicetree`). The class itself is enabled by the
application, such as `CONFIG_DISPLAY=y`, or selected by the drivers and code
that use it. Code reaches the devices it needs by their role, such as
`DISPLAY` or `FLASH` (see {ref}`device-model-roles`).

{doc}`Simulators <simulators>` are drivers with no hardware behind them, which
tests and emulated boards use in place of a device.

```{toctree}
:hidden:

device_model.md
simulators.md
```

## Classes

```{toctree}
:maxdepth: 2

bus.md
clocks_power.md
memory.md
graphics.md
sensors.md
sound.md
supply.md
system.md
```
