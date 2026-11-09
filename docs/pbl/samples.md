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

## Options

```{pbl-command} samples
```
