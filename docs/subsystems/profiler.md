# Profiler

`subsys/profiler/` measures where CPU time goes, in cycles. Its public API is
`include/pbl/profiler/profiler.h`, enabled with `CONFIG_PROFILER`.

## Nodes

A node accumulates the cycles spent between its start and stop calls, and
how many times it was stopped. Nodes are defined next to the code they
measure and collected at link time in the `.pbl_profiler_nodes` linker
section, sorted by name:

```c
PBL_PROFILER_NODE_DEFINE(compositor);

void compositor_render_app(void) {
  PBL_PROFILER_NODE_START(compositor);
  ...
  PBL_PROFILER_NODE_STOP(compositor);
}
```

Another file reaches the node with `PBL_PROFILER_NODE_DECLARE()`. Without
`CONFIG_PROFILER` the node macros compile to nothing, so instrumented code
needs no conditionals.

Starting and stopping a node is a syscall: code running unprivileged, such
as the application library inside an app, may time nodes too. Calls from
unprivileged code with anything but a defined node are ignored.

## Runs

`pbl_profiler_reset()` clears the run and every node, `pbl_profiler_start()`
and `pbl_profiler_stop()` time the run, and `pbl_profiler_print_stats()`
stops it and logs each node that ran, most cycles first, with its share of
the run.

From the shell, `profiler start` resets and starts a run, and
`profiler stop` stops it and prints the stats.
