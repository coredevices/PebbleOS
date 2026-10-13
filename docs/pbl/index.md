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
pbl flash
```

`pbl --help` lists every command, grouped; `pbl <command> --help` shows one
command's own options.

## Commands

| Build | Device | Emulator |
| --- | --- | --- |
| {doc}`configure` | {doc}`flash` | {doc}`qemu` |
| {doc}`build` | {doc}`console` | {doc}`screenshot` |
| {doc}`bundle` | {doc}`debug` | {doc}`touch` |
| {doc}`menuconfig` | {doc}`image_resources` | {doc}`swipe` |
| {doc}`clean` | {doc}`image_recovery` | {doc}`feed` |
| {doc}`test` | {doc}`reset` | |
| {doc}`itest` | {doc}`run` | |
| | {doc}`bork` | |
| | {doc}`openocd` | |

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
pbl -b build-asterix flash
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

Device commands reach a watch through a binary runner. A board lists the ones it supports in its manifest, and the
first one is the default; `-r/--runner` picks another.

| Runner | Boards | Operations |
| --- | --- | --- |
| `openocd` | asterix | flash, run, reset, erase, debug, debug server |
| `nrfutil` | asterix | flash, run, reset, erase |
| `sftool` | obelix, getafix | flash (with resources), erase |

For the OpenOCD runner, the probe and target configuration lives in the
board's `support/openocd.cfg`.

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

flash.md
console.md
debug.md
image_resources.md
image_recovery.md
reset.md
run.md
bork.md
openocd.md
```

```{toctree}
:hidden:
:caption: Emulator

qemu.md
screenshot.md
touch.md
swipe.md
feed.md
```

```{toctree}
:hidden:

extending.md
```
