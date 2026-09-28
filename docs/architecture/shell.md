# Shell

`subsys/shell/` is the debug command shell. Its public API is
`include/pbl/shell/shell.h`, and backends implement
`include/pbl/shell/backend.h`. It is modelled on the Zephyr shell: commands
are defined next to the code they drive, collected at link time, and served
by any number of shell instances, one per backend.

## Commands

A command is a `struct pbl_shell_cmd`: a name, a help string, a handler and
optionally a table of subcommands, so commands form a tree (`bt disc start`).

- `PBL_SHELL_CMD_REGISTER()` / `PBL_SHELL_CMD_ARG_REGISTER()` register a root
  command from any file. The entries land in the `.pbl_shell_root_cmds`
  linker section, sorted by name.
- A file-local subcommand table is a plain `struct pbl_shell_cmd` array of
  `PBL_SHELL_CMD()` entries ending with `PBL_SHELL_SUBCMD_SET_END`.
- `PBL_SHELL_SUBCMD_SET_CREATE()` builds a subcommand set that other files
  extend with `PBL_SHELL_SUBCMD_ADD()`, without referencing any symbol of
  the owner. The entries live in `.pbl_shell_subcmds.<set>.<name>` input
  sections, sorted between a start marker and a terminator, so a set is a
  contiguous, NULL-terminated array once linked. This is how, for
  instance, the Bluetooth, console and driver code all add to `bt` or
  `flash`.

Handlers get `argc`/`argv` with `argv[0]` being the matched command name.
`mandatory` (which counts `argv[0]`) and `optional` let the core check the
argument count; `-h`/`--help`, unknown subcommands and commands without a
handler print the help of the matched node. A handler returns 0 or a
negative errno; `-EINPROGRESS` keeps the shell busy until
`pbl_shell_cmd_done()`, for commands that finish from a callback.

Shell code is wrapped in `#ifdef CONFIG_SHELL`, and modules with a group of
commands can gate them further; for example `src/fw/drivers/imu/shell.c`
builds with `CONFIG_IMU_SHELL`.

```c
static int prv_accel_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  ...
  pbl_shell_print(sh, "x=%d y=%d z=%d mg", sample.x, sample.y, sample.z);
  return 0;
}

static const struct pbl_shell_cmd sub_accel[] = {
  PBL_SHELL_CMD(read, NULL, "Read one sample", prv_accel_read),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(accel, sub_accel, "Accelerometer", NULL);
```

## Instances and backends

`PBL_SHELL_DEFINE()` creates an instance bound to a backend. Every instance
has its own line buffer and state, and commands receive the instance they
run on, so their output goes back where the command came from. Commands of
every instance run on KernelBG.

- **Interactive** instances (with a prompt) feed received characters with
  `pbl_shell_input_from_isr()`. The core does line editing, echo, the prompt
  and tab completion of command names on KernelBG.
- **Line** instances (no prompt) submit whole command lines with
  `pbl_shell_execute_line()`, and get a `done` callback when the command
  finishes.

The firmware has two backends, both in `src/fw/console/`:

- `shell_dbgserial.c`: interactive, on the debug serial. Ctrl-C switches
  the serial port from logs to the shell, Ctrl-D back to logs.
- `shell_pulse.c`: line-based, on the PULSE prompt protocol, which is what
  `pbl console` talks to. Each output line is one prompt message.
