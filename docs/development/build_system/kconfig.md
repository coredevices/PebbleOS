# Kconfig

Everything that can be left out of the firmware is a Kconfig symbol: SoCs,
drivers, subsystems, services, debug aids and their parameters. The
configuration is resolved with [Kconfiglib](https://github.com/ulfalizer/Kconfiglib)
when the build directory is configured, and the result drives both the C
sources and the CMake build.

## Configuration tree

The root of the tree is the application's `Kconfig`, `pebble/Kconfig` for the
firmware, or the top-level `Kconfig` for an application without one. Both
source `Kconfig.pebbleos`, which sources the rest of the tree:

- `soc/*/Kconfig.defconfig` and `subsys/bluetooth/Kconfig.defconfig`
  first, so their defaults win over the ones given with the definitions;
- `soc/Kconfig`, with the SoC and CPU symbols;
- the board's files, only for the selected board and revision:
  `boards/<board>/Kconfig`, `Kconfig.<revision>`, `Kconfig.defconfig` and
  `Kconfig.defconfig.<revision>`;
- `lib/c`, `kernel`, `subsys`, `drivers` and `third_party`, each with
  a `Kconfig` that sources its own subdirectories.

A symbol is defined next to the code it enables: a driver class's
`Kconfig` lives in `drivers/<class>/`, a subsystem's in `subsys/<name>/`.
{doc}`../../reference/kconfig` lists every option, with its dependencies,
defaults and location.

## Configuration fragments

A build's configuration is resolved from these fragments, in this order:

| Fragment | Purpose |
| --- | --- |
| `boards/<board>/defconfig` | What the board is: SoC, platform, boot chain and console |
| `boards/<board>/defconfig.<revision>` | What a revision changes |
| `boards/<board>/defconfig.<board variant>` | What a board variant (`native_emery/wasm`) changes |
| `pebble/prj.conf` | Driver classes, subsystems, services and features every firmware enables |
| `pebble/prj_<variant>.conf` | What the `normal` or `prf` variant adds or removes |
| `pebble/boards/<board>.conf` | What the firmware enables or tunes on one board |
| `pebble/boards/<board>_<variant>.conf` | The same, for one variant only |
| `<build>/menuconfig.conf` | Changes made with {doc}`../../pbl/menuconfig` |
| `-DCONFIG_<SYMBOL>=<value>` | Overrides given when configuring |

The `pebble/` fragments are the firmware's. Another application brings the same
files from its own directory instead, see {doc}`applications`.

Overrides are passed to {doc}`../../pbl/configure` or to CMake:

```shell
pbl configure --board obelix@pvt -DCONFIG_RELEASE=y
```

The variant is selected with `--variant` (`-DVARIANT=`), not by setting
`CONFIG_RECOVERY_FW` directly; `prj_prf.conf` sets it, and the build stops
if the two disagree.

The fragments add to each other: a symbol can be assigned in only one of
them, and a later fragment cannot change what an earlier one set. To change
a value for one build, override it with `-D`.

Configuration warnings are errors. The build stops when:

- a fragment or an override names a symbol that does not exist;
- a fragment assigns a symbol an earlier fragment already assigned;
- a value cannot be applied because of the symbol's dependencies; the
  error names the unsatisfied ones.

## Using the configuration

Every symbol reaches all three consumers:

- **C sources**: `autoconf.h` is force-included into every compilation,
  so symbols are plain macros. An enabled `bool` symbol is defined to `1`
  and a disabled one is not defined at all, so test it with
  `#ifdef CONFIG_TOUCH`.
- **CMake**: `config.cmake` sets one variable per enabled symbol, used by
  the `_ifdef` helpers; see [CMake](cmake.md).
- **Linker script**: preprocessed with `autoconf.h`, so memory layout and
  sections can depend on the configuration.

`.config` in the build directory holds the resolved configuration.

## Writing Kconfig

Driver classes
: A class is a `menuconfig` symbol, with one symbol per driver inside its
  `if` block. The application enables the classes it uses; the board's
  defconfig does not. A driver whose devices come from the board's
  {doc}`devicetree` defaults to the tree having one
  (`default DT_HAS_NORDIC_NRF52832_TWIM_ENABLED`). Otherwise, a driver that
  only exists on one SoC defaults to `y` there (`default y if SOC_QEMU`).

What drivers use
: A driver `select`s the classes it uses, such as the bus it sits on
  (`select I2C`) or the supplies it takes (`select REGULATOR`), so an
  application asks for an accelerometer without knowing how the board
  wires it. This is safe because only drivers inside a class the
  application enables select, and the selected classes have no
  dependencies of their own. A driver that is not inside such a class,
  such as a regulator or a GPIO key, does not select: it sits inside the
  class its users select, or the one the application enables.

Capabilities
: What hardware or drivers provide is a hidden symbol (no prompt) that the
  providers `select`, such as `BACKLIGHT_HAS_COLOR` or `CPU_HAS_FPU`. Code
  and other symbols depend on the capability, not on a list of providers.

Optional libraries
: A library that is not always needed is gated behind a symbol, and its
  consumers `select` (or `imply`) it: `BT` selects `MBEDTLS`, and
  `SERVICE_VOICE` selects `SPEEX`.

Board and SoC defaults
: A default that depends on the board or SoC goes in its
  `Kconfig.defconfig`, which is read before the definitions:

  ```kconfig
  config FW_FLASH_SIZE
      default 0xf8000
  ```

Log levels
: A module's log level comes from a template:

  ```kconfig
  module = TASK_WDT
  module-str = Task watchdog
  source "subsys/logging/Kconfig.template.log_level"
  ```

  It defines `CONFIG_TASK_WDT_LOG_LEVEL` and its choice; see
  {doc}`../../subsystems/logging`.
