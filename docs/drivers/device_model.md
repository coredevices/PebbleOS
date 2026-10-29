# Device model

`include/pbl/device.h` defines `struct pbl_device`, the common core of every
hardware device the firmware drives: an on-chip controller such as a GPIO
port or an I2C bus, a peripheral behind a bus such as a PMIC, or a controller
a peripheral exposes in turn, such as the GPIOs of that PMIC. The core is
implemented in `drivers/base/`.

## Structure

The model follows the Linux kernel: a device class embeds `struct pbl_device`
in its own struct, a driver embeds the class struct in a driver-specific one,
and `container_of()` walks back out.

```{mermaid}
classDiagram
  direction LR
  class pbl_device {
    name, init, parent, deps, state
    include/pbl/device.h
  }
  class pbl_gpio_port {
    class: ops vtable
    include/pbl/drivers/gpio.h
  }
  class pbl_gpio_nrf5 {
    driver: port index
    include/pbl/drivers/gpio/nrf5.h
  }
  pbl_gpio_nrf5 *-- pbl_gpio_port : embeds
  pbl_gpio_port *-- pbl_device : embeds
```

Devices are `const` and live in flash. The only RAM is what the device points
to: `struct pbl_device_state` for the core, plus whatever the class or driver
needs.

Hardware relationships are written in C: a board or SoC file instantiates
devices with the driver's `PBL_<DRIVER>_DEFINE()` macro and points the
devices that use them at them. Consumers take a pointer to the class struct
and never see the driver struct.

## Instantiation

A driver provides a `PBL_<DRIVER>_DEFINE(sym, ...)` macro built from three
core pieces:

- `PBL_DEVICE_STATE_DEFINE(sym)` allocates the runtime state;
- `PBL_DEVICE_INIT(sym, name, init, parent, deps)` initializes the embedded
  `struct pbl_device`;
- `PBL_DEVICE_REGISTER(sym, dev)` adds a pointer to the device to the device
  table, the `.pbl_devices` section (`drivers/base/device.ld`).

Things that are not devices of their own (a pin, a bus address) are plain
structs that point at their device.

## Initialization

`pbl_device_init_all()`, called at the top of `init_drivers()` in
`fw/main.c` once the kernel services drivers rely on are up, walks the device
table and calls `pbl_device_init()` on each entry. Ordering is by dependency,
not by table position:

- a device's `parent` is initialized first: the bus a sensor sits on, or the
  multi-function device it is a function of. A parent may also bring its
  children up from its own init with `pbl_device_init_children()`;
- a driver's init calls `pbl_device_init()` on the other devices it uses, so
  structural dependencies are not written down twice;
- anything else, such as a supply that has to be up first, goes in the
  instance's `PBL_DEVICE_DEPS()`.

This is a synchronous take on Linux's deferred probe and `fw_devlink`.
`pbl_device_init()` is idempotent, so a device is initialized exactly once
however many others depend on it. A failed init marks the device
`PBL_DEVICE_FAILED`, and its dependents fail with `-ENODEV` without running
their own init. An init returning `-ENODEV` means the device is not present,
which is not logged as an error. A dependency cycle asserts.

Runtime APIs assert that their device is ready and never initialize on
demand: bring-up is deterministic and happens in one place. Code that runs
before `init_drivers()`, such as the boot splash bringing up the display,
calls `pbl_device_init()` on the devices it uses itself.

Hardware needed before the kernel runs, such as clocks
(`pbl_soc_early_init()`), stays outside the model, as in Linux.

## Multi-function devices

A chip with several unrelated functions, such as the nPM1300 PMIC (charger,
GPIOs, regulators), is one core device plus one child device per function
(cell), laid out as in Linux: the core lives in `drivers/mfd` and each cell
in the subsystem of its class.

| Part | Driver | Device |
| --- | --- | --- |
| Core | `drivers/mfd/npm1300.c` | `struct pbl_npm1300` |
| Charger | `drivers/pmic/npm1300.c` | `struct pbl_npm1300_charger` |
| GPIOs | `drivers/gpio/npm1300.c` | `struct pbl_npm1300_gpio` (a `struct pbl_gpio_port`) |
| Rails | `drivers/regulator/npm1300.c` | `struct pbl_npm1300_regulator` (a `struct pbl_regulator`) |

The core sits on its I2C bus and owns the register access under a lock, so
a read-modify-write in one cell cannot interleave with another. Its init
ends with `pbl_device_init_children()`, so a device that depends on the
PMIC can rely on its cells being up. Cells reach the core with
`pbl_npm1300_from_cell()`.

Board-level facts live in the board, which instantiates the core and the
cells it uses: each rail is a `PBL_NPM1300_REGULATOR_DEFINE()` with its
voltage, mode and whether it is always on; consumers point at the regulator
(the `vdd` of a PDM microphone) and switch it through its use count, and a PMIC pin is an
ordinary `PBL_GPIO(&s_npm1300_gpio.port, ...)`.

## Reporting to consumers

Drivers never call into the services that consume their data. A class that
produces data, such as the accelerometer or the magnetometer, lets a consumer
register a listener (`pbl_accel_set_listener()`, `pbl_mag_set_listener()`),
and its drivers report through the class (`pbl_accel_report_samples()`,
`pbl_accel_report_event()`, `pbl_mag_report_sample()`), as IIO producers push
to buffers and events on Linux. Shake and double tap are accelerometer
events, not input events: they carry the axis and direction they were
detected on.

These classes serialize their driver, their reports and their listener with
one recursive device lock. A driver runs its interrupt bottom half under it,
and the listener may take it around its own state, so there is no second
lock to order against it.

## Adding a driver

1. Define the driver struct embedding the class struct, the class ops and
   an init that recovers the driver struct with
   `container_of(dev, const struct <driver>, <class>.dev)`.
2. Provide the `PBL_<DRIVER>_DEFINE()` macro.
3. Instantiate the device in the board or SoC file.
