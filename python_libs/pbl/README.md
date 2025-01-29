# pbl
Like pebble-tool, but for internal things.

Not to be confused with pebble-tool-dev. This repository is for internal tools
that don't make sense to ever be public; pebble-tool-dev is for public tools
that can't be public yet.

Not sure where to put something? Ask Katharine!

## Installation

    pip install git+ssh://git@github.com/pebble/pbl.git

## Adding commands

Create a new file in pbl/commands. Add a class inheriting from `BaseCommand`
(or `PebbleCommand` if it connects to a pebble). The docstring will be used as
help. Include a `command` field that gives the name of the command. The class's
`__call__` method will be called when you run the command.

Many examples can be found [in pebble-tool](https://github.com/pebble/pebble-tool/tree/master/pebble_tool/commands).
