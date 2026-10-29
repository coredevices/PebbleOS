# Extending `pbl`

## Structure

The package lives in `tools/libs/pbl-cli/`, and is a small core plus one
module per command:

```
pbl/app/main.py    finds the workspace, assembles the commands, dispatches
pbl/command.py     PblCommand, the base class every command derives from
pbl/workspace.py   the checkout: pbl.yml, the layout, the extensions
pbl/build.py       a build directory and what can be read back from it
pbl/runners/       the binary runners (openocd, sftool, qemu, ...)
pbl/commands/      the commands themselves
```

### Adding a command

Drop a module in `pbl/commands/` with a `PblCommand` subclass. Commands are
discovered by scanning the package, so there is no registry to update;
classes whose name starts with an underscore are treated as shared bases
rather than commands.

```python
from pbl.command import PblCommand


class Size(PblCommand):
    group = "build"

    def __init__(self):
        super().__init__("size", "Report the firmware's size")

    def do_add_parser(self, parser_adder):
        parser = self.add_subparser(parser_adder)
        parser.add_argument("--sections", action="store_true")
        return parser

    def do_run(self, args, unknown):
        build = self.build_dir()
        return self.run_cmd(["arm-none-eabi-size", build.elf])
```

`self.build_dir()` returns the configured build, failing with a useful
message when there is none; `self.run_cmd()` and `self.check_cmd()` run
things from the workspace root and honor `--dry-run`; raising
`CommandError` is how a command fails. `group` picks the section of the
top-level help the command is listed under.

### Feeds

`pbl emulator feed <feed>` writes simulated phone data into the emulator. Each feed
is a module in `pbl/feeds/` with a `Feed` subclass, discovered the same
way commands are: it names its subcommand, adds its own options in
`add_arguments()` and writes its data in `run()` through the `Watch` it is
given, which wraps the blob DB client, sends and receives raw protocol
packets, and turns into a printer under `--dry-run`. A feed that has to
answer the watch registers handlers with `watch.on()` and keeps `run()`
going until interrupted.

```python
from pbl.feeds import BlobDb, Feed


class Steps(Feed):
    name = "steps"
    help = "A day of step counts"

    def add_arguments(self, parser):
        parser.add_argument("--steps", type=int, default=8000)

    def run(self, args, watch, inf):
        watch.blobdb_insert(BlobDb.HEALTH, key, value)
        inf(f"pushed {args.steps} steps")
```

### Extension commands

A command that should not live in the package is declared in `pbl.yml`
instead. It is only imported when it is the command being run, and is
listed under "extension commands" rather than in one of the groups:

```yaml
commands:
  - file: tools/pbl_commands/size.py
    commands:
      - name: size
        class: Size
        help: Report the firmware's size
```

## Adding a runner

A board lists the {ref}`runners <pbl-runners>` it supports in its manifest,
first one first:

```yaml
runners:
  - openocd
  - nrfutil
```

`pbl run`, `erase`, `debug` and `debugserver` dispatch to the first of
them that supports the command. Each runner contributes its own options (e.g.
`--tty` for sftool), so they show up on all of those commands. New runners
are modules in `pbl/runners/`, registered in that package's `RUNNERS`
table, and declare the operations they support in `capabilities()`.

A runner with a GDB server implements `debugserver` by running it in the
foreground, and `debug` by handing it to `run_server_and_gdb()`, which
starts it, attaches GDB once it listens and stops it when GDB quits.
