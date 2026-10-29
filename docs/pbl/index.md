# The `pbl` CLI

`pbl` is the developer command line for this repository: it configures and
builds the firmware, puts the result on a device or an emulator, and hosts
the operational commands around that.

It is a Python package, installed along with the rest of the project's
dependencies (see the {doc}`../getting_started/index`):

```shell
pip install -r requirements.txt
```

After that `pbl` is on the `PATH` whenever the virtual environment is
active. Run it from anywhere inside the checkout:

```shell
pbl configure --board asterix
pbl build
pbl run
```

`pbl --help` lists every command, grouped; `pbl <command> --help` shows one
command's own options.

## Commands

| Build | Device |
| --- | --- |
| {doc}`configure` | {doc}`run` |
| {doc}`build` | {doc}`console` |
| {doc}`bundle` | {doc}`debug` |
| {doc}`menuconfig` | {doc}`debugserver` |
| {doc}`clean` | {doc}`emulator` |
| {doc}`test` | {doc}`image_resources` |
| {doc}`itest` | {doc}`image_recovery` |
| | {doc}`erase` |

## Shared options

Two options are accepted by every command, before or after the command
name:

- `-b, --build-dir DIR` — use a different build directory. It defaults to
  the one `pbl.yml` names (`build/`, or `build-test/` for `pbl test`), and
  `PBL_BUILD_DIR` overrides that in turn.
- `--dry-run` — print the commands that would run instead of running them.

Separate build directories let several boards be built side by side:

```shell
pbl -b build-qemu configure --board qemu_emery
pbl -b build-asterix configure --board asterix
pbl -b build-asterix run
```

## Where its knowledge comes from

`pbl` holds no facts about the checkout of its own:

- The **workspace** is wherever `pbl.yml` is, found by walking up from the
  current directory. That file also names the build and test directories.
- The **build** is read back from its own byproducts: `.config` for every
  `CONFIG_` symbol, `CMakeCache.txt` for the board, the variant and the
  project name the artifacts are named after. Configuring is therefore the
  only way to tell `pbl` about a build, and reconfiguring is enough to
  change its mind.
- The **board** contributes the rest through `boards/<board>/<board>.yml`:
  its revisions, the binary runners it supports, and the SDL decorations
  the emulator can draw around its screen.

(pbl-runners)=

## Runners

Device commands reach their target through a binary runner: a watch, or a
program on the host running the firmware, such as the emulator. A board
lists the runners it supports in its manifest, and for each command the
first one that can handle the build and the command is the default;
`-r/--runner` picks another.

| Runner | Boards | Operations |
| --- | --- | --- |
| `openocd` | asterix | flash (with resources), erase, debug, debugserver |
| `nrfutil` | asterix | flash (with resources), erase |
| `sftool` | obelix, getafix | flash (with resources), erase |
| `phone` | asterix, obelix, getafix | flash (always with resources) |
| `qemu` | qemu_* | run, debug, debugserver |
| `native` | native_* | run |
| `wasm` | native_*/wasm | run |

For the OpenOCD runner, the probe and target configuration lives in the
board's `support/openocd.cfg`, and how to write the resources and erase
ranges, in `support/flash.cfg`.

The `nrfutil` runner needs a J-Link probe. To write the resources it takes
the external flash configuration from the board's
`support/nrfutil_ext_mem.json`.

The `phone` runner needs no programmer: it goes through the Pebble app's
Developer Connection, at the phone address given with `--address`. It needs
libpebble2.

```{toctree}
:hidden:
:caption: Build

configure.md
build.md
bundle.md
menuconfig.md
clean.md
test.md
itest.md
```

```{toctree}
:hidden:
:caption: Device

run.md
console.md
debug.md
debugserver.md
emulator.md
image_resources.md
image_recovery.md
erase.md
```

```{toctree}
:hidden:

extending.md
```
