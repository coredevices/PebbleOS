# `pbl power`

Power the watch on, off or through a cycle, or measure the current it
draws, with the supply the lab (see {doc}`lab`) wires to its battery
terminals.

```shell
pbl power on --voltage 3800
pbl run --resources
pbl power measure --seconds 30
pbl power start --name sync
# ... anything else: pbl console, an app on the phone, ...
pbl power stop --save results
pbl power off
```

`on` (and `cycle`) start a background process that holds the supply until
`off`: some supplies, the PPK2 among them, cut the power when their port
closes. Every other `pbl power` command talks to that process, so the
watch stays powered in between, and any other command runs meanwhile.
`status` shows whether it runs, the voltage and any measurement under way.
Its log is next to the locks (see {ref}`pbl-lab-locks`).

The supply is the one of the lab's watch for the build, the one of the
watch given with `--watch`, or the one given with `--supply`. `--voltage`
sets what it supplies; without it the supply keeps its voltage.

`measure` measures for `--seconds` and prints the mean, maximum and 99th
percentile of the current. `start` and `stop` measure everything in
between instead, across other commands. `--json` prints the whole
summary, and `--save DIR` writes it as `<name>.json`, along with
`<name>.csv` (1 ms means). Only supplies that sample the current can
measure. A measurement is kept in memory until it stops: about 24 MB a
minute with a PPK2.

The background process holds the supply's lock, so an {doc}`itest` session
on the watch fails right away, naming it, until `pbl power off`.

`on`, `off`, `cycle` and the rest talk to whichever process holds the
supply, even one from another session. `--wait` (see {ref}`pbl-lab-locks`)
makes them wait for a session in another checkout to finish first.

## Supplies

| Type | Supply | Measures |
| --- | --- | --- |
| `ppk2` | Nordic Power Profiler Kit II, in source-meter mode | yes |

A supply type is a module of `pbl.power` implementing
`pbl.power.Supply`, registered in `pbl.power.TYPES`; lab files,
`pbl power` and the integration tests take any registered type.

## Options

```{pbl-command} power
```
