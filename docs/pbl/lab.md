# `pbl lab`

Show the lab: the watches, power supplies and Bluetooth dongles wired to
this host, whether their ports are there, and the watch the build runs
on.

```shell
pbl lab
pbl lab --json
```

```text
lab: /home/me/.config/pbl/lab.yaml
build: build (obelix@pvt), on watch obelix
watches:
 * obelix: obelix@pvt
      serial: /dev/tty.wchusbserial5B7A1355001
   getafix: getafix@dvt2
      serial: /dev/tty.wchusbserial5B7A1354971 (missing)
      supply: ppk2
supplies:
   ppk2: ppk2, auto
```

`--json` prints the same for scripts and agents: per port, `present`
(`false` when the device node is not there, `null` when it cannot tell).

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
has to pick one. With a lab in place, a watch is a `pbl run` away:

```shell
pbl run --resources
pbl console
```

## Options

```{pbl-command} lab
```
