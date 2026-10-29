# Programmers

A programmer connects a watch's mainboard to a host, to flash it, reach its
serial console and, where supported, debug it. The board pages explain how
to connect one; the pages below describe each programmer.

| Programmer | Boards | Interfaces | Runners |
| --- | --- | --- | --- |
| {doc}`core_b2b_v2` | asterix | SWD, debug UART | `openocd`, `nrfutil`, `probe-rs` |
| {doc}`pblprog_sifli` | obelix, getafix | Debug UART | `sftool`, `probe-rs` |

```{toctree}
:hidden:

core_b2b_v2.md
pblprog_sifli.md
```
