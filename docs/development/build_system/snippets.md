# Snippets

A snippet is a preset a build adds on top of the board and the application:
Kconfig settings and devicetree overlays, for every board or for some. It
keeps what several applications need from a board, such as a partition for
a file system, in one place rather than in each application, and out of the
board, which defines only what any image needs (see
{ref}`devicetree-partitions`).

```shell
pbl configure --board qemu_emery --snippet filesystem samples/subsys/fs/files
```

`--snippet` (`-S`) can be repeated. With CMake directly, `-DSNIPPET=a;b`
selects them. A sample names the snippets it is built with in the
`snippets` list of its `sample.yml`.

## Layout

A snippet is a directory under `snippets/`, in the application or in
PebbleOS; the application's wins when both have one of the same name.

| File | Applies to |
| --- | --- |
| `snippet.conf` | Every board |
| `snippet.overlay` | Every board with a devicetree |
| `boards/<board>.conf` | One board |
| `boards/<board>.overlay` | One board |

They apply after the application's own files, in that order. Boards that
share a preset, such as the QEMU machines or the watches of a SoC, include
the same file from their overlays:

```dts
#include "../qemu.dtsi"
```

`pbl samples` skips a sample on a board that none of its snippets has
anything for.

