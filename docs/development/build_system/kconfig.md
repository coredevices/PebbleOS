# Kconfig

Everything that can be left out of the firmware is a Kconfig symbol: SoCs,
drivers, subsystems, services, debug aids and their parameters. The
configuration is resolved with [Kconfiglib](https://github.com/ulfalizer/Kconfiglib)
when the build directory is configured, and the result drives both the C
sources and the CMake build.

## Configuration tree

The root `Kconfig` sources the rest of the tree:

- `soc/*/Kconfig.defconfig` and `subsys/bluetooth/Kconfig.defconfig`
  first, so their defaults win over the ones given with the definitions;
- `soc/Kconfig`, with the SoC and CPU symbols;
- the board's files, only for the selected board and revision:
  `boards/<board>/Kconfig`, `Kconfig.<revision>`, `Kconfig.defconfig` and
  `Kconfig.defconfig.<revision>`;
- `lib/c`, `kernel`, `subsys`, `drivers`, `fw` and `third_party`, each with
  a `Kconfig` that sources its own subdirectories.

A symbol is defined next to the code it enables: a driver class's
`Kconfig` lives in `drivers/<class>/`, a subsystem's in `subsys/<name>/`.
{doc}`../../reference/kconfig` lists every option, with its dependencies,
defaults and location.

## Configuration fragments

A build's configuration is resolved from these fragments, in this order:

| Fragment | Purpose |
| --- | --- |
| `boards/<board>/defconfig` | What the board has: SoC, platform, drivers and their parameters |
| `boards/<board>/defconfig.<revision>` | What a revision changes |
| `fw/prj.conf` | Services and features every firmware enables |
| `fw/prj_<variant>.conf` | What the `normal` or `prf` variant adds or removes |
| `<build>/menuconfig.conf` | Changes made with {doc}`../../pbl/menuconfig` |
| `-DCONFIG_<SYMBOL>=<value>` | Overrides given when configuring |

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
  `if` block. A driver that only exists on one SoC defaults to `y` there
  (`default y if SOC_QEMU`); a part on the board is enabled by the board's
  defconfig.

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
