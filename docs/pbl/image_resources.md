# `pbl image_resources`

Write the system resources over the serial console of a running firmware.
A development kit needs this the first time it is flashed, on boards whose
runner cannot write resources itself.

```shell
pbl image_resources --tty /dev/ttyACM0
```

The transfer uses sftool on SF32LB52 boards and PULSE flash imaging
elsewhere; `--force-pulse` forces the latter.

## Options

```{pbl-command} image_resources
```
