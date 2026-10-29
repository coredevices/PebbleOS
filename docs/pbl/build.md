# `pbl build`

Build the configured firmware, or the named CMake targets.

```shell
pbl build
pbl build sdk
```

The build directory must have been set up with {doc}`configure` first. The
outputs land in it, named after the variant (`normal` or `prf`); see
{doc}`../development/build_system/index` for how the build is put together.

`rom_report`, `ram_report` and `footprint` report where the firmware's ROM
and RAM go; see {doc}`../development/debugging/index`.

## Options

```{pbl-command} build
```
