# Profiler: Nodes

This sample times two ways of sorting with
{doc}`profiler <../../../subsystems/profiler>` nodes. Each node, defined with
`PBL_PROFILER_NODE_DEFINE()`, accumulates the CPU cycles spent between its
start and stop, however many times it runs; the sample shuffles the values
before each sort under a node of its own.

`pbl_profiler_print_stats()` logs a line per node, the busiest first, and the
`PBL_PROFILER_NODE_*` macros read a node's totals.

```{pebbleos-sample}
:path: samples/subsys/profiler/nodes
:sources: src/main.c
:output:
```
