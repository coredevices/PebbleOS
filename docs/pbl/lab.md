# `pbl lab`

Show the lab: the watches, power supplies and Bluetooth dongles wired to
this host, whether their ports are there, which session holds them, and
the watch the build runs on.

```shell
pbl lab
pbl lab --json
```

```text
lab: /home/me/.config/pbl/lab.yaml
build: build (obelix@pvt), on watch obelix
watches:
 * obelix: obelix@pvt
      serial: /dev/tty.wchusbserial5B7A1355001, in use by pid 4242 (pbl console, since 2026-11-02T10:12:03+01:00, in /home/me/pebbleos)
   getafix: getafix@dvt2
      serial: /dev/tty.wchusbserial5B7A1354971 (missing)
      supply: ppk2
supplies:
   ppk2: ppk2, auto
```

`--json` prints the same for scripts and agents: per port, `present`
(`false` when the device node is not there, `null` when it cannot tell)
and `locked_by`, the session holding it.

(pbl-lab-file)=

## Lab files

A lab file (YAML) describes the hardware wired to this host, and nothing
else. `pbl` reads it from `--lab`, else `$PBL_LAB`, else
`~/.config/pbl/lab.yaml`.
`tools/libs/pbl-cli/lab.example.yaml` describes the format:

```{literalinclude} ../../tools/libs/pbl-cli/lab.example.yaml
:language: yaml
:lines: 4-
```

Commands that reach a watch take the ports the command line leaves out
from the lab's watch for the build: {doc}`run`, {doc}`erase`,
{doc}`debug` and {doc}`debugserver` (`--tty`, `--probe`), {doc}`console`
(`--tty`, `--baudrate`), {doc}`image_resources` and {doc}`image_recovery`
(`--tty`), and {doc}`itest`, which takes its whole setup from it. The watch
is the one given with `--watch`, else the one on the port given with
`--tty`, else the one running the build's board; when several do, `--watch`
has to pick one. {doc}`power` powers it with its supply. With a lab in
place, a watch is a `pbl run` away:

```shell
pbl run --resources
pbl console
```

(pbl-lab-locks)=

## Locks

While a command uses a port, it holds a lock on it, so that sessions
running in parallel, people or agents, do not use the same watch at once.
Another command that needs the port fails right away, naming who holds it:

```text
error: serial port /dev/tty.wchusbserial5B7A1355001 is in use by pid 4242 (pbl console, since 2026-11-02T10:12:03+01:00, in /home/me/pebbleos)
```

With `--wait`, it waits for the port instead, saying who it waits for, until
it is free or the command is canceled. With `--wait SECONDS`, it waits that
long for each port, and fails the same way if the port stays in use. Nested
commands, such as the flash of an {doc}`itest` session, inherit it.

```shell
pbl run --wait
pbl run --wait 1800
```

A session keeps a watch across its commands by keeping its supply on (see
{doc}`power`), while the serial port is free between them. So `--wait` also
waits while another checkout holds the watch's supply; the supply your own
checkout holds does not count, as sessions are told apart by their
checkouts. Use one checkout per session.

Serial ports are locked by their device path, whether given with `--tty` or
taken from the lab, so a `pbl console` on the debug UART blocks the `sftool`
runner, which needs the same port, but not an `openocd` flash over the
probe. Debug probes are locked per lab watch. An {doc}`itest` session on a
watch holds all of its ports, its supply and its dongles for as long as it
runs; the commands it runs itself go through.

The locks are `flock`s on files in `$PBL_LOCK_DIR` (default: a `pbl-<uid>`
directory in the system's temporary directory). The kernel drops them when
their process exits, so a crashed session leaves none behind.

## Options

```{pbl-command} lab
```
