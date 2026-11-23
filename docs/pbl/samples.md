# `pbl samples`

Build the samples under `samples/` for a board and, where the board runs on
the host, run each one and check what it prints against its `sample.yml`.
Samples are built only for the other boards.

```shell
pbl samples
pbl samples samples/kernel
pbl samples --board native_emery samples/kernel/work
pbl samples --list
```

Each sample is built in a directory of its own,
`build-samples/<board>/<sample>/`, or under the directory given with
`--build-dir`. The output of a build is only shown when it fails, and the
console of a sample when a line it should print does not come; `--verbose`
shows it for every sample.

`--integration` checks each sample on the boards its `integration` list
names, so that it is checked on a few boards that cover it instead of on all
of them, and only builds those marked `build_only`. The boards come from the
samples alone: `--board` then keeps the samples that name that board. CI
checks the samples that way:

```shell
pbl samples --integration
pbl samples --integration --board asterix
pbl samples --integration --list
```

Without `--integration`, the samples are built for `--board`, `qemu_emery`
by default.

{doc}`../samples/index` lists the samples and covers writing one.

## Options

```{pbl-command} samples
```
