# `pbl sdk`

Package the SDK of every platform into the `sdk-core` tree the `pebble`
tool installs.

```shell
pbl sdk 4.44
pbl sdk 4.44 --frozen ../sdk-core/pebble --tarball
```

Where `pbl build sdk` packages the SDK of the one platform a build is
configured for, this builds each platform from the board `sdk/boards.yml`
lists for it, in a build directory of its own under `build-sdk/`, and
puts the results together in `build-sdk/sdk-core/`. {doc}`../sdk/building`
describes what it is made of.

The headers and `libpebble.a` are those of the ABI and go into one directory,
`armv7m/`. Every board has to build the same ones.

The frozen platforms get their headers from this checkout. Their
`libpebble.a` and emulator are the ones they were frozen with, which are
not in this repository: `--frozen` names a directory holding them as
`<platform>/lib/libpebble.a` and `<platform>/qemu/`, such as the `pebble/`
directory of an earlier SDK. Without it the tree is complete for the other
platforms only.

## Options

```{pbl-command} sdk
```
