---
name: firmware-workflow
description: Use when building, flashing, running, testing or debugging PebbleOS firmware, on a watch, in QEMU or on the native simulator; when using lab hardware (pbl run, pbl power, pbl console, pbl itest, busy ports); when setting up a worktree for parallel work; or when deciding where new code goes in the tree.
---

# PebbleOS firmware workflow

Other sessions, people and agents, share this machine and its watches. Work in
your own worktree, and never disturb what another session is running.

Command details live in `docs/pbl/`, one page per command.

## Worktrees

Do not switch branches or build in the main checkout: other sessions use it.

```sh
git fetch upstream main
git worktree add --no-track -b my-topic ../pebbleos-my-topic upstream/main
cd ../pebbleos-my-topic
. "$(git worktree list --porcelain | sed -n '1s/worktree //p')/.venv/bin/activate"
```

- `--no-track` matters: on a branch tracking `upstream`, `git submodule update`
  fails with `transport 'file' not allowed`, as it looks for that remote in the
  submodule. Fix one with `git branch --unset-upstream`, then add `--force`.
- Keep the path short and next to the main checkout, not in a temporary
  directory: QEMU fails with `Path must be less than 104 bytes` when
  `<build dir>/qemu-mon.sock` is too long.
- The venv's `pbl` is the main checkout's. To run your branch's changes to the
  CLI itself: `PYTHONPATH=tools/libs/pbl-cli python -m pbl ...`.
- One build directory per board, so switching does not reconfigure:
  `pbl -b build-obelix configure --board obelix@pvt`, then `pbl -b build-obelix ...`.

### Submodules

Initialize them before the first configure, shallow, and only the ones needed:

```sh
git submodule update --init --depth 1 -- \
  third_party/cmsis_core/CMSIS third_party/mbedtls/mbedtls \
  third_party/moddable/moddable third_party/nanopb/nanopb \
  third_party/nimble/mynewt-nimble \
  third_party/qr_code_generator/QR-Code-generator \
  third_party/resources/iconography third_party/samsa/samsa \
  third_party/speex/speex third_party/tinymt/TinyMT \
  third_party/upng/upng third_party/uzlib/uzlib
```

That builds a QEMU board. Watches also need `third_party/nonfree/pebbleos-nonfree`
and their HAL: `third_party/hal_sifli/SiFli-SDK` (obelix, getafix) or
`third_party/hal_nordic/nrfx` (asterix). Unit tests also need `third_party/fff/fff`.

- A missing submodule does not always say so: besides `... does not exist`, it
  shows up as `No SOURCES given to target` or an `AttributeError` in
  `resource_generator_pdc.py`.
- Configuring without moddable leaves `moddable/build` behind and its clone then
  fails with `already exists and is not an empty directory`. Delete that
  directory, in your worktree, and initialize again.
- Never run `git submodule deinit` or `git config submodule.*`: that config is
  shared by all worktrees, so it changes the main checkout and everyone else's.
- Stage explicit paths, never `git commit -a`: it picks up dirty submodule SHAs.

## Build

```sh
pbl configure --board qemu_emery   # see pbl configure --help for boards
pbl build
pbl test -R animation              # unit tests; select some, the suite is long
```

A fresh build takes minutes of every core: run it in the background, and do not
start one you do not need. Options: `--variant prf`, `-DCONFIG_RELEASE=y`,
`-DCONFIG_MFG=y`, see `docs/pbl/configure.md`. Release builds have no usable
console.

## On a watch

Check what is there instead of guessing ports: `pbl lab --json`. Device commands
take their ports from it, for the watch that runs the build (`--watch` picks one).

Commands fail right away on a port in use. Always pass `--wait SECONDS`: it waits
for the port, and for the watch's supply while another checkout holds it, since
that session is only between commands. A bare `--wait` waits until canceled; give
a limit, so that a stale holder comes back to you rather than hanging.

```sh
pbl power on --voltage 3800 --wait 1800   # skip power if the watch has no supply
pbl run --resources --wait 1800           # flashes; --resources if they changed
(printf 'version\n'; sleep 3) | pbl console --wait 1800 > console.log
pbl power off --wait 1800                 # when done, it blocks itest on that watch
```

Run them in the background if your shell limits foreground commands. Sessions
are told apart by their checkouts, another reason to have your own. Without
`--wait`, `pbl power` acts on a supply another session holds.

- **Busy**: when `--wait` times out with `is in use by`, ask the human. A held
  port may be a person's console, or a stale daemon, and only they can tell. If
  the holder is in your own checkout, it is yours: waiting will not help.
- **Never touch what is not yours**: no killing a holder, no `pkill` by process
  name, no `pbl power off`, `cycle` or `--voltage` on a supply held from another
  checkout. Stop your own console before flashing over the same serial port.
- **No lease between commands**: locks last one command, so another session can
  reflash the watch between two of yours. Check `version` on the console before
  trusting a result, and flash again if it is not your build.
- **Consoles**: drive them from stdin, as above; the console exits when stdin
  closes. Killing `pbl console` can leave `pulse_console.py` holding the port and
  eating output: if you background one, kill the child by its PID too.
- **Sleeping watch**: sftool can only reach the debug UART at random once the
  firmware deep sleeps. With a supply: `pbl power cycle && pbl run`.
- **No supply**: `pbl power` cannot turn the watch on; ask the human to.
- **Current**: an sftool flash leaves obelix about 150 uA high until power is
  removed: `pbl power cycle` before measuring. `pbl power measure --json`, or
  `start`/`stop` around other work; see `docs/pbl/power.md`.

Integration tests hold the watch's ports, supply and dongles while they run, so
power off first:

```sh
pbl power off --wait 1800
pbl itest --wait 1800 --flash-before -k settings
```

See `docs/development/tests/integration.md`.

## In QEMU

Boards: `qemu_flint`, `qemu_emery`, `qemu_gabbro`. It runs real firmware but
simplified hardware, so drivers, power and radio timing need a watch. Full
workflow: `docs/development/emulation/emulator.md`.

```sh
pbl run -- -display none < /dev/null > qemu.log 2>&1 &   # headless
pbl emulator screenshot --output shot.png                # then read the PNG
printf 'sendkey right\n' | nc -U build/qemu-mon.sock     # up, down, left, right
pbl emulator touch 130 130                               # touch boards only
pbl emulator swipe 130 220 130 40
pbl emulator feed weather                                # phone data: docs/pbl/emulator.md
(printf 'help\n'; sleep 3) | pbl console > console.log
printf 'quit\n' | nc -U build/qemu-mon.sock              # stop it
```

- Use `pbl console` for logs: `uart1.log` can stay empty.
- `pbl run` rebuilds the flash image every launch; `--keep-flash-image` keeps
  apps, data and bondings.
- `pbl run` listens on fixed ports (1234, 12344, 12345), so a second QEMU fails
  with `Address already in use`. Check with `lsof -nP -iTCP:12344 -sTCP:LISTEN`,
  and if it is another session's, wait for it. While it runs, `pbl console` and
  `pbl emulator feed` would reach that session's emulator, not yours.
- `pbl itest` boots its own headless emulators, on private copies and ports. It
  can run next to anything, and `-n 4` runs four at once. Prefer it for checks
  that do not need a live session.
- Debug with `pbl debug` (`docs/development/debugging/gdb_tutorial.md`).

## Where things go

| Path | What |
| --- | --- |
| `lib/` | Standalone libraries: no kernel, logging or other system primitive |
| `subsys/<name>/` | Anything using a system primitive (kernel, logging, ...), self-contained OS components; doc in `docs/subsystems/` |
| `kernel/`, `kernel/arch/<arch>/` | Threads, sync, timers, heap, fatal errors; architecture code |
| `drivers/<class>/` | Drivers: the class, one implementation per chip or SoC peripheral (`drivers/uart/sf32lb.c`), its `shell.c` |
| `soc/<vendor>/<series>/` | SoC-specific code that is no driver: init, idle, sleep, reset, register headers in `hardware/` |
| `boards/<board>/` | Board description: `Kconfig*`, `defconfig*`, `<board>.yml` |
| `include/pbl/<name>/` | Public API of the above; private headers stay in the module |
| `fw/` | Product code: `services/`, `applib/` (SDK, see `docs/sdk/exporting.md`), `syscall/`, `apps/`, `comm/` |
| `third_party/<name>/` | A submodule and its glue (`CMakeLists.txt`, `Kconfig`) |
| `tests/` | `libutil/`, `kernel/`, `subsys/`, `drivers/`, `fw/` mirror the tree; `integration/` is pytest |
| `tools/`, `tools/libs/pbl-cli/`, `tools/cmake/` | Host scripts, the `pbl` CLI, build generators |

If code needs a system primitive it is not `lib/`; if it knows one SoC it does
not belong in a class or subsystem. New code goes in the layers above rather
than `fw/`, which only gets product features.

- Optional features are hidden Kconfig capabilities that drivers `select` and
  users `depend on`, not stub files or `#ifdef` fallbacks.
- Moving code means moving it: implementation included, old file and alias
  headers deleted, all users updated.
- Kernel and subsystem APIs know nothing of apps or workers; that policy lives
  in services.
- Naming, C style and commits: `docs/development/conventions.md`.
