# Kernel: Idle states

This sample sleeps and reads how long the CPU spent in its shallowest
{doc}`idle state <../../kernel/idle>` meanwhile. It then blocks that state,
as a driver does while its peripheral cannot take it, and sleeps again: the
CPU keeps running.

Idle states need `CONFIG_CPU_IDLE`, which the sample enables in its
`prj.conf`.

```{pebbleos-sample}
:path: samples/kernel/idle
:sources: src/main.c
:output:
```
