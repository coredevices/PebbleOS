# Devicetree

A board's hardware is described by a devicetree: what is on it, where, and how
it is wired. The tree follows the Linux devicetree bindings and conventions, so
the same description could boot Linux on that SoC. It only exists at build time:
nothing of it is stored or parsed by the firmware. Each device it describes
becomes a static, read-only instance, as if it had been written by hand.

The devicetree decides two things:

- **Which drivers are built.** A driver is enabled by default when the tree has
  an enabled node it binds to, through a `DT_HAS_<compatible>_ENABLED` symbol in
  Kconfig.
- **Which devices exist, and how they are connected.** Each enabled node whose
  driver is enabled becomes a call to that driver's instance macro.

The native boards, such as `native_emery`, run on the host and have no
hardware to describe: their drivers define their devices in C, and their
roles name them; see [](#devicetree-roles). The QEMU boards run on
emulated hardware, which a tree describes like any other: `dts/qemu/pebble.dtsi`
for the machine, a board tree for each.

## Trees

| File | Contents |
| --- | --- |
| `dts/<vendor>/<soc>.dtsi` | The SoC, as `dts/sifli/sf32lb52.dtsi`: CPU, interrupt controller, peripherals, mostly `status = "disabled"` |
| `dts/qemu/pebble.dtsi` | The QEMU Pebble machines, whose peripherals are QEMU devices (`qemu,pebble-*`) |
| `boards/<board>/<board>.dtsi` | What the revisions of a board share |
| `boards/<board>/<board>-pinctrl.dtsi` | The pin control states of the board, on SoCs with a pin controller |
| `boards/<board>/<board>_<revision>.dts` | A revision, e.g. `obelix_pvt.dts`, or `<board>.dts` for boards with one |
| `include/dt-bindings/` | Constants the trees use, such as clock IDs and GPIO flags |
| `dts/bindings/` | Bindings that upstream does not have |

A board tree includes its SoC tree, enables the peripherals it uses, and adds
what is on the board: sensors on a bus, the flash on its controller, the panel,
supplies switched by GPIOs.

```dts
&i2c2 {
	pinctrl-0 = <&i2c2_default>;
	pinctrl-names = "default";
	clock-frequency = <400000>;
	dmas = <&dmac1 5 23 2>;
	dma-names = "rx";
	status = "okay";

	lsm6dso: imu@6a {
		compatible = "st,lsm6dso";
		reg = <0x6a>;
		interrupt-parent = <&gpio1>;
		interrupts = <38 IRQ_TYPE_EDGE_RISING>;
		mount-matrix = "-1", "0", "0",
			       "0", "1", "0",
			       "0", "0", "1";
	};
};
```

## From tree to instances

```{mermaid}
flowchart TD
  dts["boards/&lt;board&gt;/*.dts<br/>dts/*/*.dtsi<br/>add-on and application overlays"]
  dtc["dtmap.py: C preprocessor, dtc"]
  dtb["board.dtb"]
  kdt["Kconfig.dt<br/>DT_HAS_*_ENABLED"]
  kconfig["Kconfig"]
  cfg[".config"]
  dtmap["dtmap.py"]
  specs["*.dtmap.yaml, *.class.yaml, *.regs.yaml"]
  roles["roles.yml"]
  out["devicetree/devices.h<br/>devicetree/partitions.h<br/>devicetree/idle_states.h<br/>devicetree/regs.h<br/>devicetree/instances/*.inc<br/>devicetree/roles.c"]
  drv["drivers"]
  dts --> dtc --> dtb --> kdt --> kconfig --> cfg --> dtmap
  dtb --> dtmap
  specs --> dtmap --> out --> drv
  roles --> dtmap

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class dts,roles,out app
  class dtc,kconfig,dtmap system
  class dtb,kdt,cfg,specs,drv base
```

1. **Compile.** When the build directory is configured,
   `tools/cmake/kconfig.py` has `tools/devicetree/dtmap.py` compile the
   board's tree before Kconfig runs, followed by the overlays of its add-ons
   and of the application, if any; see [](#devicetree-addons) and
   [](#devicetree-overlays). The C preprocessor
   runs first, with `boards/`, `dts/` and `include/` on the include path,
   then `dtc`, into
   `devicetree/board.dtb` in the build. `dtc`'s default checks apply, with
   `unique_unit_address_if_enabled` in place of `unique_unit_address`; a
   warning fails the build.
2. **Kconfig.** In the same step, `dtmap.py` writes `devicetree/Kconfig.dt`,
   with one `DT_HAS_<compatible>_ENABLED` symbol for every compatible a driver
   binds to or a `compatible` argument names, set when the tree has an enabled
   node of that compatible. The root `Kconfig` sources it, and drivers default
   to it.
3. **Generate.** At build time, `dtmap.py` reads the compiled tree, the
   resolved `.config` and the specs of the drivers, and writes, under
   `generated/include/devicetree/`:
   - `instances/<compatible>.inc` for every driver with a spec: one call to
     its instance macro per enabled node it binds to, followed by the devices
     it needs; see [](#devicetree-init-order). The file exists, empty, on
     boards without such a node or without a tree;
   - `devices.h`, declaring `dt_<label>` for every labelled device with a
     driver, with the type of its class, and listing in a comment the enabled
     nodes with no driver in this configuration. A device with several labels
     is defined under the first in alphabetical order, and the others are
     macros for it;
   - `partitions.h`, the flash partitions; see [](#devicetree-partitions);
   - `idle_states.h`, the idle states of the CPU; see [](#devicetree-idle-states);
   - `regs.h`, the registers of the blocks code reaches before there are
     devices; see [](#devicetree-regs);
   - `roles.c`, the device of each role; see [](#devicetree-roles).

`dtc` is needed to configure a board with a tree; see
{doc}`../../getting_started/index`.

## Writing trees

The trees follow Linux, so that what is learned there applies here and the
other way around:

- **Bindings.** A node follows the upstream binding of its compatible when
  there is one, property for property. A binding of our own goes in
  `dts/bindings/`, written the way upstream would accept it.
- **Compatibles.** A compatible names the part, e.g. `jdi,lpm015m135a`, never a
  family or a board. A peripheral that a whole SoC series shares lists the part
  it is on first and the first part that had it last, as
  `"nordic,nrf52840-twim", "nordic,nrf52832-twim"`: the driver names the last
  one in its spec, so one driver serves the series.
- **Hardware only.** A node is a piece of hardware: one node per hardware
  block, with generic node names (`i2c@`, `serial@`, `spi@`, `pwm@`,
  `display-controller@`). Software constructs, configuration and policy are
  not nodes.
- **Functions of a block.** A block that works as one of several peripherals,
  all through the same registers, has a node per function, siblings at the
  block's address, each with its own compatible and label. Only one of them
  may be enabled, which `dtc` checks. A function that a block offers on top
  of its own, as a timer driving a PWM output, is a child node of the block:

  ```dts
  /* The same registers, as an I2C or as a SPI controller: enable one */
  i2c0: i2c@40003000 {
  	compatible = "vendor,soc-i2c";
  	reg = <0x40003000 0x1000>;
  	status = "disabled";
  };

  spi0: spi@40003000 {
  	compatible = "vendor,soc-spi";
  	reg = <0x40003000 0x1000>;
  	status = "disabled";
  };

  /* A timer, and the PWM output it can drive */
  timer1: timer@40010000 {
  	compatible = "vendor,soc-timer";
  	reg = <0x40010000 0x1000>;

  	timer1_pwm: pwm {
  		compatible = "vendor,soc-timer-pwm";
  		#pwm-cells = <3>;
  	};
  };
  ```

- **Abstract IDs.** When a specifier identifies something inside the device
  it points to, such as one of the clocks of a clock controller, one of its
  resets or a request line of a DMA controller, the number is an ID that the
  binding defines in a header under `include/dt-bindings/`, never a register
  bit or the index a driver happens to use. The tree then only depends on the
  binding, as upstream requires, and a driver can number things as it
  likes: the `ids` key of its spec maps each ID to the driver's number when
  the instances are generated, and an ID the driver does not know fails the
  build.

  A header is named after the compatible it serves, and carries the license
  of the bindings, `(GPL-2.0-only OR BSD-2-Clause)`, with `_DT_BINDINGS_*`
  include guards, as upstream's do. The generic ones, as the GPIO flags, are
  our own copies of the upstream values rather than the upstream headers,
  which C code includes too and most of which are GPL-2.0-only.

  ```dts
  #include <dt-bindings/clock/vendor,soc-rcc.h>

  &i2c0 {
  	clocks = <&rcc RCC_CLK_I2C0>;
  };
  ```
- **Pins.** On SoCs with a pin controller, pins are pin control states, with
  the generic pin configuration properties (`input-enable`, `bias-pull-up`,
  `drive-strength`, `output-low`). Peripherals that select their own pins,
  as on the nRF52, take them as GPIO properties named after the signal
  (`scl-gpios`, `sck-gpios`, `tx-gpios`).
- **References.** A device that uses another one points to it, with the
  standard property for what it uses; see [](#devicetree-references).

(devicetree-references)=

### References between devices

Most of what a node describes is how it is wired to other devices: the
regulator that powers it, the clock it runs from, the GPIO that enables it,
the PWM that drives one of its inputs. The node of the device that uses
something (the consumer) points to the node of the device that provides it
(the provider), with a phandle and, when the provider has several of them,
cells saying which one:

```dts
sensor@48 {
	compatible = "vendor,sensor";
	reg = <0x48>;
	vdd-supply = <&ldo1>;                   /* a regulator */
	enable-gpios = <&gpio0 5 GPIO_ACTIVE_HIGH>; /* pin 5 of a GPIO controller */
	interrupt-parent = <&gpio0>;
	interrupts = <6 IRQ_TYPE_EDGE_RISING>;  /* an interrupt, here from a pin */
	clocks = <&rcc RCC_CLK_SENSOR>;         /* one of the clocks of the RCC */
	pwms = <&timer1_pwm 0 16666666 0>;      /* a PWM channel, period, flags */
};
```

The properties are the standard ones of the upstream bindings: `<name>-supply`
for regulators, `<name>-gpios`, `interrupts` or `interrupts-extended`,
`clocks`, `resets`, `dmas`, `pwms` and `mboxes`, with `-names` properties to
pick an entry by name. A rail switched by a GPIO is a `regulator-fixed` node,
so that its consumers refer to it as to any other regulator. Devices along a
data path, such as a display controller and its panel, are connected by a
graph: a `port` in each node, with an `endpoint` pointing at the other.

The generator turns each reference into C in two steps. The consumer's spec
says which reference fills which argument of its instance macro (`supply:
vdd`, `gpio: enable`, `pwm: 0`); see [](#devicetree-specs). The provider's
class, or its spec, gives the C of a reference to one of its devices from
the cells (`provides`); see [](#devicetree-classes). A reference to a node
that is disabled, or whose driver is not enabled, fails the build, and the
provider is initialized before the consumer; see
[](#devicetree-init-order).

(devicetree-connectors)=

### Connectors

A connector is a set of pins that another board plugs into. Its node is a
nexus: references to it give a pin or an interrupt of the connector, and the
node maps them to those of the board, with `gpio-map` and `interrupt-map`, as
in the devicetree specification. What plugs in then refers to the connector
alone, whatever board it is on:

```dts
lcm: connector {
	compatible = "vendor,lcm-connector";
	#gpio-cells = <2>;
	/* connector pin, flags: the pin selects the entry, the flags pass through */
	gpio-map-mask = <0xffffffff 0x0>;
	gpio-map-pass-thru = <0x0 0xffffffff>;
	gpio-map = <0 0 &gpio1 43 0>,  /* reset */
	           <1 0 &gpio1 7 0>;   /* touch reset */
	#address-cells = <0>;
	#interrupt-cells = <2>;
	interrupt-map = <2 IRQ_TYPE_EDGE_FALLING &gpio1 6 IRQ_TYPE_EDGE_FALLING>;
};

touchscreen@38 {
	interrupt-parent = <&lcm>;
	interrupts = <2 IRQ_TYPE_EDGE_FALLING>;
	reset-gpios = <&lcm 1 GPIO_ACTIVE_LOW>;
};
```

Each reference is followed through the maps to the provider: the specifier,
masked by `<name>-map-mask`, selects the entry, and the bits of
`<name>-map-pass-thru` come from the specifier given. This holds for every
kind of reference with `#<name>-cells`. An interrupt map has no
`interrupt-map-pass-thru`, so it lists each flag it accepts, and the connector
takes `#address-cells = <0>`. A specifier no entry maps fails the build.

The buses and controllers wired to the connector get a label of their own
in the board's tree, next to their usual one, as do the pin states they take,
so that what plugs in can refer to them without knowing the board:

```dts
lcm_i2c: &i2c1 {
	pinctrl-0 = <&i2c1_default>;
	pinctrl-names = "default";
};
```

(devicetree-roles)=

### Roles

Which device code uses for a purpose is not in the tree: which UART is the
console, which flash holds the filesystem, which sensor is the accelerometer.
These are roles. Each class lists the roles of its devices, with the Kconfig
symbol that needs them, if any:

```yaml
class: audio
type: struct pbl_audio
include: pbl/drivers/audio.h
roles:
  mic: MIC
  speaker: SPEAKER
```

A role is reached from C as its name in capitals, a pointer declared by the
class header, as `SPEAKER`:

| Role | Class | Needed by |
| --- | --- | --- |
| `console_uart` | `uart` | |
| `qemu_uart` | `uart` | |
| `bt_hci_uart` | `uart` | `NIMBLE_HCI_UART` |
| `bt_controller` | `rproc` | `NIMBLE_HCI_SF32LB` |
| `display` | `display` | `DISPLAY` |
| `backlight` | `backlight` | `BACKLIGHT` |
| `flash` | `flash` | `FLASH` |
| `accel` | `accel` | `ACCEL` |
| `mag` | `mag` | `MAG` |
| `als` | `als` | `ALS` |
| `pressure` | `pressure` | `PRESSURE` |
| `touch` | `touch` | `TOUCH` |
| `vibe` | `vibe` | `VIBE` |
| `mic` | `audio` | `MIC` |
| `speaker` | `audio` | `SPEAKER` |
| `die_temp` | `temp` | `TEMP` |
| `rtc` | `rtc` | `RTC` |
| `battery` | `battery` | |
| `charger` | `charger` | `CHARGER` |

A board gives each role it has a device in `boards/<board>/roles.yml`, and
a revision changes them in `roles.<revision>.yml`. A device is the label of
a node, or a symbol for a device defined in C, with the header declaring it:

```yaml
console_uart: uart2
flash: flash0
accel:
  symbol: SIM_ACCEL
  include: pbl/drivers/imu/sim.h
```

The add-ons of the board add theirs in their `roles.yml`; see
[](#devicetree-addons). An application changes them in its `roles.yml`, then in
`boards/<board>.roles.yml`: a role there replaces the board's, and `null`
removes it. A class object of the node is taken, so a node implementing
several classes, as a display with its backlight, can have several roles.

`dtmap.py` writes `devicetree/roles.c`, defining each role whose symbol is
enabled:

```c
const struct pbl_accel *const ACCEL = &dt_lsm6dso;
```

An enabled symbol whose role has no device, or a device with no driver, fails
the build. A role no symbol needs is left out when its device has no driver.

(devicetree-addons)=

### Add-ons

An add-on is hardware plugged into a connector of the board: a display
module, an expansion board on a header. It lives in
`boards/addons/<addon>/`, and is plugged in when the build is configured:

```shell
pbl configure --board sf32lb57_dpi_hdk --addon <addon> samples/demos/logo
```

`--addon` can be repeated. A sample plugs in those of its `addons` list; see
{doc}`/samples/index`. An add-on has:

- `addon.yml`, with the compatibles of the connectors it needs:

  ```yaml
  connectors:
    - vendor,lcm-connector
  ```

  A board whose tree has no node of each fails the build.
- `<addon>.overlay`, the overlay of its devices, which refers to the
  connector, and to the labels the board gives what is wired to it, only.
- `boards/<board>.overlay`, optionally: what depends on the board too, as
  how the add-on sits on it.
- `roles.yml`, optionally: the roles of its devices, applied after the
  board's.

Its overlays apply after the board's tree, and before the application's.
Each add-on has a page in {doc}`/boards/addons/index`.

(devicetree-overlays)=

### Overlays

An application changes the board's tree with overlays, applied after it and
its add-ons in this order: `app.overlay`, for every board, then
`boards/<board>.overlay`.
An overlay changes nodes by their label, and can add new ones:

```dts
&lsm6dso {
	status = "disabled";
};

&i2c2 {
	status = "okay";
};
```

They take the include path and preprocessor of the board's tree. A board
without a tree has nothing to overlay, which fails the build.

### Validating

`tools/devicetree/validate.py` checks our bindings and the board trees with
dtschema, against the upstream bindings and ours. The upstream bindings and
their headers come from the `third_party/devicetree/devicetree-rebasing`
submodule, pinned to a release, and dtschema is in `requirements.txt`:

```shell
python3 tools/devicetree/validate.py \
    --bindings dts/bindings/<dir>/<vendor>,<part>.yaml \
    --all-trees
```

`--bindings` checks bindings as upstream checks its own: against the
meta-schema, and their examples against the bindings. `--all-trees` compiles
every board tree and checks it, then again with each add-on whose connectors
it has; compiled trees, such as
`build/devicetree/board.dtb`, can be given instead. CI checks the bindings a
pull request adds or changes, and every tree.

## Connecting a driver

A driver takes part through three things: a class file for its class, a spec
next to the driver, and the generated instances it includes. Classes and specs
are found under `drivers/`, `kernel/` and `soc/`, and anywhere in the
application's directory, so an application can bring drivers of its own.

(devicetree-classes)=

### Classes

A class is the software model of a kind of device, unknown to the tree and to
the bindings. `<class>.class.yaml`, next to the class, gives its type, its
header, and how other devices refer to one of its devices:

```yaml
class: gpio
type: struct pbl_gpio_port
include: pbl/drivers/gpio.h
provides:
  # Cells are <pin flags>
  gpio: "PBL_GPIO(&{sym}, {0}, PBL_GPIO_DT_FLAGS({1}))"
```

A `provides` template turns a reference from another node into C: `{sym}` is
the symbol of the referenced device, and `{0}`, `{1}`, ... the cells of the
specifier. Instance files include only class headers, so drivers do not need
to know each other.

The objects of a class hold their `struct pbl_device` as `dev`. A class that
differs says so with `device`: a template such as `"&{sym}"` for a class whose
type is the device itself, or `false` for objects that are not devices, as
pin states.

(devicetree-specs)=

### Specs

`<driver>.dtmap.yaml`, next to the driver, says which compatible the driver
binds to, which class it implements, and where each argument of its instance
macro comes from:

```yaml
compatible: vendor,soc-i2c
config: I2C_SOC
class: i2c
define: PBL_I2C_SOC_DEFINE
args:
  - name: true
  - reg: 0
  - irq: 0
  - clock: 0
  - prop: clock-frequency
    default: 100000
  - pinctrl: default
```

| Key | Meaning |
| --- | --- |
| `compatible` | The compatible the driver binds to. A node gets the drivers of the first compatible of its list that has an enabled one |
| `config` | The driver's Kconfig symbol |
| `class` | The class it implements, or a list for a node giving several class objects |
| `define` | Its instance macro |
| `args` | The macro arguments after the symbol, in order |
| `symbol` | The symbol of each class object, when it is not `dt_<label>`: a template with `{sym}` for nodes with several class objects, or a fixed name for a device the driver still defines itself |
| `provides` | Templates of references to this driver's devices, when they differ from the class's |
| `header` | Driver headers the `provides` templates need, e.g. for the modes of the parts a panel driver supports |
| `ids` | How the abstract IDs of a binding map to the numbers of the driver; see below |
| `pinctrl` | For pin controllers: how pins and their configuration become C |
| `children` | For devices made of named children, such as the regulators of a PMIC |
| `drives` | Compatibles of the child a node must have for the driver to bind to it, for a controller with a driver per kind of memory it can drive |

Several drivers may bind to one compatible, each with its own spec, class and
instance file `<compatible>_<class>.inc`, as the clock and the reset drivers
of a clock controller that also resets. The instance file of a spec with
several compatibles is named after the first.

An argument takes its value from one place:

| Kind | Value |
| --- | --- |
| `name: true` | The device name, from the label or the node name |
| `reg: <index or name>` | Address of a `reg` entry, translated through the `ranges` above |
| `size: <index or name>` | Size of a `reg` entry |
| `memory: <property>` | The RAM the region this phandle property points to is in, absent when it is no device; see [](#devicetree-reserved-memory) |
| `irq: <index or name>` | An interrupt, from `interrupts` or `interrupts-extended` |
| `clock`, `reset`, `mbox`, `pwm`, `power-domain` | A specifier of `clocks`, `resets`, `mboxes`, `pwms`, `power-domains`, by index or by name; a `clock` name missing from `clock-names` is absent, for an input a node may lack |
| `assigned-clocks: <macro>` | The macro of each clock of `assigned-clocks`, with its parent from `assigned-clock-parents`, `{}` for none, and its rate in Hz from `assigned-clock-rates`, 0 for none |
| `dma: <name>` | A specifier of `dmas`, by name |
| `gpio: <name>` | `<name>-gpios`, or `gpios` for an empty name |
| `supply: <name>` | The regulator of `<name>-supply` |
| `pinctrl: <state>` | A pin control state, by name |
| `prop`, `string`, `strings` | A property, as a number, a string or a list of strings; with `item`, a `prop` is a table whose rows of `cells` cells are each formatted by `item`, as the OCV points of a battery |
| `flag: <property>` | `true` when a boolean property is present |
| `flags: {<property>: <C>}` | The C of each present boolean property, ORed |
| `parent: <kind>` | What the parent provides, as the bus of a device on it |
| `self: <kind>` | What a node provides, with `remote`, the device at the other end of a port |
| `phandle: <property>` | What the node this phandle property points to provides, as the battery of `monitored-battery` |
| `consumers: <property>` | One more than the highest specifier cell, `index` (0 by default), that the enabled nodes give this one in that property, absent without any: how many channels of a mailbox are used, from `mboxes` |
| `list: [<compatible>, ...]` | One `item` per enabled child of the node reached through children of these compatibles, or of the node itself for `[]`: a template of the child's `reg` cells and name, or a `define` with `args` read from the child, as the keys of `gpio-keys` |
| `id: true` | For `children`, the ID of the child |
| `compatible: true` | The C of the first of the node's compatibles found in `values`, for a model named before the generic compatible the driver binds to, as the part of a flash |

with these options:

| Option | Meaning |
| --- | --- |
| `default` | Value when the node does not have it; without one, the build fails |
| `optional` | Leave the argument out when absent, for the trailing arguments of a variadic macro. A node that has an argument after an absent one fails the build. `list` arguments and `prop` tables are variadic too, so they come last |
| `index` | Which cell of a `prop`, or which entry of a `gpio` list |
| `cells` | The width of the rows of a `prop` table, 1 by default |
| `all` | Every entry of a `gpio` list |
| `of: <property>` | Read from the node this phandle property points to, e.g. `monitored-battery` |
| `child: <unit address or name>` | Read from a child, e.g. the memory on a controller, or from a descendant by a path of names, e.g. `panel/panel-timing` |
| `remote: <port>` | Read from the device connected to a port |
| `up: true` | Read from the parent, for a function of a block, or from the device a child of `children` is part of |
| `wrap`, `field`, `as`, `item` | Wrap the value in a macro, make it a designated initializer, take another kind from a provider, format list items |
| `values` | For `string`, the C of each value the property may take; for `compatible`, the C of each model |

A spec with `ids` maps the IDs a binding defines to the numbers of the driver,
by the prefix of their names in two headers: the binding's, and one of the
driver's where each name has the same suffix:

```yaml
ids:
  clock:
    cell: 0                                 # which cell of the specifier
    header: dt-bindings/clock/vendor,soc-rcc.h
    prefix: RCC_CLK_
    values:
      header: pbl/drivers/clk/soc.h
      prefix: SOC_CLK_
```

What a node needs is checked at build time: a node with a missing property,
a reference to a disabled node or to one whose driver is not enabled, or an
unknown clock ID fails the build with the path of the node.

### Instance macros

An instance macro defines a registered device whose class object is the first
member of the driver's struct, with `PBL_DEVICE_INSTANCE()`: the device is
`sym` with the type of its class, for any code, and `sym##_inst` with the type
of the driver, for the driver. A driver with an interrupt connects it in the
macro, with a priority from Kconfig. The arguments are what the hardware
description gives, never an index into a table in the driver:

```c
#define PBL_I2C_SOC_DEFINE(sym, _name, _base, _irq, _clock, _clock_hz, _pinctrl) \
  PBL_I2C_BUS_STATE_DEFINE(sym);                                                \
  static struct pbl_i2c_soc_state sym##_soc_state;                              \
  PBL_DEVICE_INSTANCE(sym, struct pbl_i2c_soc, struct pbl_i2c_bus, bus){        \
    .bus = PBL_I2C_BUS_INIT(sym, _name, nullptr, &pbl_i2c_soc_ops),             \
    ...                                                                         \
  };                                                                            \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_I2C_SOC_IRQ_PRIO, pbl_i2c_soc_irq_handler, &sym##_inst, 0)
```

The macro stays usable from C, for boards without a tree. The driver includes
its instances at its end, and defaults on from the tree:

```c
#include PBL_DT_INSTANCES(vendor_soc_i2c)
```

```kconfig
config I2C_SOC
    bool "Vendor SoC I2C controller"
    default DT_HAS_VENDOR_SOC_I2C_ENABLED
```

(devicetree-init-order)=

### Initialization order

The devices a node refers to are initialized before its own: the provider of
each clock, reset, DMA channel, GPIO, interrupt, PWM, mailbox and supply, and
the devices reached with `self`, as a panel through a port. The generator
lists them after the instance, and `pbl_device_init()` brings them up first,
so the order of the device table does not matter:

```c
PBL_SENSOR_VENDOR_DEFINE(dt_sensor, ...);
PBL_DEVICE_DEPENDS(dt_sensor, &dt_sensor.dev, &dt_ldo1.dev, &dt_gpio0.dev, &dt_rcc.dev,
                   &dt_timer1_pwm.dev);
```

The parent of a device is left out, as the instance macro names it, and so
are the devices of the same node, of the nodes above it and of the nodes
inside it: a multi-function device brings its functions up from its own init.
Devices that need each other fail the build.

## Using devices

Code reaches a labelled device as `dt_<label>`, declared in
`devicetree/devices.h` with the type of its class:

```c
#include <devicetree/devices.h>

pbl_display_clear(&dt_lcdc0);
```

Unlabelled devices can only be reached through other nodes, such as a sensor
through its bus. Code that needs a kind of device rather than a particular
one uses its role, such as `DISPLAY`; see [](#devicetree-roles).

(devicetree-partitions)=

## Flash partitions

The partitions of a flash are a `fixed-partitions` node under it, as in Linux,
each with a `label`. Code finds a partition by its label, which is the
contract between the tree and the code using it.

A board defines only what any image needs: `code`, where an image runs from
by default, and what has to stay whatever is flashed.

```dts
partitions {
	compatible = "fixed-partitions";
	#address-cells = <1>;
	#size-cells = <1>;

	partition@10000 {
		label = "code";
		reg = <0x10000 0x3f0000>;
	};
};
```

An application lays out the rest in its overlay, deleting `code` when it
runs from elsewhere.

The partitions of the flash serving the `flash` role become
`devicetree/partitions.h`, with `PART_<LABEL>_OFFSET`, `PART_<LABEL>_SIZE` and
`PART_<LABEL>` for each, as the C layouts of `include/pbl/board/` give them
for boards without a tree. `CONFIG_BOARD_PARTITIONS_DT` selects them, and
`pbl` reads the generated file to find a partition. Partitions on other
flashes are only seen by Kconfig.

Kconfig reads the partitions of every flash through preprocessor functions:
`$(dt_has_partition,LABEL)`, `$(dt_partition_offset,LABEL)` and
`$(dt_partition_size,LABEL)`. Code using a partition depends on
`$(has_partition,LABEL)`, which also holds on boards without a tree, whose C
layout has every partition. The image is linked at `CONFIG_FLASH_OFFSET`,
for `CONFIG_FW_FLASH_SIZE` bytes, which default to the `code` partition. An
application that runs from another partition sets them from its label.

(devicetree-regs)=

(devicetree-idle-states)=

## Idle states

The states a CPU idles in beyond the wait for interrupt are `arm,idle-state`
nodes in the `idle-states` node of `/cpus`, as in Linux, with the times the
SoC takes to enter and leave each and the shortest idle it pays off for. The
CPU lists them in `cpu-idle-states`:

```dts
cpus {
	#address-cells = <1>;
	#size-cells = <0>;

	cpu0: cpu@0 {
		compatible = "arm,cortex-m33";
		device_type = "cpu";
		reg = <0>;
		cpu-idle-states = <&deepsleep>;
	};

	idle-states {
		deepsleep: cpu-sleep-0 {
			compatible = "arm,idle-state";
			entry-latency-us = <200>;
			exit-latency-us = <500>;
			min-residency-us = <10000>;
		};
	};
};
```

They become `devicetree/idle_states.h`, with
`PBL_DT_IDLE_STATE_<LABEL>_ENTRY_LATENCY_US`, `_EXIT_LATENCY_US` and
`_MIN_RESIDENCY_US` for each labelled state. The SoC, which knows how to
enter each state, fills its {doc}`idle states <../../kernel/idle>` from
them with `PBL_US_TO_TICKS_CEIL()`. A board overrides the times of a state
when its crystal or its flash take longer to come back.

(devicetree-reserved-memory)=

## Reserved memory

Memory set aside for one device, as in Linux, is a child of
`/reserved-memory` that the device points at with `memory-region`, as the
framebuffer of a display too large for the heap:

```dts
/ {
	reserved-memory {
		framebuffer: framebuffer@60000000 {
			reg = <0x60000000 0x55c00>;
		};
	};
};

&lcdc1 {
	memory-region = <&framebuffer>;
};
```

The driver takes its base and size with `reg` and `size` read `of:
memory-region`, and with `memory: memory-region` the RAM the region is in,
when that is a device, as a PSRAM in the window of its controller, so that it
can keep it powered. A region in a RAM whose driver is not enabled fails the
build. Every region also becomes `devicetree/reserved_memory.h`,
whose `PBL_DT_RESERVED_MEMORY_FOREACH(fn)` calls `fn(base, size)` for each:
the scratch heap leaves those in the external RAM out.

## Registers before devices

Code that runs before any device is up, as the SoC's early init, its reset
path or the kernel timer, takes the registers of the blocks it touches from
the tree too. A `*.regs.yaml` next to it names their labels, and the Kconfig
symbol of that code:

```yaml
config: SOC_SF32LB52
labels: [hpsys_rcc, pmuc]
```

When the symbol is set, `devicetree/regs.h` has `PBL_DT_REG_<LABEL>` for the
first `reg` entry of each, translated through the `ranges` above as for
instances. A label may also name another entry, as `{label: flash0, reg: mem}`,
which gives `PBL_DT_REG_FLASH0_MEM` for the entry of that name in `reg-names`.
The node may be disabled, as its registers
are there all the same. Only the labels listed get a constant: everything else
reaches the tree through its instance.

## Tests

The generator has unit tests, run on the host:

```shell
python3 -m pytest tools/devicetree/tests
```
