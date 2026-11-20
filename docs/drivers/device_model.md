# Device model

`include/pbl/device.h` defines `struct pbl_device`, the common core of every
hardware device the firmware drives: an on-chip controller such as a GPIO
port or an I2C bus, a peripheral behind a bus such as a sensor, or a
controller a peripheral exposes in turn, such as the GPIOs of a PMIC. The core
is implemented in `drivers/base/`.

This page describes the model end to end: what devices, classes and drivers
are, how a device comes to exist, how it is brought up and how code finds it.

## Layers

Three layers take part in getting from the hardware to the code that uses it:

- **the device model**, the core: the devices, the drivers behind them and
  the classes their APIs belong to;
- **the devicetree**, optional: a description of the hardware the devices are
  generated from;
- **roles**: which device serves which purpose.

The device model needs nothing but C. Each device is declared in code, from
what is known about the hardware: where its registers are, which interrupt it
uses, which other devices it relies on. This is how the native boards work,
as they have no hardware behind them to describe.

The devicetree describes the hardware with the standard devicetree bindings,
and generates the same device declarations a board would otherwise write by
hand, along with what is tedious and easy to get wrong by hand:

- the connections between devices, and from them the order they are brought
  up in;
- memory: register bases, RAM regions, flash partitions;
- which drivers are built, from the nodes that are enabled.

Drivers do not know which of the two declared their devices.

Roles work the same with or without a devicetree: they say which device
serves a purpose, such as the display or the accelerometer, so code that
needs "the accelerometer" does not name a particular device.

```{mermaid}
flowchart TB
  dt["Devicetree (optional)<br/>tree, driver specs"]
  c["C<br/>boards, drivers, simulators"]
  model["Device model<br/>devices, drivers, classes"]
  roles["Roles<br/>roles.yml"]
  user["Consumers"]
  dt -->|"instance macro calls"| model
  c -->|"instance macro calls"| model
  model -->|"a device by name"| user
  roles -->|"the device for a purpose"| user

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class dt,c app
  class roles system
  class model,user base
```

## Devices, classes and drivers

The model is layered by embedding: a device class embeds `struct pbl_device`
in its own struct, a driver embeds the class struct in a driver-specific one,
and `PBL_CONTAINER_OF()` walks back out.

The rest of this page follows one example throughout: an imaginary
temperature sensor, `vendor,temp`, at address `0x48` of an I2C bus, powered
from a regulator. It is a device of the temperature class, `drivers/temp/`,
driven by a `temp_vendor` driver.

```{mermaid}
classDiagram
  direction LR
  class pbl_device {
    name, init, parent, cls, state
    include/pbl/device.h
  }
  class pbl_temp {
    class: ops, state
    include/pbl/drivers/temp.h
  }
  class pbl_temp_vendor {
    driver: state, i2c, supply, resolution_bits
    include/pbl/drivers/temp/vendor.h
  }
  pbl_temp_vendor *-- pbl_temp : embeds
  pbl_temp *-- pbl_device : embeds
```

Consumers take a pointer to the class struct and never see the driver
struct. Things that are not devices of their own (a pin, a bus address) are
plain structs that point at their device, as the sensor's `struct
pbl_i2c_dev`.

### ROM and RAM

A device is `const` and lives in ROM. Everything that describes it is known
at build time: its init, parent and class, the driver's ops, its bus and
address, its supply, the properties of the part. RAM holds only what changes
while the firmware runs, in state structs the device points to, one per layer
that needs it:

```{mermaid}
flowchart LR
  subgraph rom["ROM: dt_temp0"]
    direction TB
    dev["pbl_device<br/>init, parent, class"]
    temp["pbl_temp<br/>ops"]
    drv["pbl_temp_vendor<br/>bus, address, supply"]
  end
  subgraph ram["RAM"]
    direction TB
    ds["device state<br/>status"]
    vs["driver state<br/>configuration"]
  end
  dev --> ds
  drv --> vs

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class ds,vs app
  class dev,temp,drv base
```

- `struct pbl_device_state`, from `PBL_DEVICE_STATE_DEFINE()`: whether the
  device is initialized, and the result of its init.
- The class state, from `PBL_<CLASS>_STATE_DEFINE()`, for a class that needs
  one: the temperature class has none, while a bus class keeps what is shared
  by every device on the bus.
- The driver state, defined by the instance macro: what the driver needs to
  remember about the hardware, here the configuration it wrote.

A class or driver with nothing to keep has no state. Keeping the rest in
ROM costs no RAM per device, means no code fills it in at boot, and leaves
nothing a stray write can change: a device's description is what was built.

### Classes

A class is the software model of a kind of device: a temperature sensor, a
GPIO port, a display. It lives in `drivers/<class>/`, with its interface in
`include/pbl/drivers/<class>.h`, and is made of:

- **The class struct**, embedding `struct pbl_device` as `dev`, with a pointer
  to the driver's ops:

  ```c
  struct pbl_temp_ops {
    /** Optional. Bring the hardware up. */
    int (*init)(const struct pbl_temp *temp);
    /** Read the temperature, in millidegrees Celsius. */
    int (*read)(const struct pbl_temp *temp, int32_t *millicelsius);
  };

  struct pbl_temp {
    struct pbl_device dev;
    const struct pbl_temp_ops *ops;
  };
  ```

- **The class object**, `PBL_DEVICE_CLASS_DEFINE(pbl_temp_class, "temp")`,
  which every device of the class points at, so code can tell what a device is
  and walk the devices of a class with `PBL_DEVICE_CLASS_FOREACH()`.
- **State and init macros** for drivers to build their instances with:
  `PBL_<CLASS>_STATE_DEFINE(sym)` allocates the device state, and the class
  state if the class has one, and
  `PBL_<CLASS>_INIT(sym, name, parent, ops, ...)` fills the class struct,
  wrapping `PBL_DEVICE_INIT_CLASS()`.
- **The API**, functions taking the class struct. They check the device is
  ready and call the driver:

  ```c
  int pbl_temp_read(const struct pbl_temp *temp, int32_t *millicelsius) {
    if (!pbl_device_is_ready(&temp->dev)) {
      return -ENODEV;
    }

    return temp->ops->read(temp, millicelsius);
  }
  ```

  Properties fixed by the hardware are read through getters, such as
  `pbl_display_get_props()` or `pbl_flash_get_geometry()`, not by reaching
  into the class struct.
- **The device init**, `pbl_<class>_dev_init()`, set as the device's `init`
  by the class init macro: it calls the driver's `init` op, if any. A class whose drivers bring their devices up in their own way
  takes the device init from the driver instead.
- **Roles**: pointers the class header declares, one per purpose a device of
  the class may serve, as `DIE_TEMP`; see {ref}`device-model-roles`.
- **Kconfig**, in `drivers/<class>/Kconfig`: a `menuconfig` for the class,
  sourced from `drivers/Kconfig`, enclosing its drivers, each in its own
  `Kconfig.<driver>` next to it. A class that logs
  defines its log module there, `module = DRIVER_<CLASS>` with
  `subsys/logging/Kconfig.template.log_level`; the class source defines it
  with `PBL_LOG_MODULE_DEFINE(driver_<class>, ...)` and its drivers declare
  it with `PBL_LOG_MODULE_DECLARE()`.
- **A shell command**, optional, in `drivers/<class>/shell.c`: one command
  per class, under `<CLASS>_SHELL`, which defaults to `DRIVER_SHELL`.
- **A simulator**, optional: a driver with no hardware behind it, driven
  through an API next to the class; see [](simulators.md).
- **A class file**, `<class>.class.yaml`, giving the build its type, its
  header and its roles, and how devicetree nodes refer to its devices; see
  {ref}`devicetree-classes`.

### Drivers

A driver implements a class for one kind of hardware. It lives in
`drivers/<class>/<driver>.c`, with a public header in
`include/pbl/drivers/<class>/<driver>.h` holding:

- **The driver struct**, embedding the class struct first, followed by its
  state and what the driver needs from the hardware description:

  ```c
  struct pbl_temp_vendor_state {
    /** Configuration register, as last written. */
    uint8_t config;
  };

  struct pbl_temp_vendor {
    /** Temperature sensor, child of the I2C bus. */
    struct pbl_temp temp;
    /** Driver state. */
    struct pbl_temp_vendor_state *state;
    /** I2C device. */
    struct pbl_i2c_dev i2c;
    /** Supply. */
    const struct pbl_regulator *supply;
    /** Conversion resolution, in bits. */
    uint8_t resolution_bits;
  };
  ```

- **The instance macro**, `PBL_<DRIVER>_DEFINE(sym, ...)`, taking what the
  hardware description gives, never an index into a table in the driver. It
  defines the state of each layer in RAM and the device in ROM; the bus is
  the device's parent:

  ```c
  #define PBL_TEMP_VENDOR_DEFINE(sym, _bus, _addr, _supply, _resolution_bits)              \
    PBL_TEMP_STATE_DEFINE(sym);                                                          \
    static struct pbl_temp_vendor_state sym##_vendor_state;                              \
    PBL_DEVICE_INSTANCE(sym, struct pbl_temp_vendor, struct pbl_temp, temp) {            \
      .temp = PBL_TEMP_INIT(sym, "temp-vendor", &(_bus)->dev, &pbl_temp_vendor_ops),     \
      .state = &sym##_vendor_state, .i2c = PBL_I2C_DEV(_bus, _addr), .supply = (_supply), \
      .resolution_bits = (_resolution_bits),                                             \
    }
  ```

- the ops and interrupt handlers the macro names.

The ops recover the driver struct from the class struct, and only ever write
to the state:

```c
static int prv_init(const struct pbl_temp *temp) {
  const struct pbl_temp_vendor *vendor = PBL_CONTAINER_OF(temp, const struct pbl_temp_vendor, temp);
  int ret;

  ret = pbl_regulator_enable(vendor->supply);
  if (ret != 0) {
    return ret;
  }

  vendor->state->config = VENDOR_CONFIG_RES(vendor->resolution_bits);

  return pbl_i2c_reg_write_byte(&vendor->i2c, VENDOR_REG_CONFIG, vendor->state->config);
}

const struct pbl_temp_ops pbl_temp_vendor_ops = {
  .init = prv_init,
  .read = prv_read,
};
```

A driver with an interrupt connects it in its instance macro, with a priority
from Kconfig.

## Instances

`PBL_DEVICE_INSTANCE(sym, type, class_type, member)` defines a device and
registers it in the device table, the `devices` iterable section. The device
has two names: `sym`, with the type of its class, for any code, and
`sym##_inst`, with the type of the driver, for the driver. The class member
has to come first in the driver struct, which the macro checks.

### From C

An instance is a call to the driver's instance macro, with the device's
symbol and its hardware description. Here the bus `i2c1` and the regulator
`ldo1` are devices defined the same way:

```c
PBL_TEMP_VENDOR_DEFINE(temp0, &i2c1, 0x48, &ldo1, 12);
PBL_DEVICE_DEPENDS(temp0, &temp0.dev, &ldo1.dev);
```

Code then reaches the device as `temp0`, a `struct pbl_temp`, from anywhere
that declares it. The bus is the device's parent, so it comes up first; the
supply is not, so `PBL_DEVICE_DEPENDS()` says it has to be up first too; see
{ref}`device-model-init`.

Devices of drivers with nothing to describe, as the simulators and the
drivers of the native boards, are defined by the driver itself and named by a
symbol its header declares:

```c
PBL_TEMP_STATE_DEFINE(pbl_temp_sim);
const struct pbl_temp pbl_temp_sim = PBL_TEMP_INIT(pbl_temp_sim, "temp", nullptr, &s_ops);
PBL_DEVICE_REGISTER(pbl_temp_sim, &pbl_temp_sim.dev);
```

### From the devicetree

A board with a devicetree describes the sensor as a node, and the build
writes the same calls, as `dt_temp0`; see {ref}`device-model-devicetree`.

(device-model-devicetree)=

## The devicetree layer

The devicetree layer writes, from a description of the hardware, the C a
board would otherwise write by hand. `tools/devicetree/dtmap.py` does it in
two steps, both at build time; nothing of the tree is stored or parsed by
the firmware.

```{mermaid}
flowchart TB
  tree["Board tree<br/>*.dts, *.dtsi, overlays"]
  dtb["board.dtb, Kconfig.dt"]
  cfg[".config"]
  specs["Driver specs, class files<br/>*.dtmap.yaml, *.class.yaml"]
  gen(("dtmap.py"))
  out["instances/*.inc, devices.h,<br/>partitions.h, regs.h, ..."]
  tree -->|"cpp, dtc"| dtb -->|"Kconfig"| cfg --> gen
  dtb --> gen
  specs --> gen
  gen --> out

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class tree app
  class gen,specs system
  class dtb,cfg,out base
```

1. **Configure.** Before Kconfig runs, the tree is compiled with the C
   preprocessor and `dtc` into `board.dtb`, and `Kconfig.dt` gets one
   `DT_HAS_<compatible>_ENABLED` symbol per compatible with an enabled node.
   Drivers default to these, so the tree decides which drivers are built.
2. **Generate.** At build time, `dtmap.py` reads the compiled tree, the
   resolved `.config`, the specs of the drivers and the class files, and,
   for every enabled node whose driver is enabled, turns the node into a call
   to the driver's instance macro, argument by argument, as the spec says.

Taking the sensor of this page, the board's tree describes it under its bus:

```dts
&i2c1 {
	status = "okay";

	temp0: temperature-sensor@48 {
		compatible = "vendor,temp";
		reg = <0x48>;
		vdd-supply = <&ldo1>;
		vendor,resolution-bits = <12>;
	};
};
```

and the driver's spec, `drivers/temp/vendor.dtmap.yaml`, says what each
argument of its instance macro is made of; see {ref}`devicetree-specs` for
every kind:

```yaml
compatible: vendor,temp
config: TEMP_VENDOR
class: temp
define: PBL_TEMP_VENDOR_DEFINE
args:
  - parent: i2c                 # _bus: what the parent node provides
  - reg: 0                      # _addr: the first reg entry
  - supply: vdd                 # _supply: the regulator of vdd-supply
  - prop: vendor,resolution-bits
    default: 12                 # _resolution_bits
```

Here is what is generated from them, next to what a board without a tree
writes instead.

**Driver selection.** `devicetree/Kconfig.dt`, in the build directory, has

```kconfig
config DT_HAS_VENDOR_TEMP_ENABLED
	def_bool y
```

which turns the driver on through its default:

```kconfig
config TEMP_VENDOR
    bool "Vendor temperature sensor"
    default DT_HAS_VENDOR_TEMP_ENABLED
    select I2C
```

In C, the board enables it in its `defconfig`, with `CONFIG_TEMP_VENDOR=y`.

**Instances.** `devicetree/instances/vendor_temp.inc` has one call per
enabled node, with the symbol `dt_<label>`, followed by the devices the node
refers to that are not its parent, so they are brought up first; see
{ref}`devicetree-init-order`:

```c
/* Generated from the board devicetree. */

#include <devicetree/devices.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/regulator.h>

PBL_TEMP_VENDOR_DEFINE(dt_temp0, &dt_i2c1, 0x48, &dt_ldo1, 12);
PBL_DEVICE_DEPENDS(dt_temp0, &dt_temp0.dev, &dt_ldo1.dev);
```

The driver includes it at its end, so the instances are compiled with it; the
file exists, empty, when the board has no such node or no tree:

```c
#include PBL_DT_INSTANCES(vendor_temp)
```

In C, the board writes the same two lines in one of its sources, with names
of its own:

```c
PBL_TEMP_VENDOR_DEFINE(temp0, &i2c1, 0x48, &ldo1, 12);
PBL_DEVICE_DEPENDS(temp0, &temp0.dev, &ldo1.dev);
```

**Declarations.** `devicetree/devices.h` declares every device with a driver,
with the type of its class, so any code can name it, and lists the enabled
nodes left without a driver:

```c
extern const struct pbl_i2c_bus dt_i2c1;
extern const struct pbl_regulator dt_ldo1;
extern const struct pbl_temp dt_temp0;
```

In C, the board declares its devices in a header of its own.

### One node, several devices

A node describes a piece of hardware, not a device: one block may do several
things, each the job of a different class. A clock controller that also
resets the peripherals it clocks is one node:

```dts
rcc: clock-controller@40000000 {
	compatible = "vendor,soc-rcc";
	reg = <0x40000000 0x1000>;
	#clock-cells = <1>;
	#reset-cells = <1>;
};
```

and two drivers bind to it, each with its own spec and class, giving one
device each:

```yaml
# drivers/clk/soc.dtmap.yaml
compatible: vendor,soc-rcc
config: CLK_SOC
class: clk
define: PBL_CLK_SOC_DEFINE
args:
  - name: true
  - reg: 0
```

```yaml
# drivers/reset/soc.dtmap.yaml
compatible: vendor,soc-rcc
config: RESET_SOC
class: reset
symbol: "{sym}_reset"
define: PBL_RESET_SOC_DEFINE
args:
  - name: true
  - reg: 0
```

Each driver gets its own instance file, named after the compatible and the
class, and includes its own:

```c
/* devicetree/instances/vendor_soc_rcc_clk.inc */
PBL_CLK_SOC_DEFINE(dt_rcc, "rcc", 0x40000000);

/* devicetree/instances/vendor_soc_rcc_reset.inc */
PBL_RESET_SOC_DEFINE(dt_rcc_reset, "rcc", 0x40000000);
```

A reference to the node resolves to the device of the class it asks for:
`clocks = <&rcc 12>` to the clock device, `resets = <&rcc 12>` to the reset
device, so a consumer never knows they share a node. In C, a board defines
the same two devices with the two instance macros.

A node gives several devices in two other ways:

- one driver implementing several classes for it, with `class` as a list,
  as a GPIO port that is also the interrupt controller of its pins;
- one device per child of a node of named children, as the regulators of a
  PMIC; see {ref}`device-model-mfd`.

The tree also gives what is not a device, from the same description:
flash partitions, register bases of blocks used before the devices are up,
CPU idle states, reserved memory; see
[](../development/build_system/devicetree.md).

(device-model-roles)=

## Roles

Code that needs a kind of device rather than a particular one, as "the
accelerometer", uses its role: a pointer to the class struct, named after the
role in capitals and declared by the class header, such as `ACCEL`. Roles
are their own layer, independent of how devices are defined:

- each class lists the roles of its devices in its class file, with the
  Kconfig symbol that needs each, if any:

  ```yaml
  roles:
    die_temp: TEMP
  ```

- a board gives each role it has a device in `boards/<board>/roles.yml`, as a
  devicetree label or as a C symbol with the header declaring it:

  ```yaml
  die_temp: temp0          # a devicetree node
  accel:                   # a device defined in C
    symbol: SIM_ACCEL
    include: pbl/drivers/imu/sim.h
  ```

- revisions, add-ons and applications change them in roles files of their
  own, so an application can swap the device behind a role, for a simulator
  for instance.

`tools/devicetree/dtmap.py` turns them into `devicetree/roles.c`, defining
each role whose symbol is enabled:

```c
const struct pbl_sensor *const TEMP = &dt_temp0;
const struct pbl_accel *const ACCEL = SIM_ACCEL;
```

An enabled symbol whose role has no device fails the build. The override
order and the roles of each class are in {ref}`devicetree-roles`.

(device-model-init)=

## Initialization

The kernel calls `pbl_device_init_all()` on the main thread, right before
`main()`: it walks the device table and calls `pbl_device_init()` on each
entry. An application therefore starts with its devices ready, and drivers
may only rely on the kernel in their init. Ordering is by dependency, not by
table position:

- a device's `parent` is initialized first: the bus a sensor sits on, or the
  multi-function device it is a function of. A parent may also bring its
  children up from its own init with `pbl_device_init_children()`;
- a driver's init calls `pbl_device_init()` on the other devices it uses, so
  structural dependencies are not written down twice;
- anything else, such as a supply that has to be up first, is listed next to
  the instance with `PBL_DEVICE_DEPENDS()`, in the `dev_deps` iterable
  section. The devicetree writes these for the devices a node refers to; see
  {ref}`devicetree-init-order`.

`CONFIG_DEVICE_INIT_REVERSE` walks the table backwards, to check that nothing
relies on its position.

`pbl_device_init()` is idempotent, so a device is initialized exactly once
however many others depend on it. A failed init marks the device
`PBL_DEVICE_FAILED`, and its dependents fail with `-ENODEV` without running
their own init. An init returning `-ENODEV` means the device is not present,
which is not logged as an error. A dependency cycle asserts.

Runtime APIs never initialize on demand: bring-up is deterministic and
happens in one place. What they do with a device that is not ready depends
on what the device is to its callers:

- a device other devices are built on asserts it, as its consumers only
  come up once it is: a call on it before then is a bug;
- a device a caller may find absent or failed returns `-ENODEV`, which the
  caller handles;
- a device used before the devices come up, or while the memory holding
  the device model may be unreadable, does not check: its users already
  know it is up, and the check could not run.

A driver may still leave to the first use of its device the work that only
matters once something uses it, such as turning on a clock that would
otherwise run with nothing behind it: the device is ready after its init,
and what is left stays inside the driver.

Hardware needed before the kernel runs, such as clocks
(`pbl_soc_early_init()`), stays outside the model.

## Power

A supply is a `struct pbl_regulator` device the device using it points at,
as the sensor's `supply`.
Consumers switch it with `pbl_regulator_enable()` and
`pbl_regulator_disable()`, which keep a use count, so a rail shared by
several devices stays on while any of them needs it. A rail marked always-on
is switched on at init and stays on; any other rail starts off. Enabling a
rail waits its enable time, so the rail is up when the call returns; the time
what it feeds needs after that is up to the consumer's driver.

(device-model-mfd)=

## Multi-function devices

A chip with several unrelated functions, such as a PMIC with a charger, GPIOs
and regulators, is one core device plus one device per function (cell): the
core lives in `drivers/mfd` and each cell in the class it implements.

| Part | Driver | Device |
| --- | --- | --- |
| Core | `drivers/mfd/<chip>.c` | the chip, on its bus |
| Charger | `drivers/charger/<chip>.c` | a `struct pbl_charger` |
| GPIOs | `drivers/gpio/<chip>.c` | a `struct pbl_gpio_port` |
| Rails | `drivers/regulator/<chip>.c` | a `struct pbl_regulator` per rail |

The core sits on its bus and owns the register access under a lock, so a
read-modify-write in one cell cannot interleave with another. Each cell has
the core as its parent and reaches it through it. The core's init ends with
`pbl_device_init_children()`, so a device that depends on the chip can rely
on its cells being up.

In the devicetree, the chip is a single node: each driver has its own spec for
the same compatible, so the node gives one device per enabled driver, and one
regulator per child of its `regulators` node. A consumer refers to a rail or
a pin of the chip as to any other, with `vdd-supply = <&ldo1>` or
`<&pmic 1 GPIO_ACTIVE_HIGH>`.

## Reporting to consumers

Drivers never call into the services that consume their data. A class whose
devices produce data on their own, such as samples or events signalled by an
interrupt, lets a consumer register a listener with
`pbl_<class>_set_listener()`, and its drivers report through the class with
`pbl_<class>_report_<what>()`. The class hands the report to the listener, so
a driver does not know who consumes its data, and a consumer does not know
which driver produced it.

## Adding a class

This walks through adding a class for a kind of device the firmware does not
know yet: a humidity sensor, read on demand, with one role, `humidity`.

```text
include/pbl/drivers/humidity.h      class interface
drivers/humidity/humidity.c         class object, device init, API
drivers/humidity/humidity.class.yaml
drivers/humidity/Kconfig
drivers/humidity/CMakeLists.txt
drivers/humidity/shell.c            optional
```

**The interface**, `include/pbl/drivers/humidity.h`, has the ops drivers
implement, the class struct, the macros drivers build their
instances with, the role and the API:

```c
struct pbl_humidity;

struct pbl_humidity_ops {
  /** Optional. Bring the hardware up. */
  int (*init)(const struct pbl_humidity *hum);
  /** Read the relative humidity, in thousandths of a percent. */
  int (*read)(const struct pbl_humidity *hum, uint32_t *milli_percent);
};

struct pbl_humidity {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_humidity_ops *ops;
};

extern const struct pbl_device_class pbl_humidity_class;
int pbl_humidity_dev_init(const struct pbl_device *dev);

/** Humidity sensor of the board. */
extern const struct pbl_humidity *const HUMIDITY;

#define PBL_HUMIDITY_STATE_DEFINE(sym) PBL_DEVICE_STATE_DEFINE(sym)

#define PBL_HUMIDITY_INIT(sym, _name, _parent, _ops)                                               \
  {                                                                                                \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_humidity_dev_init, _parent, &pbl_humidity_class), \
    .ops = (_ops),                                                                                 \
  }

int pbl_humidity_read(const struct pbl_humidity *hum, uint32_t *milli_percent);
```

**The class**, `drivers/humidity/humidity.c`, defines the class object, brings
devices up through their driver, and implements the API, refusing devices
that are not ready:

```c
PBL_DEVICE_CLASS_DEFINE(pbl_humidity_class, "humidity");

int pbl_humidity_dev_init(const struct pbl_device *dev) {
  const struct pbl_humidity *hum = PBL_CONTAINER_OF(dev, const struct pbl_humidity, dev);

  if (hum->ops->init == nullptr) {
    return 0;
  }

  return hum->ops->init(hum);
}

int pbl_humidity_read(const struct pbl_humidity *hum, uint32_t *milli_percent) {
  if (!pbl_device_is_ready(&hum->dev)) {
    return -ENODEV;
  }

  return hum->ops->read(hum, milli_percent);
}
```

**The class file**, `drivers/humidity/humidity.class.yaml`, tells the build
its type, its header and its role, needed whenever the class is enabled; see
{ref}`devicetree-classes`:

```yaml
class: humidity
type: struct pbl_humidity
include: pbl/drivers/humidity.h
roles:
  humidity: HUMIDITY
```

**Kconfig and the build**: `drivers/humidity/Kconfig` is a `menuconfig`
enclosing the drivers of the class, sourced from `drivers/Kconfig`, and the
directory is added to `drivers/CMakeLists.txt`:

```kconfig
menuconfig HUMIDITY
    bool "Humidity Sensor"
    help
      Humidity Sensor Drivers

if HUMIDITY

config HUMIDITY_SHELL
    bool "Shell commands"
    default DRIVER_SHELL
    depends on SHELL
    help
      The humidity shell command, to read the humidity sensor.

endif # HUMIDITY
```

```cmake
# drivers/CMakeLists.txt
pbl_add_subdirectory_ifdef(CONFIG_HUMIDITY humidity)

# drivers/humidity/CMakeLists.txt
pbl_library()
pbl_library_sources(humidity.c)
pbl_library_sources_ifdef(CONFIG_HUMIDITY_SHELL shell.c)
```

A class that logs adds its log module to the same Kconfig file,
`module = DRIVER_HUMIDITY` followed by
`source "subsys/logging/Kconfig.template.log_level"`, defines it in
`humidity.c` with `PBL_LOG_MODULE_DEFINE(driver_humidity,
CONFIG_DRIVER_HUMIDITY_LOG_LEVEL)`, and its drivers declare it with
`PBL_LOG_MODULE_DECLARE()`.

**The shell command**, `drivers/humidity/shell.c`, one per class:

```c
static int prv_cmd_humidity(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t milli_percent;
  int ret;

  ret = pbl_humidity_read(HUMIDITY, &milli_percent);
  if (ret != 0) {
    pbl_shell_error(sh, "read failed (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "%" PRIu32, milli_percent);

  return 0;
}

PBL_SHELL_CMD_REGISTER(humidity, nullptr, "Read the humidity sensor", prv_cmd_humidity);
```

Finally, the role gets a row in the roles table of
[](../development/build_system/devicetree.md), and a simulator, if the class
has one, a row in [](simulators.md).

## Adding a driver

This adds a driver for an imaginary I2C humidity sensor, `vendor,humidity`,
to the class above, and puts the sensor on a board.

```text
include/pbl/drivers/humidity/vendor.h      driver struct, instance macro
drivers/humidity/vendor.c                  ops
drivers/humidity/vendor.dtmap.yaml         devicetree spec
dts/bindings/iio/humidity/vendor,humidity.yaml
drivers/humidity/Kconfig.vendor            driver symbol
drivers/humidity/CMakeLists.txt            driver source
```

**The header**, `include/pbl/drivers/humidity/vendor.h`, has the driver
struct, with the class struct first, and the instance macro, taking what the
hardware description gives:

```c
struct pbl_humidity_vendor {
  /** Humidity sensor, child of the I2C bus. */
  struct pbl_humidity humidity;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
  /** Supply. */
  const struct pbl_regulator *supply;
};

extern const struct pbl_humidity_ops pbl_humidity_vendor_ops;

#define PBL_HUMIDITY_VENDOR_DEFINE(sym, _bus, _addr, _supply)                            \
  PBL_HUMIDITY_STATE_DEFINE(sym);                                                        \
  PBL_DEVICE_INSTANCE(sym, struct pbl_humidity_vendor, struct pbl_humidity, humidity) {  \
    .humidity = PBL_HUMIDITY_INIT(sym, "humidity-vendor", &(_bus)->dev,                  \
                                  &pbl_humidity_vendor_ops),                             \
    .i2c = PBL_I2C_DEV(_bus, _addr), .supply = (_supply),                                \
  }
```

**The driver**, `drivers/humidity/vendor.c`, implements the ops and ends by
including the instances the devicetree gives:

```c
#define REG_HUMIDITY 0x01
#define MILLI_PERCENT_PER_LSB 2

static int prv_init(const struct pbl_humidity *hum) {
  const struct pbl_humidity_vendor *vendor =
      PBL_CONTAINER_OF(hum, const struct pbl_humidity_vendor, humidity);

  return pbl_regulator_enable(vendor->supply);
}

static int prv_read(const struct pbl_humidity *hum, uint32_t *milli_percent) {
  const struct pbl_humidity_vendor *vendor =
      PBL_CONTAINER_OF(hum, const struct pbl_humidity_vendor, humidity);
  uint8_t raw[2];
  int ret;

  ret = pbl_i2c_burst_read(&vendor->i2c, REG_HUMIDITY, raw, sizeof(raw));
  if (ret != 0) {
    return ret;
  }

  *milli_percent = ((uint32_t)raw[0] << 8 | raw[1]) * MILLI_PERCENT_PER_LSB;

  return 0;
}

const struct pbl_humidity_ops pbl_humidity_vendor_ops = {
  .init = prv_init,
  .read = prv_read,
};

#include PBL_DT_INSTANCES(vendor_humidity)
```

**The spec**, `drivers/humidity/vendor.dtmap.yaml`, maps the node to the
arguments of the instance macro; see {ref}`devicetree-specs`. A compatible
with no upstream binding gets one in `dts/bindings/`:

```yaml
compatible: vendor,humidity
config: HUMIDITY_VENDOR
class: humidity
define: PBL_HUMIDITY_VENDOR_DEFINE
args:
  - parent: i2c
  - reg: 0
  - supply: vdd
```

**Kconfig and the build**: the driver is on by default when the tree has a
node it binds to, can still be enabled by hand for a board without a tree,
and selects the bus it needs. Its options go in
`drivers/humidity/Kconfig.vendor`, sourced from the class `Kconfig` with
`rsource "Kconfig.vendor"`:

```kconfig
config HUMIDITY_VENDOR
    bool "Vendor humidity sensor"
    default DT_HAS_VENDOR_HUMIDITY_ENABLED
    select I2C
    help
      Support for the Vendor humidity sensor.
```

```cmake
pbl_library_sources_ifdef(CONFIG_HUMIDITY_VENDOR vendor.c)
```

**The board** describes the sensor under its bus and gives it the role, in
its tree and its `roles.yml`:

```dts
&i2c1 {
	status = "okay";

	hum0: humidity-sensor@40 {
		compatible = "vendor,humidity";
		reg = <0x40>;
		vdd-supply = <&ldo1>;
	};
};
```

```yaml
humidity: hum0
```

A board without a tree writes the same instance in C, and gives the role its
symbol in `roles.yml`, with the header declaring it:

```c
PBL_HUMIDITY_VENDOR_DEFINE(board_hum, &board_i2c1, 0x40, &board_ldo1);
PBL_DEVICE_DEPENDS(board_hum, &board_hum.dev, &board_ldo1.dev);
```

**Consumers** enable `HUMIDITY` and read the sensor through its role,
without knowing which driver is behind it:

```c
uint32_t milli_percent;

if (pbl_humidity_read(HUMIDITY, &milli_percent) == 0) {
  ...
}
```
