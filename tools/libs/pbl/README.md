# pbl-tool

Internal developer commands for interacting with Pebble watches, built on
`pebble-tool` and `libpebble2` (e.g. `install_firmware`, `install_lang`,
`coredump`, `flash_logs`). Installs a `pbl` entry point:

    pip install ./tools/libs/pbl

Not to be confused with the repo-root `./pbl` build wrapper.

To add a command, create a file in `pbl/commands/` with a class inheriting
from `BaseCommand` (or `PebbleCommand` if it connects to a watch): the
docstring becomes the help text, the `command` field names the command, and
`__call__` runs it. Examples can be found
[in pebble-tool](https://github.com/pebble/pebble-tool/tree/master/pebble_tool/commands).
