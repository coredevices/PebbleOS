# `pbl configure`

Configure a build directory for a board. `pbl configure` runs CMake into
the build directory (`build/` by default) and remembers the board, the
variant and every Kconfig choice, so later commands need none of them
again.

```shell
pbl configure --board qemu_emery
```

The board names, and the revisions of each, are listed in
{doc}`../boards/index`. A board with revisions takes one after `@`, e.g.
`obelix@pvt`. A board variant, another way to build the same board, comes
after `/`: `native_emery/wasm` builds the native watch as WebAssembly. It
applies `boards/<board>/defconfig.<variant>` and can bring its own
toolchain (`toolchain.<variant>.cmake`).

A board with several cores, listed under `cores:` in `boards/<board>/<board>.yml`,
is built for one of them at a time, named after `/` the same way:
`stm32h747i_disco/m7` and `stm32h747i_disco/m4` are two builds, configured,
built and flashed on their own. Such a board takes no bare name. A board
variant then comes after the core, as `<board>/<core>/<variant>`: the full name
is `BOARD[@REVISION][/CORE][/VARIANT]`. Each core lists its own variants,
in place of the board's `variants:`, a core being a name or a mapping with its
`name` and `variants`:

```yaml
cores:
  - m7
  - name: m4
    variants:
      - <variant>
```

The core and the variant pick their own board files, those of the core
first, then the variant's, then those of the core with the variant
(`<core>_<variant>`), as
{doc}`../development/build_system/kconfig` and
{doc}`../development/build_system/devicetree` list.

## Add-ons

`--addon` plugs an add-on, such as a display module, into a connector of
the board, and can be repeated. They are listed in
{doc}`../addons/index`:

```shell
pbl configure --board sf32lb57_dpi_hdk --addon <addon> samples/demos/logo
```

## Applications

Without more arguments, the build is for the watch firmware, the
application in `pebble/`. To build another application, such as a sample,
give its directory (see
{doc}`../development/build_system/applications`):

```shell
pbl configure --board qemu_emery samples/hello_world
```

## Variants

`--variant` picks the image to build: `normal`, the main firmware, or
`prf`, the recovery firmware, which applies `pebble/prj_prf.conf` on top of
the board defconfig.

```shell
pbl configure --board obelix@pvt --variant prf
```

## Kconfig options

Anything `pbl configure` does not recognize is passed straight to CMake,
so Kconfig symbols are set with `-D`:

```shell
pbl configure --board asterix -DCONFIG_RELEASE=y
```

A few that select the kind of image:

:`-DCONFIG_RELEASE=y`:
A release image: strips debug aids, enables shipping defaults and reduces
battery usage compared to a debug build.

:`-DCONFIG_MFG=y`:
Manufacturing-only functionality in the PRF build, so with `--variant=prf`.

Options for debugging and logging are listed with
{doc}`../development/debugging/index` and {doc}`../subsystems/logging`. To browse
and change every symbol interactively, use {doc}`menuconfig`.

## Options

```{pbl-command} configure
```
