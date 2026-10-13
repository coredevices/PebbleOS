# Boards

A board is a build target, passed to {doc}`../pbl/configure` with
`--board`. Boards with several hardware revisions take one after `@`, e.g.
`obelix@pvt`.

| Board | Revisions | Device | Platform |
| --- | --- | --- | --- |
| `asterix` | | Pebble 2 Duo | Flint |
| `obelix` | `bb2`, `dvt`, `pvt` | Pebble Time 2 | Emery |
| `getafix` | `dvt`, `dvt2` | Pebble Round 2 | Gabbro |
| `qemu_flint` | | Emulated Pebble 2 Duo | Flint |
| `qemu_emery` | | Emulated Pebble Time 2 | Emery |
| `qemu_gabbro` | | Emulated Pebble Round 2 | Gabbro |
| `native_emery` | | Host program, see {doc}`../development/native` | Emery |

The QEMU boards run in the emulator; see {doc}`../development/qemu`. The
manifests and defconfigs under `boards/` define each target; a listed board
does not guarantee every configuration builds as-is.

```{toctree}
:glob:

**/*
```
