# `pbl image_recovery`

Write a recovery firmware (PRF) image over the serial console. By default
it writes the build's own firmware, so configure a `prf` build with
{doc}`configure` first, or pass `--file`.

```shell
pbl -b build-prf image_recovery --tty /dev/ttyACM0
```

## Options

```{pbl-command} image_recovery
```
