# Shell: Commands

This sample registers {doc}`shell <../../../subsystems/shell>` commands: one
whose argument count the shell checks, one with subcommands, and one that
finishes later, returning `-EINPROGRESS` and calling `pbl_shell_cmd_done()`
from a delayed work item.

To run them without anyone typing, the sample defines a shell of its own: a
line shell, the kind a transport that carries whole commands implements. It
submits each line with `pbl_shell_execute_line()`, prints the output to the
standard output and waits for the `done` callback before the next. The
console's interactive shell has the same commands: type `help` at its prompt.

```{pebbleos-sample}
:path: samples/subsys/shell/commands
:sources: src/main.c
:output:
```
