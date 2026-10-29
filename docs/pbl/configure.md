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

## Variants

`--variant` picks the image to build: `normal`, the main firmware, or
`prf`, the recovery firmware, which applies `fw/prj_prf.conf` on top of
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
Manufacturing-only functionality in the PRF build.

Options for debugging and logging are listed with
{doc}`../development/debugging/index` and {doc}`../subsystems/logging`. To browse
and change every symbol interactively, use {doc}`menuconfig`.

## Options

```{pbl-command} configure
```
